//
// Created by Clouds Flowing on 5/5/17.
//

#include "library.h"
#include <iostream>

using namespace cpp_task;
struct Test {
    decltype(auto) test1(int index, std::unique_ptr<double> &a, double &b) {
        auto f = [index](std::unique_ptr<double> a, double &b) {
            std::cerr << index << std::endl;
            *a *= b;
            b += 1;
            return a;
        };
        auto g = [](std::unique_ptr<double> p) {return *p;};
        return task_queue::main.push(job_base::make(job_base::priority_realtime ,f, std::move(a), std::ref(b))
                                 .then([](std::unique_ptr<double> p) {return std::make_pair(std::move(p), 2);})
                , true);
    }

    std::future<std::pair<double, double>> test2(double a, double b) {
        auto *f = +[](double a, double b) {
            return std::make_pair(a + b, a - b);
        };
        return task_queue::main.push(f, a, b);
    }

    double rtn = 0;

    Test() {
        auto a = std::unique_ptr<double>(new double);
        auto b = 2.5;
        *a = 3;
        auto c = test1(1, a, b).get();
        a = std::move(c.first);
        std::cerr << *a << " " << b << " " << c.second << std::endl;
        auto d = test2(*a, b).get();
        std::cerr << d.first << " " << d.second << std::endl;
        auto s = static_cast<priority_scheduler*>(task_queue::main.get_scheduler().get());
        auto job = job_base::make_shared(1, [](std::pair<double, double> i) { return i.first + i.second; }, d)
                ->then([this](double p) { this->rtn = 1; return p/2; })->commit();
        s->push("test", job.first);
        std::cerr << job.second.get() << std::endl;
    }
    ~Test() {
        std::cerr << rtn << std::endl;
    }
};

int main() {
    Test test;
    return 0;
}
