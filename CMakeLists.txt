cmake_minimum_required(VERSION 3.20)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
endif()

project(WavefrontRenderer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(include)

list(APPEND CMAKE_PREFIX_PATH /opt/homebrew)

option(ENABLE_USD "Enable USD scene loading" OFF)

if(ENABLE_USD)
    set(USD_INSTALL_DIR "$ENV{HOME}/Documents/Projects/USDInstall")
    find_package(pxr REQUIRED HINTS ${USD_INSTALL_DIR})
    include_directories(${USD_INSTALL_DIR}/include)
    link_directories(${USD_INSTALL_DIR}/lib)
    add_compile_definitions(ENABLE_USD)
    set(TBB_LINK "${USD_INSTALL_DIR}/lib/libtbb.dylib")
    set(USD_LIBS usd_usd usd_usdGeom usd_usdShade usd_sdf usd_tf usd_ar usd_gf usd_vt usd_plug usd_work)
    
    # Suppress TBB warnings
    add_compile_options(-Wno-\#pragma-messages)
else()
    find_package(TBB REQUIRED)
    set(TBB_LINK TBB::tbb)
    set(USD_LIBS "")
endif()

find_package(embree 4 REQUIRED)
find_package(GTest REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenImageIO REQUIRED)

include_directories(${EMBREE_INCLUDE_DIRS})

set(RENDERER_SOURCES
    src/core/Scene.cpp
    src/core/Image.cpp
    src/geometry/Mesh.cpp
    src/shading/Material.cpp
    src/render/WavefrontRenderer.cpp
    src/scene/CornellBox.cpp
    src/accelerator/Embree.cpp
)

if(ENABLE_USD)
    list(APPEND RENDERER_SOURCES src/scene/UsdSceneLoader.cpp)
endif()

add_executable(renderer
    src/main.cpp
    ${RENDERER_SOURCES}
)

target_link_libraries(renderer PRIVATE
    ${TBB_LINK}
    embree
    nlohmann_json::nlohmann_json
    ${USD_LIBS}
    OpenImageIO::OpenImageIO
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

if(ENABLE_USD)
    target_sources(tests PRIVATE tests/scene/UsdSceneLoaderTest.cpp)
endif()

target_link_libraries(tests PRIVATE
    ${TBB_LINK}
    embree
    nlohmann_json::nlohmann_json
    ${USD_LIBS}
    OpenImageIO::OpenImageIO
    GTest::GTest
    GTest::Main
)

# rpath so renderer and tests find USD dylibs at runtime
if(ENABLE_USD)
    set_target_properties(renderer PROPERTIES
        BUILD_RPATH "${USD_INSTALL_DIR}/lib"
        INSTALL_RPATH "${USD_INSTALL_DIR}/lib"
    )
    set_target_properties(tests PROPERTIES
        BUILD_RPATH "${USD_INSTALL_DIR}/lib"
        INSTALL_RPATH "${USD_INSTALL_DIR}/lib"
    )
endif()

enable_testing()
add_test(NAME WavefrontTests COMMAND tests)