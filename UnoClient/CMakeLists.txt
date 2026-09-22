cmake_minimum_required(VERSION 4.0)
project(UnoClient C)

set(CMAKE_C_STANDARD 11)

add_executable(UnoClient
        src/main.c
)

target_include_directories(UnoClient PRIVATE
        include
)

target_compile_definitions(UnoClient PRIVATE
        _WIN32_WINNT=0x0601
        WIN32_LEAN_AND_MEAN
)

target_link_libraries(UnoClient PRIVATE
        ws2_32
)