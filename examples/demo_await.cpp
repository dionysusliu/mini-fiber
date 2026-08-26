#include <chrono>
#include <minifiber/await.hpp>
#include <minifiber/fiber.hpp>
#include <minifiber/round_robin.hpp>
#include <cstdio>

int main() {
    minifiber::use_scheduler(std::make_unique<minifiber::RoundRobinScheduler>());

    minifiber::spawn([] {
        printf("A: start, sleeping 100ms\n");
        minifiber::sleep_for(std::chrono::milliseconds(100));
        printf("A: woke after sleep\n");
    });
    minifiber::spawn([] {
        printf("B: start, sleeping 30ms\n");
        minifiber::sleep_for(std::chrono::milliseconds(30));
        printf("B: woke after sleep\n");
    });
    minifiber::spawn([] {
        printf("C: running without sleep\n");
        minifiber::yield();
        printf("C: done\n");
    });

    printf("main: run...\n");
    minifiber::run();
    printf("main: all done\n");
}