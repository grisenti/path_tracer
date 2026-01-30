include(../cmake/warnings.cmake)
include(../cmake/sanitizers.cmake)

add_executable(path_tracer
        main.cpp
)

target_compile_features(path_tracer PRIVATE cxx_std_23)

enable_warnings(path_tracer WARNINGS_AS_ERRORS_ON)

target_link_libraries(path_tracer gmath)