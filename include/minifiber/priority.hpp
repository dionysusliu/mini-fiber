#pragma once
#include <map>
#include "minifiber/scheduler.hpp"

namespace minifiber {

class PriorityScheduler final: public Scheduler {
public:
    void awakened(Fiber *f) override { ready_.emplace(f->priority, f); }

    Fiber *pick_next() override {
        if (ready_.empty()) return nullptr;
        Fiber *f = ready_.begin()->second; ready_.erase(ready_.begin());
        return f;
    }

    bool has_ready_fibers() const override { return !ready_.empty(); }

private:
    std::multimap<int, Fiber *, std::greater<int>> ready_;
};

}