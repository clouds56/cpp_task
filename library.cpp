//
// Created by Clouds Flowing on 5/4/17.
//

#include "library.h"

#include <iostream>

using namespace cpp_task;
struct Test {
    task_queue main;
    decltype(auto) test1(int index, std::unique_ptr<double> &a, double &b) {
        auto f = [index](std::unique_ptr<double> a, double &b) {
            std::cerr << index << std::endl;
            *a *= b;
            b += 1;
            return a;
        };
        auto g = [](std::unique_ptr<double> p) {return *p;};
        return main.push(job_base::make(job_base::priority_realtime ,f, std::move(a), std::ref(b))
                                 .then([](std::unique_ptr<double> p) {return std::make_pair(std::move(p), 2);})
                , true);
    }

    std::future<std::pair<double, double>> test2(double a, double b) {
        auto *f = +[](double a, double b) {
            return std::make_pair(a + b, a - b);
        };
        return main.push(f, a, b);
    }

private:
    Test() {
        auto a = std::unique_ptr<double>(new double);
        auto b = 2.5;
        *a = 3;
        auto c = test1(1, a, b).get();
        a = std::move(c.first);
        std::cerr << *a << " " << b << " " << c.second << std::endl;
        auto d = test2(*a, b).get();
        std::cerr << d.first << " " << d.second << std::endl;
    }
    static Test test;
};

Test Test::test;
