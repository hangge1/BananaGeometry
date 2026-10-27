set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(catch2_source
    URL https://codeload.github.com/catchorg/Catch2/tar.gz/refs/tags/v3.16.1
    URL_HASH SHA256=D16BD1B8B2364918BD472CB216394ACB93BDE6784890DF0D91BCCFB3F28D777D
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(catch2_source)
configure_file(test_config.h.in generated/test_config.h @ONLY)
add_executable(drawer_tests value_test.cpp history_test.cpp
    view_test.cpp document_test.cpp session_test.cpp interaction_test.cpp)
target_include_directories(drawer_tests PRIVATE . "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(drawer_tests PRIVATE drawer_runtime drawer_platform Catch2::Catch2WithMain)
set_target_properties(drawer_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
if(MSVC)
    foreach(target drawer_tests Catch2 Catch2WithMain)
        set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endforeach()
endif()
list(APPEND CMAKE_MODULE_PATH "${catch2_source_SOURCE_DIR}/extras")
include(Catch)
catch_discover_tests(drawer_tests TEST_PREFIX "unit::" PROPERTIES LABELS "unit" TIMEOUT 60)
add_test(NAME window_smoke COMMAND drawer --smoke-test)
set_tests_properties(window_smoke PROPERTIES TIMEOUT 30 LABELS "integration")
