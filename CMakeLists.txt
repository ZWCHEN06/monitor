cmake_minimum_required(VERSION 3.16)
project(index_fund_monitor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(index_fund_monitor
    src/main.cpp
    src/cli/parser.cpp
)

target_include_directories(index_fund_monitor PRIVATE include)

target_compile_options(index_fund_monitor PRIVATE
    -Wall -Wextra -Wpedantic -Werror
    -Wconversion -Wsign-conversion
    -Wshadow -Wnon-virtual-dtor
)

add_executable(test_parser
    tests/test_parser.cpp
    src/cli/parser.cpp
)

target_include_directories(test_parser PRIVATE include)

target_compile_options(test_parser PRIVATE
    -Wall -Wextra -Wpedantic -Werror
    -Wconversion -Wsign-conversion
    -Wshadow -Wnon-virtual-dtor
)