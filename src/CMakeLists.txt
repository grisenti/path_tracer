include(../cmake/warnings.cmake)
include(../cmake/sanitizers.cmake)

add_executable(path_tracer
        cameras.hpp
        cameras.cpp
        scene.hpp
        scene.cpp
        integrators.hpp
        integrators.cpp
        color.hpp
        shapes.hpp
        shapes.cpp
        primitives.hpp
        primitives.cpp
        handle.hpp
        aggregates.hpp
        aggregates.cpp
        utils.hpp
        utils.cpp
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
)

target_compile_features(path_tracer PRIVATE cxx_std_23)
target_precompile_headers(path_tracer PRIVATE
        <print>
        <format>
        <optional>
        <vector>
        <span>
        <gmath/gmath.hpp>
)

enable_warnings(path_tracer)

if (MSVC)
    # since __VA_OPT__ is not supported by default on msvc
    target_compile_options(path_tracer PRIVATE /Zc:preprocessor)
endif ()

find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(fast-obj CONFIG REQUIRED)
target_link_libraries(path_tracer PRIVATE SDL3::SDL3 imgui::imgui tomlplusplus::tomlplusplus fast-obj::fast-obj gmath)