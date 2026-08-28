cmake_minimum_required(VERSION 3.14)
project(mini_fiber CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- 依赖库 ----
find_package(Threads REQUIRED)                    # std::thread/mutex/cv 需要

# ---- 正式 runtime 库 ----
add_library(minifiber
    src/fiber.cpp
    src/baton.cpp
    src/await.cpp
    src/switch_context.S
)
target_include_directories(minifiber PUBLIC include)
target_link_libraries(minifiber PUBLIC Threads::Threads)

# ---- 以下为开发期产物：仅顶层构建时生效，被 FetchContent 收编时全部跳过 ----
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)

# ---- 示例 ----
add_executable(demo_sched examples/demo_sched.cpp)
target_link_libraries(demo_sched PRIVATE minifiber)

add_executable(demo_baton examples/demo_baton.cpp)
target_link_libraries(demo_baton PRIVATE minifiber)

add_executable(demo_await examples/demo_await.cpp)
target_link_libraries(demo_await PRIVATE minifiber)

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

add_executable(baton_test tests/baton_test.cpp)
target_link_libraries(baton_test PRIVATE minifiber GTest::gtest_main)
add_test(NAME baton_test COMMAND baton_test)

add_executable(await_test tests/await_test.cpp)
target_link_libraries(await_test PRIVATE minifiber GTest::gtest_main)
add_test(NAME await_test COMMAND await_test)

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

add_executable(wake_bench poc/wake_bench.cpp)
target_link_libraries(wake_bench PRIVATE minifiber)

endif()