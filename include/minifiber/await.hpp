#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <deque>
#include <vector>

#include "minifiber/fiber.hpp"

namespace minifiber {

/** 
 * thread-safe mailbox: a remote thread would park on this mailbox to awaken blocking fibers
 * 
 * based on mutex and condVar
*/ 
class RemoteMailbox {
public: 
    /// @brief called by any thread, to unblock a thread
    /// @param f fiber to be unblocked
    /// @return whether mailbox is empty
    bool push(Fiber *f);

    /// @brief called by runtime thread, to awaken all ready fibers
    /// @return all ready fibers
    std::vector<Fiber *> take_all();

    /// @brief  called by runtime thread, sleep until mailbox is non-empty OR deadline is passed
    /// @param deadline timeout
    void wait_until_nonempty(std::chrono::steady_clock::time_point deadline);

    /// @brief  called by any thread, to awaken the runtime thread
    void notify();


private:
    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<Fiber *> q_;
};

/// @brief  interface for park/poke pair in EventBase runtime
class IdleDriver {
public: 
    virtual ~IdleDriver() = default;
    virtual void park(std::chrono::steady_clock::time_point deadline) = 0;
    virtual void poke() = 0;
};


class CondVarIdleDriver final : public IdleDriver {
public:
    explicit CondVarIdleDriver(RemoteMailbox& mbox) : mbox_(mbox) {}
    void park(std::chrono::steady_clock::time_point deadline) override {
        mbox_.wait_until_nonempty(deadline);
    }
    void poke() override { mbox_.notify();}

private:
    RemoteMailbox& mbox_;
};


/// @brief  only entry for remote threads: awake one blocking fiber. The caller needs to ensure f would live until the runtime drain the mailbox
/// @param f 
void remote_wake(Fiber *f);

/// called by fiber: sleep for @p deadline seconds, awake by remote timer threads
void sleep_for(std::chrono::milliseconds ms);

namespace detail {
// for run() to run

void drain_remote_wakes();
bool has_pending_external();
void park_idle();
}
}