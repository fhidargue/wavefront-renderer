#pragma once

#include <cstdlib>

// NOTE: randomFloat() uses std::rand() which is NOT thread-safe.

inline float randomFloat() {
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}