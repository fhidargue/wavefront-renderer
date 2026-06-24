cmake_minimum_required(VERSION 3.20)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
endif()

project(WavefrontRenderer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(include)
include_directories(external)

# TBB, solution for mac and linux / github cicd
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew)
    find_package(TBB REQUIRED)
    set(TBB_LINK TBB::tbb)
else()
    find_library(TBB_LIB
        NAMES tbb
        HINTS /usr/local/lib64
        REQUIRED
    )
    message(STATUS "Found TBB at: ${TBB_LIB}")
    add_library(tbb_imported SHARED IMPORTED)
    set_target_properties(tbb_imported PROPERTIES
        IMPORTED_LOCATION ${TBB_LIB}
        INTERFACE_INCLUDE_DIRECTORIES /usr/local/include
    )
    set(TBB_LINK tbb_imported)
endif()

find_package(embree 4 REQUIRED)
find_package(GTest REQUIRED)

include_directories(${EMBREE_INCLUDE_DIRS})
include_directories(${GTEST_INCLUDE_DIRS})

set(RENDERER_SOURCES
    src/core/Scene.cpp
    src/geometry/Mesh.cpp
    src/shading/Material.cpp
    src/render/WavefrontRenderer.cpp
    src/scene/CornellBox.cpp
    src/accelerator/Embree.cpp
)

add_executable(renderer
    src/main.cpp
    ${RENDERER_SOURCES}
)

target_link_libraries(renderer PRIVATE
    ${TBB_LINK}
    embree
)

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
    ${TBB_LINK}
    embree
    GTest::GTest
    GTest::Main
)

enable_testing()
add_test(NAME WavefrontTests COMMAND tests)