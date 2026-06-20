#pragma once

#include <cstdlib>

inline float randomFloat() {
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}