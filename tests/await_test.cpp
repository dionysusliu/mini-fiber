#include <gtest/gtest.h>
#include <chrono>
#include <string>
#include <vector>
#include "minifiber/await.hpp"
#include "minifiber/fiber.hpp"
#include "minifiber/round_robin.hpp"

using namespace minifiber;

struct AwaitFixture : ::testing::Test {
    void SetUp() override {
        use_scheduler(std::make_unique<RoundRobinScheduler>());
    }
};

TEST_F(AwaitFixture, SleepIsWokenExternally) {
    std::vector<std::string> order;
    spawn([&] {
        order.push_back("sleep-begin");
        sleep_for(std::chrono::milliseconds(20));
        order.push_back("sleep-end");
    });
    spawn([&] { order.push_back("other"); });
    run();
    EXPECT_EQ((std::vector<std::string>{"sleep-begin", "other", "sleep-end"}), order);
}

TEST_F(AwaitFixture, TwoSleepsWakeInDeadlineOrder) {
    std::vector<std::string> order;
    spawn([&] { sleep_for(std::chrono::milliseconds(120)); order.push_back("late"); });
    spawn([&] { sleep_for(std::chrono::milliseconds(20)); order.push_back("early"); });
    run();
    EXPECT_EQ((std::vector<std::string>{"early", "late"}), order);
}

TEST_F(AwaitFixture, RunUnchangedWithoutExternalWaits) {
    // 无外部等待时 run() 语义与 v0.3 完全一致
    int ran = 0;
    spawn([&] { ++ran; yield(); ++ran; });
    run();
    EXPECT_EQ(2, ran);
}