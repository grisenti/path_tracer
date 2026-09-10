include(../cmake/warnings.cmake)
include(../cmake/sanitizers.cmake)

add_executable(path_tracer
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
        cli.hpp
        cli.cpp
        main.cpp
        light_samplers.cpp
        light_samplers.hpp
        coordinate_frame.hpp
        interactive_viewer.cpp
        interactive_viewer.hpp
        stb.cpp
)

target_include_directories(path_tracer PRIVATE ./)
target_compile_features(path_tracer PRIVATE cxx_std_23)
target_precompile_headers(path_tracer PRIVATE
        "common.hpp"
        <print>
        <format>
        <optional>
        <vector>
        <span>
)

enable_warnings(path_tracer)

if (MSVC)
    # since __VA_OPT__ is not supported by default on msvc
    target_compile_options(path_tracer PRIVATE /Zc:preprocessor)
endif ()

# TODO: not sure if this is actually how you would do it
if (UNIX)
    # enables simd extensions
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()

find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(fast-obj CONFIG REQUIRED)
find_package(Stb REQUIRED)
target_include_directories(path_tracer PRIVATE ${Stb_INCLUDE_DIR})
target_link_libraries(path_tracer PRIVATE SDL3::SDL3 imgui::imgui tomlplusplus::tomlplusplus fast-obj::fast-obj gmath)