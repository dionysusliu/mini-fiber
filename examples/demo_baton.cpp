#include <minifiber/baton.hpp>
#include <minifiber/fiber.hpp>
#include <minifiber/round_robin.hpp>
#include <cstdio>

int main() {
    minifiber::use_scheduler(std::make_unique<minifiber::RoundRobinScheduler>());

    // 场景 1：先 wait 后 post（真正的阻塞-唤醒路径）
    minifiber::Baton b1;
    minifiber::spawn([&] {
        printf("1: waiter blocks\n");
        b1.wait();
        printf("1: waiter woke\n");
    });
    minifiber::spawn([&] {
        printf("1: poster posts\n");
        b1.post();
    });
    minifiber::run();

    // 场景 2：先 post 后 wait（消费"记忆"的路径）
    minifiber::Baton b2;
    minifiber::spawn([&] {
        b2.post();                      // 此刻无人等待 → 记住
        printf("2: posted early\n");
    });
    minifiber::spawn([&] {
        printf("2: waiter checks\n");
        b2.wait();                      // 消费记忆，立即通过
        printf("2: waiter passed without blocking\n");
    });
    minifiber::run();

    printf("done\n");
}