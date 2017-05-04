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

    template <typename F, typename... Args>
    struct function_task : function_task_base {
        using ArgTp = std::tuple<typename std::decay<Args>::type...>;
        F functor;
        ArgTp args;
        function_task(F &&functor, Args&&... args) noexcept : functor(std::move(functor)), args(std::make_tuple(std_exp::decay_copy(std::forward<Args>(args))...)) { }
        function_task(function_task<F, Args...> &&f) = default;
        void operator()() { std_exp::apply(functor, std::move(args)); }
    };

    typedef std::unique_ptr<function_task_base> Task_ptr;

    template <typename Functor, typename... Args>
    static Task_ptr make_function_task(Functor &&functor, Args&&... args) {
        return Task_ptr(new function_task<Functor, Args...>(std::forward<Functor>(functor), std::forward<Args>(args)...));
    }
};

struct result_base_shared;
struct result_base {
    virtual void get_void() = 0;
    virtual result_base_shared* share() = 0;
};

struct result_base_shared : result_base {
    result_base_shared* share() {
        return this;
    }
};

template <typename T> struct Job;

struct job_base : std::enable_shared_from_this<job_base> {
    typedef int priority_type;

    static const priority_type priority_realtime = 0;

    job_base(priority_type priority, utility::Task_ptr &&task) : task(std::move(task)), next(nullptr), last(nullptr), priority(priority) { }
    job_base(job_base &&o) : job_base(o.priority, std::move(o.task)) {
        std::cerr << "moving job_base" << std::endl;
    }
    job_base(const job_base &) = delete;

    utility::Task_ptr task;
    std::shared_ptr<job_base> next;
    job_base *last;
    const priority_type priority;

    template <typename Fn, class... Args, typename R = typename std::result_of<Fn(Args...)>::type>
    static Job<R> make(priority_type priority, Fn &&fn, Args... args) { // -> job_base<decltype(fn(args...))>
        std::cerr << "pushing task" << std::endl;
        std::packaged_task<R(Args...)> raw_task(std::forward<Fn>(fn));
        auto fut = raw_task.get_future();
        static_assert(std::is_same<std::future<typename std::result_of<Fn(Args...)>::type>, decltype(fut)>::value, "");
        auto task = utility::make_function_task(std::move(raw_task), std::forward<Args>(args)...);
        return { priority, std::move(task), std::move(fut) };
    }

    template <typename Fn, class... Args, typename R = typename std::result_of<Fn(Args...)>::type>
    static std::shared_ptr<Job<R>> make_shared(priority_type priority, Fn &&fn, Args... args) {
        return std::make_shared<Job<R>>(make(priority, std::forward<Fn>(fn), std::forward<Args>(args)...));
    };
};

template <typename T>
struct Job : job_base {
    typedef T ResultType;

    Job(priority_type priority, utility::Task_ptr &&task, std::future<ResultType> &&fut) : job_base(priority, std::move(task)), fut(std::move(fut)) { }
    Job(Job &&o) : job_base(std::move(o)), fut(std::move(o.fut)) { }
    Job(const Job &) = delete;
    std::future<ResultType> fut;

    template <typename Fn, typename U>
    static Job make(priority_type priority, Fn &&fn, std::future<U> &&arg_fut) { // -> job_base<decltype(fn(args...))>
        std::cerr << "pushing task (then)" << std::endl;
        std::packaged_task<T(U)> raw_task(std::forward<Fn>(fn));
        auto fut = raw_task.get_future();
        static_assert(std::is_same<std::future<T>, decltype(fut)>::value, "");
        auto task = utility::make_function_task([arg_fut=std::move(arg_fut),raw_task=std::move(raw_task)]() mutable { std::cerr<<"raw_task_then"<<std::endl;raw_task(arg_fut.get()); });
        return { priority, std::move(task), std::move(fut) };
    }

    template <typename Fn, typename R = typename std_exp::invoke_result<Fn, T>::type>
    std::shared_ptr<Job<R>> then(Fn &&f) {
        return then(priority, std::forward<Fn>(f));
    }
    template <typename Fn, typename R = typename std_exp::invoke_result<Fn, T>::type>
    std::shared_ptr<Job<R>> then(priority_type priority, Fn &&f) {
        auto job = std::make_shared<Job<R>>(std::move(Job<R>::make(priority, f, std::move(fut))));
        next = job;
        job->last = this;
        return job;
    }

    std::pair<std::shared_ptr<job_base>, std::future<ResultType>> commit() {
        job_base *root = this;
        while (root->last) {
            root = root->last;
        }
        return { root->shared_from_this(), std::move(fut) };
    }
};

class scheduler {
public:
    scheduler() = default;
    virtual utility::Task_ptr pop() = 0;
    virtual ~scheduler() { };
};

class task_queue {
    std::thread th;
    std::atomic_bool stopping;
    std::mutex q_mutex;
    std::queue<utility::Task_ptr> q;
    std::shared_ptr<scheduler> s;

    task_queue(std::shared_ptr<scheduler> s = {}) : stopping(false), s(s) {
        std::cerr << "tasks running..." << std::endl;
        th = std::thread([this]() { this->loop(); });
    }

    ~task_queue() {
        std::cerr << "tasks shutting down..." << std::endl;
        stopping = true;
        if (th.joinable()) {
            th.join();
        }
        std::cerr << "tasks exit" << std::endl;
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
        if (s)
            return s->pop();
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

    std::shared_ptr<scheduler> get_scheduler() { return s; }

    template <typename Fn, class... Args>
    std::future<typename std::result_of<Fn(Args...)>::type> push(Fn &&fn, Args&&... args) { // -> std::future<decltype(fn(args...))>
        auto job = job_base::make(job_base::priority_realtime, std::forward<Fn>(fn), std::forward<Args>(args)...);
        if (std::this_thread::get_id() == th.get_id()) {
            (*job.task)();
        } else {
            push_(std::move(job.task));
        }
        return std::move(job.fut);
    }

    void push(job_base *job) {
        if (!job) return;
        if (job->last) {
            push(job->last);
        }
        push_(std::move(job->task));
    }

    template <typename R>
    std::future<R> push(std::shared_ptr<Job<R>> job, bool recursive) {
        if (!job) return {};
        if (recursive && job->last) {
            push(job->last);
        }
        push_(std::move(job->task));
        return std::move(job->fut);
    }
};

class priority_scheduler : public scheduler {
    template <typename Key, typename Value>
    struct PriorityList {
        typedef int priority_type;
        typedef std::list<std::pair<const Key, Value>> list_type;
        typedef typename list_type::iterator list_iter;
        typedef std::pair<priority_type, list_iter> Iter;
        std::map<priority_type, list_type> q;
        std::map<Key, Iter> idx;

        PriorityList() : size(0) { }

        void push(priority_type priority, const Key& key, Value value) {
            erase_(key);
            q[priority].push_back({key, std::move(value)});
            size++;
            idx[key] = std::make_pair(priority, std::prev(q[priority].end()));
        }

        bool erase(const Key& key) {
            return erase_(key);
        }

        std::pair<const Key, Value> pop() {
            if (!empty_()) {
                auto it = get_first();
                auto value = std::move(*it.second);
                erase_(it.second->first);
                return value;
            }
            throw;
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

        Iter get_first() {
            for (auto& qit : q) {
                if (!qit.second.empty()) {
                    return { qit.first, qit.second.begin() };
                }
            }
            return {};
        }

        bool empty_() const {
            return size == 0;
        }
    };

    PriorityList<std::string, std::shared_ptr<job_base>> pl;
    std::mutex pl_mutex;

public:
    utility::Task_ptr pop() {
        std::lock_guard<std::mutex> lg(pl_mutex);
        if (pl.empty()) {
            return {};
        }
        auto item = pl.pop();
        if (item.second->next) {
            pl.push(item.second->priority, item.first, item.second->next);
        }
        return std::move(item.second->task);
    }

    void push(const std::string &key, std::shared_ptr<job_base> job) {
        std::lock_guard<std::mutex> lg(pl_mutex);
        pl.push(job->priority, key, job);
    }

    void erase(const std::string &key) {
        std::lock_guard<std::mutex> lg(pl_mutex);
        pl.erase(key);
    }
};

}

#endif //CPP_TASK_LIBRARY_H
