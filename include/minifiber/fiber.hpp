#pragma once
#include <functional>
#include <memory>
#include <vector>

#include "minifiber/context.hpp"

namespace minifiber {

enum class State {
    Ready,
    Running,
    Blocked,
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

namespace detail {

Fiber* current(); // currently running Fiber
void suspend_current(); // shift from Running --> Blocked, and switch back to scheduler;
void wake(Fiber *f); // shift from Blocked --> Ready 

}

class Scheduler; // see scheduler.hpp for interfaces
void use_scheduler(std::unique_ptr<Scheduler> sched); // RoundRobin by default

}
