cmake_minimum_required(VERSION 3.16)

project(DesktopDup)

add_definitions(-DUNICODE -D_UNICODE)
add_executable(DesktopDup WIN32 main.cpp pch.h)

target_precompile_headers(DesktopDup PRIVATE pch.h)

add_custom_target(
    copy_shader_files
    ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/shaders.hlsl ${CMAKE_BINARY_DIR}
)
add_dependencies(DesktopDup copy_shader_files)