cmake_minimum_required(VERSION 3.16)

project(c_implement)

include_directories(.)

add_executable(${PROJECT_NAME} main.c serial.c)