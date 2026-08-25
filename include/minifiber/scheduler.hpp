#pragma once
#include "minifiber/fiber.hpp"

namespace minifiber {

/// @brief policy adaptor for a thread running fibers
/// a 'policy' expose three interfaces: 
///     1. register a new fiber; 
///     2. pick next fiber to run; 
///     3. tell if any ready fibers left?
class Scheduler {
public:
    virtual ~Scheduler() = default;
    virtual void awakened(Fiber *f) = 0; // make f ready, policy would manage it
    virtual Fiber* pick_next() = 0; // pick next fiber to run
    virtual bool has_ready_fibers() const = 0;
};

}