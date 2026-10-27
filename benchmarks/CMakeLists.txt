set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_DOWNLOAD_DEPENDENCIES OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(benchmark_source
    URL https://codeload.github.com/google/benchmark/tar.gz/refs/tags/v1.10.0
    URL_HASH SHA256=ACC5C7C0FBED855C4AAA2D7C3C36FB82B0C15C28F4E151E3F679C2FC0AA404AF
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(benchmark_source)
configure_file(benchmark_config.h.in generated/benchmark_config.h @ONLY)
add_executable(drawer_benchmarks document_benchmark.cpp history_benchmark.cpp
    session_benchmark.cpp)
target_include_directories(drawer_benchmarks PRIVATE . "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(drawer_benchmarks PRIVATE drawer_runtime benchmark::benchmark_main)
set_target_properties(drawer_benchmarks PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
if(MSVC)
    foreach(target drawer_benchmarks benchmark benchmark_main)
        set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endforeach()
endif()
if(BUILD_TESTING)
    add_test(NAME benchmark_smoke COMMAND drawer_benchmarks
        --benchmark_filter=^DocumentEncode/100/real_time$ --benchmark_min_time=0.01s)
    set_tests_properties(benchmark_smoke PROPERTIES TIMEOUT 30 LABELS "performance"
        PASS_REGULAR_EXPRESSION "DocumentEncode/100/real_time[ ]+[0-9]")
endif()
