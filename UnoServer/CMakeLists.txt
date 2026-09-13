cmake_minimum_required(VERSION 4.0)
project(UnoServer C)

set(CMAKE_C_STANDARD 11)

add_executable(UnoServer
        src/main.c
        src/structs.c
)

target_include_directories(UnoServer PRIVATE
        include
)