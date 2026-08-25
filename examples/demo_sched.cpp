#include <minifiber/fiber.hpp>
#include <minifiber/round_robin.hpp>
#include <cstdio>

int main() { 
    minifiber::use_scheduler(std::make_unique<minifiber::RoundRobinScheduler>());

    minifiber::spawn([] {
        printf("fiber1: start\n");
        minifiber::yield();
        printf("fiber1: resumed\n");
    });
    minifiber::spawn([] {
        printf("fiber2: start\n");
        minifiber::yield();
        printf("fiber2: done\n");
    });

    printf("main: before run\n");
    minifiber::run();
    printf("main: after run\n");

}