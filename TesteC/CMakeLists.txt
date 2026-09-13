cmake_minimum_required(VERSION 4.0)
project(TesteC C)

set(CMAKE_C_STANDARD 11)

add_executable(TesteC main.c
        calculos.h
        calculos.c
)
