include(../cmake/warnings.cmake)
include(../cmake/sanitizers.cmake)

add_library(pt_lib
        core/handle.hpp
        core/ray.hpp
        core/color.hpp
        core/bbox.hpp
        core/simd.hpp

        scene/toml_helpers.hpp
        scene/toml_helpers.cpp
        scene/mesh_parser.hpp
        scene/mesh_parser.cpp
        scene/properties.hpp
        scene/properties.cpp

        aggregates/bvh.cpp
        aggregates/bvh.hpp
        aggregates/bvh_build.cpp
        aggregates/bvh_build.hpp
        aggregates/bvh_8.hpp
        aggregates/bvh_8.cpp

        cameras.hpp
        cameras.cpp
        scene.hpp
        scene.cpp
        integrators.hpp
        integrators.cpp
        shapes.hpp
        shapes.cpp
        primitives.hpp
        primitives.cpp
        core/utils.hpp
        core/utils.cpp
        sampler.hpp
        warp.hpp
        emitters.hpp
        emitters.cpp
        bsdfs.hpp
        bsdfs.cpp
        concurrency.hpp
        concurrency.cpp

        light_samplers.cpp
        light_samplers.hpp
        coordinate_frame.hpp
        stb.cpp
)

target_include_directories(pt_lib PUBLIC ./)
target_compile_features(pt_lib PUBLIC cxx_std_23)
target_precompile_headers(pt_lib PUBLIC
        "common.hpp"
        <print>
        <format>
        <optional>
        <vector>
        <span>
)

enable_warnings(pt_lib)
treat_warnings_as_errors(pt_lib)

if (MSVC)
    # since __VA_OPT__ is not supported by default on msvc
    target_compile_options(pt_lib PUBLIC /Zc:preprocessor)
else ()
    # TODO: probably not the best way of doing this not sure
    # if both gcc and clang need the flag to support simd

    # enables simd extensions
    target_compile_options(pt_lib PUBLIC -march=native)
endif ()

find_package(tomlplusplus REQUIRED)
find_package(fast-obj REQUIRED)
find_package(Stb REQUIRED)
target_include_directories(pt_lib PRIVATE ${Stb_INCLUDE_DIR})
target_link_libraries(pt_lib PRIVATE
        tomlplusplus::tomlplusplus
        fast-obj::fast-obj
)
target_link_libraries(pt_lib PUBLIC
        gmath
)

add_executable(pt
        bin/cli.hpp
        bin/cli.cpp
        bin/interactive_viewer.cpp
        bin/interactive_viewer.hpp
        bin/main.cpp
)

target_compile_features(pt PRIVATE cxx_std_23)

enable_warnings(pt)
treat_warnings_as_errors(pt)

find_package(SDL3 REQUIRED)
target_link_libraries(pt PRIVATE pt_lib SDL3::SDL3)
