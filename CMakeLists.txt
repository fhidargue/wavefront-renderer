cmake_minimum_required(VERSION 3.20)
project(WavefrontRenderer VERSION 0.1.0 LANGUAGES CXX)

# Tell the compiler to use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include the target directories
include_directories(include)

# Find our source files
add_executable(renderer src/main.cpp)
