cmake_minimum_required(VERSION 3.21)

project(tiny_bin LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- C++ Standard ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Build type default ----
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---- Compiler warnings ----
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)

# ---- Server library ----
add_library(server
    src/cpp/server/server.cpp
)


target_include_directories(server PUBLIC src/cpp) 

target_compile_options(server PRIVATE
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Debug>:-O0 -g>
)


# ---- Server executable ----
add_executable(tiny_bin_server src/cpp/server/main.cpp)

target_link_libraries(tiny_bin_server PRIVATE server Threads::Threads)

target_compile_options(tiny_bin_server PRIVATE
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Debug>:-O0 -g>
)