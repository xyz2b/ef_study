cmake_minimum_required(VERSION 3.19)
project(ef)

set(CMAKE_C_STANDARD 99)

ENABLE_LANGUAGE(ASM)

add_executable(ef main.c coroutine.c epoll.c fiber.c framework.c amd64/fiber.s)

