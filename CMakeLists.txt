cmake_minimum_required(VERSION 3.16)

project(DesktopDup)

add_definitions(-DUNICODE -D_UNICODE)
add_executable(DesktopDup WIN32 main.cpp pch.h)

target_precompile_headers(DesktopDup PRIVATE pch.h)

