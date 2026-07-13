cmake_minimum_required(VERSION 3.20)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
endif()

project(WavefrontRenderer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(include)

set(HDRI_DIR "${CMAKE_SOURCE_DIR}/scenes/hdri" CACHE PATH "Directory containing HDRI environment maps")
add_compile_definitions(HDRI_DIR="${HDRI_DIR}")

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew)
else()
    list(APPEND CMAKE_PREFIX_PATH $ENV{HOME}/deps)
endif()

option(ENABLE_USD "Enable USD scene loading" OFF)

if(ENABLE_USD)
    if(DEFINED ENV{USD_INSTALL_DIR})
        set(USD_INSTALL_DIR "$ENV{USD_INSTALL_DIR}")
    elseif(APPLE)
        set(USD_INSTALL_DIR "$ENV{HOME}/Documents/Projects/USDInstall")
    else()
        set(USD_INSTALL_DIR "$ENV{HOME}/USDInstall")
    endif()

    list(APPEND CMAKE_PREFIX_PATH ${USD_INSTALL_DIR} ${USD_INSTALL_DIR}/lib64/cmake)
    find_package(pxr REQUIRED HINTS ${USD_INSTALL_DIR})

    include_directories(SYSTEM ${USD_INSTALL_DIR}/include)
    link_directories(${USD_INSTALL_DIR}/lib)
    add_compile_definitions(ENABLE_USD)
    set(TBB_LINK "${USD_INSTALL_DIR}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}tbb${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(USD_LIBS usd_usd usd_usdGeom usd_usdShade usd_sdf usd_tf usd_ar usd_gf usd_vt usd_plug usd_work usd_usdLux)
    
    # Suppress TBB deprecation messages
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        add_compile_options(-Wno-\#pragma-messages)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-Wno-cpp)
    endif()
else()
    find_package(TBB REQUIRED)
    set(TBB_LINK TBB::tbb)
    set(USD_LIBS "")
endif()

# Main packages
find_package(embree 4 REQUIRED)

if(NOT APPLE)
    if(EXISTS "$ENV{HOME}/deps/lib64/cmake/GTest")
        set(GTest_DIR "$ENV{HOME}/deps/lib64/cmake/GTest" CACHE PATH "" FORCE)
    endif()
    if(EXISTS "$ENV{HOME}/deps/lib64/cmake/OpenImageIO")
        set(OpenImageIO_DIR "$ENV{HOME}/deps/lib64/cmake/OpenImageIO" CACHE PATH "" FORCE)
    endif()
    if(EXISTS "$ENV{HOME}/deps/lib64/cmake/OpenImageDenoise")
        set(OpenImageDenoise_DIR "$ENV{HOME}/deps/lib64/cmake/OpenImageDenoise" CACHE PATH "" FORCE)
    endif()
endif()

find_package(GTest REQUIRED)
find_package(OpenImageIO REQUIRED)
find_package(OpenImageDenoise REQUIRED)

if(TARGET GTest::gtest)
    set(GTEST_LIBS GTest::gtest GTest::gtest_main)
else()
    set(GTEST_LIBS GTest::GTest GTest::Main)
endif()

include_directories(${EMBREE_INCLUDE_DIRS})

set(RENDERER_SOURCES
    src/core/Scene.cpp
    src/core/Image.cpp
    src/core/Denoiser.cpp
    src/core/EnvironmentMap.cpp
    src/geometry/Mesh.cpp
    src/shading/Material.cpp
    src/render/WavefrontRenderer.cpp
    src/scene/CornellBox.cpp
    src/scene/CameraLoader.cpp
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
    ${USD_LIBS}
    OpenImageIO::OpenImageIO
    OpenImageDenoise
)

add_executable(tests
    tests/main.cpp
    tests/math/Vec3Test.cpp
    tests/core/RayTest.cpp
    tests/core/EnvironmentMapTest.cpp
    tests/core/SceneTest.cpp
    tests/geometry/TriangleTest.cpp
    tests/geometry/SphereTest.cpp
    tests/scheduling/RayQueueTest.cpp
    tests/scheduling/ShadingQueueTest.cpp
    tests/shading/MaterialTest.cpp
    tests/accelerator/EmbreeTest.cpp
    tests/render/WavefrontRendererTest.cpp
    tests/render/CostTrackerTest.cpp
    tests/render/AdaptiveSamplerTest.cpp
    tests/shading/MaterialFeaturesTest.cpp
    ${RENDERER_SOURCES}
)

if(ENABLE_USD)
    target_sources(tests PRIVATE tests/scene/UsdSceneLoaderTest.cpp tests/scene/UsdLuxLightTest.cpp)
endif()

target_link_libraries(tests PRIVATE
    ${TBB_LINK}
    embree
    ${USD_LIBS}
    OpenImageIO::OpenImageIO
    OpenImageDenoise
    ${GTEST_LIBS}
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