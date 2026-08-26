#include <thread>
#include <map>

#include "minifiber/await.hpp"

namespace minifiber {
namespace {

RemoteMailbox g_mailbox;
CondVarIdleDriver g_driver(g_mailbox);
int g_pending_external = 0;

/**
 * unified event source for a runtime thread, managing all external async events
 */
class EventSource {
public: 
    virtual ~EventSource() = default;
    virtual void stop() = 0;
};

/**
 * long-lived timer thread + multimap-based minheap
 */
class TimerSource final : public EventSource {
public:
    ~TimerSource() override { stop(); }

    void schedule(Fiber *f, std::chrono::steady_clock::time_point when) {
        std::lock_guard<std::mutex> lk(mu_);
        timers_.emplace(when, f);
        if (!worker_.joinable()) // lazy loading
            worker_ = std::thread(&TimerSource::run, this);
        cv_.notify_one();
    }

    void stop() override {
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (stopping_ ) return; // idempotent
            stopping_ = true;
        }
        cv_.notify_all();
        if (worker_.joinable()) worker_.join(); // ensure zero inflight pokes
    }

private:

    void run() {
        std::unique_lock<std::mutex> lk(mu_);
        for (;;) {
            if (timers_.empty()) 
                cv_.wait(lk, [&] { return stopping_ || !timers_.empty(); });
            else 
                cv_.wait_until(lk, timers_.begin()->first,
                               [&] { return stopping_; });

            if (stopping_) break;
            auto now = std::chrono::steady_clock::now();
            while (!timers_.empty() && timers_.begin()->first <= now) {
                Fiber* f = timers_.begin()->second;
                timers_.erase(timers_.begin());
                lk.unlock();                      // remote_wake 会锁邮箱，
                remote_wake(f);                   // 不能嵌套持锁调用
                lk.lock();
            }
        }
    }

    std::mutex mu_;
    std::condition_variable cv_;
    std::multimap<std::chrono::steady_clock::time_point, Fiber*> timers_;
    bool stopping_ = false;
    std::thread worker_;
};
TimerSource g_timer;


struct EventSourceFinalizer {
    ~EventSourceFinalizer() { g_timer.stop(); }
};
const EventSourceFinalizer g_finalizer;

} // namespace


bool RemoteMailbox::push(Fiber *f) {
    std::lock_guard<std::mutex> lk(mu_);
    bool was_first = q_.empty();
    q_.push_back(f);
    return was_first;
}

std::vector<Fiber *> RemoteMailbox::take_all() {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<Fiber *> out(std::make_move_iterator(q_.begin()),
                             std::make_move_iterator(q_.end()));
    q_.clear();
    return out;
}

void RemoteMailbox::wait_until_nonempty(std::chrono::steady_clock::time_point deadline) {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait_until(lk, deadline, [this] { return !q_.empty(); });
}

void RemoteMailbox::notify() {
    cv_.notify_one();
}

void remote_wake(Fiber *f) {
    if (g_mailbox.push(f)) { // poke only when g_mailbox shifts from empty->non-empty
        g_driver.poke();
    }
    // --- from now on mailbox is owned by runtime until it gets drained
}

void sleep_for(std::chrono::milliseconds ms) {
    ++g_pending_external;
    Fiber *self = detail::current();
    g_timer.schedule(self, std::chrono::steady_clock::now() + ms);
    detail::suspend_current(); // current fiber shift from running->blocked
}

namespace detail {

void drain_remote_wakes() {
    for (Fiber *f : g_mailbox.take_all()) {
        --g_pending_external;
        wake(f); // Blocked -> Ready && awakened
    }
}

bool has_pending_external() { return g_pending_external > 0; }

void park_idle() {
    g_driver.park(std::chrono::steady_clock::time_point::max());
}

}
}