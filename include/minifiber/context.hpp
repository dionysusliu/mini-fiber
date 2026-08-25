#pragma once
#include <cstdint>


namespace minifiber {

struct Context {
    std::uint64_t rbx;
    std::uint64_t rbp;
    std::uint64_t r12;
    std::uint64_t r13;
    std::uint64_t r14;
    std::uint64_t r15;
    std::uint64_t rsp;
};

extern "C" void switch_context(Context *from, Context *to);

}