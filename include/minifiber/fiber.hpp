#pragma once
#include <functional>
#include <memory>
#include <vector>

#include "minifiber/context.hpp"

namespace minifiber {

enum class State {
    Ready,
    Running,
    Finished,
};

struct Fiber {
    Context ctx;
    std::vector<char> stack;
    std::function<void()> fn;
    State state = State::Ready;
    int priority = 0; // referenced by policy
};

void spawn(std::function<void()> fn, int priority = 0); 
void yield();
void run();


class Scheduler; // see scheduler.hpp for interfaces
void use_scheduler(std::unique_ptr<Scheduler> sched); // RoundRobin by default

}
