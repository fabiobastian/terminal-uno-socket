cmake_minimum_required(VERSION 4.0)
project(UnoClient C)

set(CMAKE_C_STANDARD 11)

add_executable(UnoClient src/main.c)

if (WIN32)
    target_link_libraries(UnoClient ws2_32)
endif()