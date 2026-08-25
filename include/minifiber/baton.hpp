#pragma once
#include "minifiber/fiber.hpp"


namespace minifiber {

// 最小等待/唤醒原语（单 waiter 约定，违反由 assert 抓住）。
// 顺序无关：先 post 后 wait 与先 wait 后 post 都正确——
// Posted 态就是"早到的 post 的记忆"。
class Baton {
public:
    void wait();
    void post();

private:
    enum class St { Init, Posted, Waiting };
    St st_ = St::Init;
    Fiber* waiter_ = nullptr;
};

}