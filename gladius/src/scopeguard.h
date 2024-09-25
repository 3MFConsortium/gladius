#pragma once

#include <functional>

namespace gladius
{
    struct scope_guard
    {
        std::function<void()> onExit;
        ~scope_guard()
        {
            onExit();
        }

        scope_guard(scope_guard const &) = delete;
        scope_guard & operator=(scope_guard const &) = delete;

        scope_guard(std::function<void()> onExit)
            : onExit(onExit)
        {
        }
    };
}