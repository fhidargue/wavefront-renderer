cmake_minimum_required(VERSION 3.20)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
endif()

project(WavefrontRenderer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_PREFIX_PATH /opt/homebrew)

include_directories(include)
include_directories(external)

find_package(TBB REQUIRED)
find_package(embree 4 REQUIRED)
find_package(GTest REQUIRED)

include_directories(${EMBREE_INCLUDE_DIRS})
include_directories(${GTEST_INCLUDE_DIRS})

# Shared sources used by both renderer and tests
set(RENDERER_SOURCES
    src/core/Scene.cpp
    src/geometry/Mesh.cpp
    src/shading/Material.cpp
    src/render/WavefrontRenderer.cpp
    src/scene/CornellBox.cpp
    src/accelerator/Embree.cpp
)

# Main renderer executable
add_executable(renderer
    src/main.cpp
    ${RENDERER_SOURCES}
)

target_link_libraries(renderer PRIVATE
    TBB::tbb
    embree
)

# Test executable
add_executable(tests
    tests/main.cpp
    tests/math/Vec3Test.cpp
    tests/core/RayTest.cpp
    tests/geometry/TriangleTest.cpp
    tests/geometry/SphereTest.cpp
    tests/scheduling/RayQueueTest.cpp
    tests/scheduling/ShadingQueueTest.cpp
    tests/shading/MaterialTest.cpp
    tests/accelerator/EmbreeTest.cpp
    tests/render/WavefrontRendererTest.cpp
    ${RENDERER_SOURCES}
)

target_link_libraries(tests PRIVATE
    TBB::tbb
    embree
    GTest::GTest
    GTest::Main
)

enable_testing()
add_test(NAME WavefrontTests COMMAND tests)