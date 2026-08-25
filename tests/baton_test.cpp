#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "minifiber/baton.hpp"
#include "minifiber/fiber.hpp"
#include "minifiber/round_robin.hpp"

using namespace minifiber;

struct RuntimeFixture : ::testing::Test {
    void SetUp() override {
        use_scheduler(std::make_unique<RoundRobinScheduler>());
    }
};

TEST_F(RuntimeFixture, WaitBlocksUntilPost) {
    std::vector<std::string> order;
    Baton baton;
    spawn([&] { order.push_back("wait-begin"); baton.wait(); order.push_back("wait-end"); });           
    spawn([&] { order.push_back("post"); baton.post(); });
    run();
    EXPECT_EQ((std::vector<std::string>{"wait-begin", "post", "wait-end"}), order);
}

TEST_F(RuntimeFixture, PostBeforeWaitDoesNotBlock) {
    std::vector<std::string> order;
    Baton baton;
    spawn([&] { baton.post(); order.push_back("posted"); });
    spawn([&] { order.push_back("wait-begin"); baton.wait(); order.push_back("wait-end"); });
    run();
    EXPECT_EQ((std::vector<std::string>{"posted", "wait-begin", "wait-end"}), order);
}

TEST_F(RuntimeFixture, WokenTakesQueuePositionAtPostTime) {                                             
    // wake 只入队不切换：被唤醒者在 post() 时刻获得队位，
    // 早于 poster 之后 yield 的队位（无抢占的直接证据）
    std::vector<std::string> order;
    Baton baton;
    spawn([&] { baton.wait(); order.push_back("woken"); });
    spawn([&] {
        baton.post();                   // waiter 入队：[A]
        order.push_back("poster-working");
        yield();                        // poster 入队：[A, B]
        order.push_back("poster-resumed");
    });
    run();
    EXPECT_EQ((std::vector<std::string>{"poster-working", "woken", "poster-resumed"}), order);
}

TEST_F(RuntimeFixture, BatonIsReusable) {
    int passes = 0;
    Baton baton;
    spawn([&] { baton.wait(); ++passes; baton.wait(); ++passes; });
    spawn([&] { baton.post(); baton.post(); });
    run();
    EXPECT_EQ(2, passes);
}