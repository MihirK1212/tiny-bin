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
pkg_check_modules(LIBPQXX REQUIRED IMPORTED_TARGET libpqxx)

include(FetchContent)
FetchContent_Declare(
    cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    GIT_TAG v3.2.0
)
FetchContent_MakeAvailable(cxxopts)

# ---- Server library ----
add_library(server
    src/cpp/server/socket_utils.cpp
    src/cpp/server/server.cpp
    src/cpp/server/worker.cpp
    src/cpp/server/event_loop.cpp
)


target_include_directories(server PUBLIC src/cpp) 

target_compile_options(server PRIVATE
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Debug>:-O0 -g>
)


# ---- Server executable ----
add_executable(tiny_bin_server
    src/cpp/server/main.cpp
    src/cpp/server/app/db/connection.cpp
)

target_link_libraries(tiny_bin_server PRIVATE server cxxopts::cxxopts Threads::Threads PkgConfig::LIBPQXX)

target_compile_options(tiny_bin_server PRIVATE
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Debug>:-O0 -g>
)