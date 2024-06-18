cmake_minimum_required(VERSION 3.16)
project(kilo_c)

set(CMAKE_C_STANDARD 11)

add_executable(kilo_c kilo.c
        configs.c
        configs.h)