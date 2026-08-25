cmake_minimum_required(VERSION 3.14)
project(mini_fiber CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.9.1
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)  # 不连带拉 GoogleTest
FetchContent_MakeAvailable(benchmark)

add_executable(bench_ctx poc/ctx_bench.cpp poc/switch_context.S)
target_link_libraries(bench_ctx PRIVATE benchmark::benchmark)
target_compile_options(bench_ctx PRIVATE -march=native)  # AGENTS.md：Release 加分毫必争