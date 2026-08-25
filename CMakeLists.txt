cmake_minimum_required(VERSION 3.14)
project(mini_fiber CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- 正式 runtime 库 ----
add_library(minifiber
    src/fiber.cpp
    src/switch_context.S
)
target_include_directories(minifiber PUBLIC include)

# ---- 示例 ----
add_executable(demo_sched examples/demo_sched.cpp)
target_link_libraries(demo_sched PRIVATE minifiber)

# ---- 单测（GoogleTest，FetchContent）----
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(sched_test tests/sched_test.cpp)
target_link_libraries(sched_test PRIVATE minifiber GTest::gtest_main)
add_test(NAME sched_test COMMAND sched_test)

add_executable(demo_priority examples/demo_priority.cpp)
target_link_libraries(demo_priority PRIVATE minifiber)

# ---- benchmark（FetchContent，bench 改为链库取符号）----
include(FetchContent)
FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG        v1.9.1
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

add_executable(bench_ctx poc/ctx_bench.cpp)
target_link_libraries(bench_ctx PRIVATE minifiber benchmark::benchmark)
target_compile_options(bench_ctx PRIVATE -march=native)