include(../cmake/warnings.cmake)
include(../cmake/sanitizers.cmake)

add_executable(path_tracer
        cameras.hpp
        scene.hpp
        integrators.hpp
        color.hpp
        shapes.cpp
        handle.hpp
        aggregates.hpp

        main.cpp
        cameras.cpp
        shapes.cpp
        primitives.cpp
        aggregates.cpp
)

target_compile_features(path_tracer PRIVATE cxx_std_23)

enable_warnings(path_tracer WARNINGS_AS_ERRORS_ON)

find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
target_link_libraries(path_tracer PRIVATE SDL3::SDL3 imgui::imgui gmath)