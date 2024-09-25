#pragma once

#include <chrono>
#include <iostream>

template <typename TimeT = std::chrono::milliseconds>

// usage: auto time = measure::execution([&](){ /* code */ });  // returns time in milliseconds
struct measure
{
    template <typename F, typename... Args>
    static typename TimeT::rep execution(F && func, Args &&... args)
    {
        const auto start = std::chrono::steady_clock::now();
        std::forward<decltype(func)>(func)(std::forward<Args>(args)...);
        auto duration = std::chrono::duration_cast<TimeT>(std::chrono::steady_clock::now() - start);
        return duration.count();
    }
};
