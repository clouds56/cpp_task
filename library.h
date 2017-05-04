//
// Created by Clouds Flowing on 5/4/17.
//

#ifndef CPP_TASK_LIBRARY_H
#define CPP_TASK_LIBRARY_H

#include <cstdlib>
#include <thread>
#include <future>
#include <iostream>
#include <queue>
#include <functional>
#include <list>
#include <map>
#include "common.h"

namespace cpp_task{

struct utility {
    struct function_task_base {
        virtual void operator()() = 0;
        virtual ~function_task_base() {}
        function_task_base() = default;
        function_task_base(function_task_base&&) = default;
        function_task_base(const function_task_base&) = delete;
        function_task_base& operator=(const function_task_base&) = delete;
    };

    template <typename F>
    struct function_task : function_task_base {
        F functor;
        function_task(F &&functor) noexcept : functor(std::move(functor)) { }
        function_task(function_task<F> &&f) = default;
        void operator()() { functor(); }
    };

    typedef std::unique_ptr<function_task_base> Task_ptr;

    template <typename Functor>
    static Task_ptr make_function_task(Functor &&functor) {
        return Task_ptr(new function_task<Functor>(std::move(functor)));
    }
};

template <typename T> struct job;

struct job_base {
    typedef int priority_type;

    static const priority_type priority_realtime = 0;

    job_base(priority_type priority, utility::Task_ptr &&task) : task(std::move(task)), last(nullptr), priority(priority) { }
    job_base(job_base &&o) : job_base(o.priority, std::move(o.task)) {
        std::cerr << "moving job_base" << std::endl;
    }
    job_base(const job_base &) = delete;

    utility::Task_ptr task;
    job_base *last;
    const priority_type priority;

    template <typename Fn, class... Args, typename R = typename std::result_of<Fn(Args...)>::type>
    static job<R> make(priority_type priority, Fn &&fn, Args... args) { // -> job_base<decltype(fn(args...))>
        std::cerr << "pushing task" << std::endl;
        std::packaged_task<typename std::remove_pointer<typename std::remove_reference<Fn>::type>::type> raw_task(std::forward<Fn>(fn));
        auto fut = raw_task.get_future();
        static_assert(std::is_same<std::future<typename std::result_of<Fn(Args...)>::type>, decltype(fut)>::value, "");
        auto task = utility::make_function_task([args...,raw_task=std::move(raw_task)]() mutable { std::cerr<<"raw_task<void>"<<std::endl;raw_task(args...); });
        return { priority, std::move(task), std::move(fut) };
    }
};

template <typename T>
struct job : job_base {
    typedef T ResultType;

    job(priority_type priority, utility::Task_ptr &&task, std::future<ResultType> &&fut) : job_base(priority, std::move(task)), fut(std::move(fut)) { }
    job(job &&o) : job_base(std::move(o)), fut(std::move(o.fut)) { }
    job(const job &) = delete;
    std::future<ResultType> fut;

    template <typename Fn, typename U>
    static job make(priority_type priority, Fn &&fn, std::future<U> &&arg_fut) { // -> job_base<decltype(fn(args...))>
        std::cerr << "pushing task (then)" << std::endl;
        std::packaged_task<T(U)> raw_task(std::forward<Fn>(fn));
        auto fut = raw_task.get_future();
        static_assert(std::is_same<std::future<T>, decltype(fut)>::value, "");
        auto task = utility::make_function_task([arg_fut=std::move(arg_fut),raw_task=std::move(raw_task)]() mutable { std::cerr<<"raw_task_then"<<std::endl;raw_task(arg_fut.get()); });
        return { priority, std::move(task), std::move(fut) };
    }

    template <typename Fn, typename R = typename std_exp::invoke_result<Fn, T>::type>
    job<R> then(Fn &&f) {
        return then(std::forward<Fn>(f), priority);
    }
    template <typename Fn, typename R = typename std_exp::invoke_result<Fn, T>::type>
    job<R> then(Fn &&f, priority_type priority) {
        auto job = job<R>::make(priority, f, std::move(fut));
        job.last = this;
        return job;
    }
};

class scheduler {
public:
    scheduler() = default;
    virtual utility::Task_ptr pop();
    virtual ~scheduler() { };
};

class task_queue {
    std::thread th;
    std::atomic_bool stopping;
    std::mutex q_mutex;
    std::queue<utility::Task_ptr> q;
    scheduler *s;

    task_queue() : stopping(false) {
        std::cerr << "tasks running..." << std::endl;
        th = std::thread([this]() { this->loop(); });
    }

    ~task_queue() {
        stopping = true;
        if (th.joinable()) {
            th.join();
        }
    }

    void loop() {
        while (!stopping) {
            auto task = pop();
            if (task) {
                (*task)();
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
    }

    void push_(utility::Task_ptr &&task) {
        std::lock_guard<std::mutex> lg(q_mutex);
        q.push(std::move(task));
    }

    utility::Task_ptr pop() {
        auto task = pop_();
        if (task)
            return task;
//        return s->pop();
        return {};
    }

    utility::Task_ptr pop_() {
        std::lock_guard<std::mutex> lg(q_mutex);
        if (!q.empty()) {
            auto task = std::move(q.front());
            q.pop();
            return task;
        }
        return nullptr;
    }

public:
    static task_queue main;

    void push(job_base *job) {
        if (job->last) {
            push(job->last);
        }
        push_(std::move(job->task));
    }

    template <typename R>
    std::future<R> push(job<R> &&job) {
        push_(std::move(job.task));
        return std::move(job.fut);
    }

    template <typename Fn, class... Args>
    std::future<decltype(std::declval<Fn>()(std::declval<Args>()...))> push(Fn &&fn, Args... args) { // -> std::future<decltype(fn(args...))>
        auto job = job<void>::make(job_base::priority_realtime, std::forward<Fn>(fn), args...);
        if (std::this_thread::get_id() == th.get_id()) {
            (*job.task)();
        } else {
            push_(std::move(job.task));
        }
        return std::move(job.fut);
    }

    static std::future<double> test1(int index, std::unique_ptr<double> a, double b) {
        auto f = [index](std::unique_ptr<double> a, double b) {
            std::cerr << index << std::endl;
            *a *= b;
            return std::move(a);
        };
        return main.push(job_base::make(job_base::priority_realtime ,f, std::move(a), b).then([](std::unique_ptr<double> p) {return *p;}));
    }

    static std::future<std::pair<double, double>> test2(double a, double b) {
        auto *f = +[](double a, double b) {
            return std::make_pair(a + b, a - b);
        };
        return main.push(f, a, b);
    }
};

class priority_scheduler {
    template <typename Key, typename Value>
    struct PriorityList {
        typedef int priority_type;
        typedef std::list<std::pair<const Key, Value>> list_type;
        typedef typename list_type::iterator list_iter;
        typedef std::pair<priority_type, list_iter> Iter;
        std::map<priority_type, list_type> q;
        std::map<Key, Iter> idx;

        void push(priority_type priority, const Key& key, Value value) {
            erase_(key);
            q[priority].push_back({key, value});
            size++;
            idx[key] = std::make_pair(priority, std::prev(q[priority].end()));
        }

        bool erase(const Key& key) {
            return erase_(key);
        }

        std_exp::optional<Value> pop() {
            if (!empty_()) {
                auto it = get_first();
                auto value = it.second->second;
                erase_(it.second->first);
                return value;
            }
            return {};
        }

        bool empty() const {
            return empty_();
        }

    private:
        size_t size;

        bool erase_(const Key& key) {
            auto iit = idx.find(key);
            if (iit != idx.end()) {
                q[iit->second.first].erase(iit->second.second);
                size--;
                idx.erase(iit);
                return true;
            }
            return false;
        }

        Iter get_first() const {
            for (auto qit : q) {
                if (!qit->second.empty()) {
                    return { qit->first, qit->second.begin() };
                }
            }
            return {};
        }

        bool empty_() const {
            return size == 0;
        }
    };

    PriorityList<std::string, job_base> pl;
    std::mutex pl_mutex;

//    utility::Task_ptr pop() {
//        std::lock_guard<std::mutex> lg(pl_mutex);
//        auto job_ = pl.pop();
//        if (job_.hasValue()) {
//            const auto& job = *job_;
//            pl.push(job.priority, "", *job.last);
//            return std::move(job.task);
//        }
//        return {};
//    }
};

}
#endif //CPP_TASK_LIBRARY_H
