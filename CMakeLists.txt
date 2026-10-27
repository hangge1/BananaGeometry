cmake_minimum_required(VERSION 3.24)
project(Drawer VERSION 0.1.0 LANGUAGES C CXX)
if(NOT WIN32)
    message(FATAL_ERROR "Drawer currently supports Windows only")
endif()
include(CTest)
include(FetchContent)
FetchContent_Declare(imgui_source
    URL https://codeload.github.com/ocornut/imgui/tar.gz/refs/tags/v1.91.9b
    URL_HASH SHA256=8E1BBC76C71D74FEF2FB85DB7E7CA8EBA13D6A86623C54992B60162DB554FFDB
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(lua_source
    URL https://www.lua.org/ftp/lua-5.4.8.tar.gz
    URL_HASH SHA256=4F18DDAE154E793E46EEAB727C59EF1C0C0C2B744E7B94219710D76F530629AE
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(imgui_source lua_source)

add_library(drawer_options INTERFACE)
target_compile_features(drawer_options INTERFACE cxx_std_17)
target_compile_definitions(drawer_options INTERFACE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(drawer_options INTERFACE $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /utf-8 /permissive->)

set(lua_files lapi lauxlib lbaselib lcode lcorolib lctype ldblib ldebug ldo ldump lfunc
    lgc linit liolib llex lmathlib lmem loadlib lobject lopcodes loslib lparser lstate
    lstring lstrlib ltable ltablib ltm lundump lutf8lib lvm lzio)
set(lua_sources)
foreach(file IN LISTS lua_files)
    list(APPEND lua_sources "${lua_source_SOURCE_DIR}/src/${file}.c")
endforeach()
add_library(lua STATIC ${lua_sources})
target_include_directories(lua PUBLIC "${lua_source_SOURCE_DIR}/src")

add_library(imgui STATIC "${imgui_source_SOURCE_DIR}/imgui.cpp" "${imgui_source_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_source_SOURCE_DIR}/imgui_tables.cpp" "${imgui_source_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_source_SOURCE_DIR}/backends/imgui_impl_win32.cpp" "${imgui_source_SOURCE_DIR}/backends/imgui_impl_dx11.cpp")
target_include_directories(imgui PUBLIC "${imgui_source_SOURCE_DIR}" "${imgui_source_SOURCE_DIR}/backends")
target_link_libraries(imgui PRIVATE d3d11 d3dcompiler dwmapi)

add_library(drawer_core STATIC src/core/value.cpp src/core/geometry.cpp
    src/core/history.cpp src/core/document.cpp)
target_include_directories(drawer_core PUBLIC src)
target_link_libraries(drawer_core PUBLIC drawer_options)
add_library(drawer_runtime STATIC src/runtime/session.cpp
    src/runtime/session_bindings.cpp src/runtime/detail/lua_support.cpp)
target_link_libraries(drawer_runtime PUBLIC drawer_core PRIVATE lua)
add_library(drawer_platform STATIC src/platform/document_io.cpp src/platform/file_dialog.cpp)
target_link_libraries(drawer_platform PUBLIC drawer_core PRIVATE comdlg32)
add_library(drawer_render STATIC src/render/canvas_renderer.cpp)
target_link_libraries(drawer_render PUBLIC drawer_core imgui)
add_library(drawer_app STATIC src/app/application.cpp src/app/canvas_view.cpp src/app/lua_ui.cpp)
target_link_libraries(drawer_app PUBLIC drawer_runtime PRIVATE drawer_platform drawer_render)
add_executable(drawer WIN32 src/main.cpp)
target_link_libraries(drawer PRIVATE drawer_app drawer_render d3d11 dxgi shell32)
set_target_properties(drawer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(MSVC)
    foreach(target lua imgui drawer_core drawer_runtime drawer_platform drawer_render drawer_app drawer)
        set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endforeach()
endif()
add_custom_command(TARGET drawer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/sdk" "$<TARGET_FILE_DIR:drawer>/sdk"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/cases" "$<TARGET_FILE_DIR:drawer>/cases")
if(BUILD_TESTING)
    set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(catch2_source
        URL https://codeload.github.com/catchorg/Catch2/tar.gz/refs/tags/v3.16.1
        URL_HASH SHA256=D16BD1B8B2364918BD472CB216394ACB93BDE6784890DF0D91BCCFB3F28D777D
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(catch2_source)
    configure_file(tests/test_config.h.in generated/test_config.h @ONLY)
    add_executable(drawer_tests tests/value_test.cpp tests/history_test.cpp
        tests/view_test.cpp tests/document_test.cpp tests/session_test.cpp)
    target_include_directories(drawer_tests PRIVATE tests "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_link_libraries(drawer_tests PRIVATE drawer_runtime drawer_platform Catch2::Catch2WithMain)
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
endif()
install(TARGETS drawer RUNTIME DESTINATION .)
install(DIRECTORY sdk cases docs templates DESTINATION .)
install(FILES README.md AGENTS.md DESTINATION .)
install(FILES third_party/README.md DESTINATION third_party)
install(FILES "${imgui_source_SOURCE_DIR}/LICENSE.txt" DESTINATION licenses RENAME imgui.txt)
install(FILES "${lua_source_SOURCE_DIR}/doc/readme.html" DESTINATION licenses RENAME lua.html)
