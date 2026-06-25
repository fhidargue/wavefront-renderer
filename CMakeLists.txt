cmake_minimum_required(VERSION 3.20)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
endif()

project(WavefrontRenderer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# All Homebrew packages
list(APPEND CMAKE_PREFIX_PATH /opt/homebrew)

include_directories(include)

# USD from local build
set(USD_INSTALL_DIR "$ENV{HOME}/Documents/Projects/USDInstall")
find_package(pxr REQUIRED HINTS ${USD_INSTALL_DIR})
include_directories(${USD_INSTALL_DIR}/include)

find_package(embree 4 REQUIRED)
find_package(GTest REQUIRED)
find_package(nlohmann_json REQUIRED)

include_directories(${EMBREE_INCLUDE_DIRS})

set(RENDERER_SOURCES
    src/core/Scene.cpp
    src/geometry/Mesh.cpp
    src/shading/Material.cpp
    src/render/WavefrontRenderer.cpp
    src/scene/CornellBox.cpp
    src/scene/UsdSceneLoader.cpp
    src/accelerator/Embree.cpp
)

add_executable(renderer
    src/main.cpp
    ${RENDERER_SOURCES}
)

target_link_libraries(renderer PRIVATE
    TBB::tbb
    embree
    nlohmann_json::nlohmann_json
    usd_usd
    usd_usdGeom
    usd_usdShade
    usd_sdf
    usd_tf
    usd_ar
    usd_gf
    usd_vt
    usd_plug
    usd_work
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
    TBB::tbb
    embree
    nlohmann_json::nlohmann_json
    usd_usd
    usd_usdGeom
    usd_usdShade
    usd_sdf
    usd_tf
    usd_ar
    usd_gf
    usd_vt
    usd_plug
    usd_work
    GTest::GTest
    GTest::Main
)

enable_testing()
add_test(NAME WavefrontTests COMMAND tests)