#pragma once

#include <random>
#include <thread>

inline std::mt19937& getThreadRNG() {
    // This run on each thread call instead
    thread_local std::mt19937 rng(
        std::hash<std::thread::id>{}(std::this_thread::get_id()) ^ 42u
    );

    return rng;
}

inline float randomFloat() {
    thread_local std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(getThreadRNG());
}