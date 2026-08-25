// tests/sched_test.cpp — 策略纯逻辑单测：不需要运行 fiber
#include <gtest/gtest.h>
#include "minifiber/fiber.hpp"
#include "minifiber/scheduler.hpp"
#include "minifiber/round_robin.hpp"
#include "minifiber/priority.hpp"

using namespace minifiber;

// Fiber 只作为策略的载荷：用到的只有指针身份和 priority 字段
static Fiber mk(int p = 0) {
    Fiber f;
    f.priority = p;
    return f;
}

TEST(RoundRobin, FifoOrder) {
    RoundRobinScheduler s;
    Fiber a = mk(), b = mk(), c = mk();
    s.awakened(&a); s.awakened(&b); s.awakened(&c);
    EXPECT_TRUE(s.has_ready_fibers());
    EXPECT_EQ(s.pick_next(), &a);
    EXPECT_EQ(s.pick_next(), &b);
    EXPECT_EQ(s.pick_next(), &c);
    EXPECT_FALSE(s.has_ready_fibers());
    EXPECT_EQ(s.pick_next(), nullptr);
}

TEST(RoundRobin, YieldedFiberGoesToTail) {
    RoundRobinScheduler s;
    Fiber a = mk(), b = mk();
    s.awakened(&a); s.awakened(&b);
    EXPECT_EQ(s.pick_next(), &a);
    s.awakened(&a);               // run() 归还 yield 的 fiber → 排队尾
    EXPECT_EQ(s.pick_next(), &b);
    EXPECT_EQ(s.pick_next(), &a);
}

TEST(Priority, HigherRunsFirst) {
    PriorityScheduler s;
    Fiber low = mk(1), high = mk(10), mid = mk(5);
    s.awakened(&low); s.awakened(&high); s.awakened(&mid);
    EXPECT_EQ(s.pick_next(), &high);
    EXPECT_EQ(s.pick_next(), &mid);
    EXPECT_EQ(s.pick_next(), &low);
}

TEST(Priority, SamePriorityIsFifo) {
    PriorityScheduler s;
    Fiber a = mk(3), b = mk(3), c = mk(3);
    s.awakened(&a); s.awakened(&b); s.awakened(&c);
    EXPECT_EQ(s.pick_next(), &a);
    EXPECT_EQ(s.pick_next(), &b);
    EXPECT_EQ(s.pick_next(), &c);
}

TEST(Priority, LateHighPriorityWinsNextPick) {
    PriorityScheduler s;
    Fiber a = mk(1), b = mk(9);
    s.awakened(&a);
    EXPECT_EQ(s.pick_next(), &a);
    s.awakened(&b);               // a 让出后，高优先级先于任何后来者被取
    EXPECT_EQ(s.pick_next(), &b);
}