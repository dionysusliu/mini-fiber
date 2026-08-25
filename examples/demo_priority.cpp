#include <minifiber/fiber.hpp>
#include <minifiber/priority.hpp>
#include <cstdio>

int main() {
    minifiber::use_scheduler(std::make_unique<minifiber::PriorityScheduler>());

    minifiber::spawn([] { printf("low  (p=1) runs\n"); }, 1);
    minifiber::spawn([] { printf("high (p=10) runs\n"); }, 10);
    minifiber::spawn([] { printf("mid  (p=5) runs\n"); }, 5);

    minifiber::run();   // 三个 fiber 均无 yield：按优先级依次跑完
}