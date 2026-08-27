// poc/wake_bench.cpp — remote wake path 延迟/吞吐基准
// 用法: wake_bench <idle|busy> <runtime_cpu> <b_cpu> <iters>                                     
//   idle: 完整唤醒链（投递→futex wake→内核调度→drain→resume fiber→ack）
//   busy: 纯投递路径（独立 mailbox，无人消费：mutex+deque+notify 三件套）
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <thread>
#include <sched.h>
#include <vector>

#include "minifiber/await.hpp"
#include "minifiber/fiber.hpp"
#include "minifiber/round_robin.hpp"

using namespace std::chrono;

static std::atomic<int> g_ack{0};
static std::atomic<bool> g_stop{false};
static minifiber::Fiber* g_sleeper = nullptr;

static void sleeper_fiber() {
    g_sleeper = minifiber::detail::current();   // 自报家门（bench 专用）
    while(!g_stop.load(std::memory_order_acquire)) {
        minifiber::wait_for_external();         // 纯等待：不占 timer 堆
        g_ack.fetch_add(1, std::memory_order_release);
    }
}

static void pin_self(int cpu) {
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(cpu, &set);
    sched_setaffinity(0, sizeof set, &set);
}

int main(int argc, char** argv) {
    const char* mode = argc > 1 ? argv[1] : "idle";
    int rt_cpu = argc > 2 ? atoi(argv[2]) : 0;
    int b_cpu  = argc > 3 ? atoi(argv[3]) : 1;
    int iters  = argc > 4 ? atoi(argv[4]) : 10000;

    minifiber::use_scheduler(std::make_unique<minifiber::RoundRobinScheduler>());
    pin_self(rt_cpu);                            // runtime 线程钉核

    if (strcmp(mode, "idle") == 0) {
        minifiber::spawn(sleeper_fiber);

        std::thread b([&] {
            pin_self(b_cpu);
            std::this_thread::sleep_for(milliseconds(200));  // 等 park 稳定
            std::vector<long long> samples;
            const int warmup = 100;
            for (int i = 0; i < warmup + iters; ++i) {
                g_ack = 0;
                auto t0 = steady_clock::now();
                minifiber::remote_wake(g_sleeper);
                while (g_ack.load(std::memory_order_acquire) == 0) {}
                auto t1 = steady_clock::now();
                if (i >= warmup)
                    samples.push_back(duration_cast<nanoseconds>(t1 - t0).count());
            }
            std::sort(samples.begin(), samples.end());
            auto q = [&](double p) { return samples[(size_t)(p * (samples.size() - 1))]; };
            printf("idle  rt=cpu%d b=cpu%d n=%d  min=%lldns median=%lldns p99=%lldns\n",
                    rt_cpu, b_cpu, iters, samples.front(), q(0.5), q(0.99));
            g_stop.store(true, std::memory_order_release);
            minifiber::remote_wake(g_sleeper);
        });

        minifiber::run();      // park → drain → resume 循环
        b.join();
    } else {                   // busy：纯投递开销（无人消费，deque 涨到 N 条）
        minifiber::RemoteMailbox mbox;
        std::thread b([&] {
            pin_self(b_cpu);
            auto t0 = steady_clock::now();
            for (int i = 0; i < iters; ++i)
                mbox.push((minifiber::Fiber*)0x1);   // 哑指针
            auto t1 = steady_clock::now();
            auto ns = duration_cast<nanoseconds>(t1 - t0).count();
            printf("busy  rt=cpu%d b=cpu%d n=%d  %.1f ns/push\n",
                    rt_cpu, b_cpu, iters, (double)ns / iters);
            g_stop.store(true, std::memory_order_release);
            minifiber::remote_wake(g_sleeper);
        });
        b.join();
    }
    return 0;
}

