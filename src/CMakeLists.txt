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
)

target_compile_features(path_tracer PRIVATE cxx_std_23)
target_precompile_headers(path_tracer PRIVATE
        <print>
        <format>
        <optional>
        <vector>

        <gmath/vec.hpp>
        <gmath/mat.hpp>
        <gmath/formatting.hpp>
        <gmath/constants.hpp>
        <gmath/geometry.hpp>
        <gmath/array_algorithms.hpp>
        <gmath/transform/transform2D.hpp>
        <gmath/transform/transform3D.hpp>
)

enable_warnings(path_tracer)

find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(fast-obj CONFIG REQUIRED)
target_link_libraries(path_tracer PRIVATE SDL3::SDL3 imgui::imgui tomlplusplus::tomlplusplus fast-obj::fast-obj gmath)