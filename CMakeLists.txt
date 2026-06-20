cmake_minimum_required(VERSION 3.20)

set(CMAKE_OSX_ARCHITECTURES "arm64")

project(WavefrontRenderer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_PREFIX_PATH /opt/homebrew)


include_directories(include)

find_package(TBB REQUIRED)
find_package(embree 4 REQUIRED)

add_executable(renderer src/main.cpp)

target_link_libraries(renderer PRIVATE TBB::tbb embree)