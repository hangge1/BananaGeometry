cmake_minimum_required(VERSION 3.24)
project(Drawer VERSION 0.1.0 LANGUAGES C CXX)
if(NOT WIN32)
    message(FATAL_ERROR "Drawer currently supports Windows only")
endif()
include(CTest)
include(FetchContent)
option(DRAWER_BUILD_BENCHMARKS "Build developer performance measurements" OFF)
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

add_library(drawer_core STATIC src/core/value.cpp src/core/geometry.cpp src/core/bezier.cpp
    src/core/history.cpp src/core/document.cpp)
target_include_directories(drawer_core PUBLIC src)
target_link_libraries(drawer_core PUBLIC drawer_options)
add_library(drawer_runtime STATIC src/runtime/session.cpp
    src/runtime/session_bindings.cpp src/runtime/session_feedback.cpp src/runtime/detail/lua_support.cpp)
target_link_libraries(drawer_runtime PUBLIC drawer_core PRIVATE lua)
add_library(drawer_platform STATIC src/platform/document_io.cpp src/platform/file_dialog.cpp
    src/platform/desktop_actions.cpp src/platform/preferences.cpp)
target_link_libraries(drawer_platform PUBLIC drawer_core PRIVATE comdlg32 shell32 ole32 uuid)
add_library(drawer_render STATIC src/render/canvas_renderer.cpp src/render/text_metrics.cpp)
target_link_libraries(drawer_render PUBLIC drawer_core imgui)
add_library(drawer_app STATIC src/app/application.cpp src/app/canvas_view.cpp src/app/lua_ui.cpp
    src/app/workspace_paths.cpp src/app/case_library.cpp src/app/canvas_feedback.cpp)
target_link_libraries(drawer_app PUBLIC drawer_runtime PRIVATE drawer_platform drawer_render)
add_executable(drawer WIN32 src/main.cpp)
target_link_libraries(drawer PRIVATE drawer_app imgui d3d11 dxgi shell32)
set_target_properties(drawer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(MSVC)
    foreach(target lua imgui drawer_core drawer_runtime drawer_platform drawer_render drawer_app drawer)
        set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endforeach()
endif()
add_custom_command(TARGET drawer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/sdk" "$<TARGET_FILE_DIR:drawer>/sdk"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/cases" "$<TARGET_FILE_DIR:drawer>/cases")
include(cmake/ModuleRules.cmake)
drawer_check_dependencies(drawer_core drawer_options)
drawer_check_dependencies(drawer_runtime drawer_core)
drawer_check_dependencies(drawer_platform drawer_core)
drawer_check_dependencies(drawer_render drawer_core)
drawer_check_dependencies(drawer_app drawer_runtime drawer_platform drawer_render)
drawer_check_dependencies(drawer drawer_app)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
if(DRAWER_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
install(TARGETS drawer RUNTIME DESTINATION .)
install(DIRECTORY sdk cases docs templates DESTINATION .)
install(FILES README.md AGENTS.md DESTINATION .)
install(FILES third_party/README.md DESTINATION third_party)
install(FILES benchmarks/README.md DESTINATION benchmarks)
install(FILES "${imgui_source_SOURCE_DIR}/LICENSE.txt" DESTINATION licenses RENAME imgui.txt)
install(FILES "${lua_source_SOURCE_DIR}/doc/readme.html" DESTINATION licenses RENAME lua.html)
