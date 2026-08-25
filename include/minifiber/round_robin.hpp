#pragma once
#include <deque>
#include "minifiber/scheduler.hpp"

namespace minifiber {

class RoundRobinScheduler final: public Scheduler {
public:
    void awakened(Fiber *f) override { ready_.push_back(f); }

    Fiber *pick_next() override {
        if (ready_.empty()) return nullptr;
        Fiber *f = ready_.front(); ready_.pop_front();
        return f;
    }

    bool has_ready_fibers() const override { return !ready_.empty(); }

private:
    std::deque<Fiber *> ready_;
};

}