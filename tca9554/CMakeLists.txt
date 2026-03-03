cmake_minimum_required(VERSION 3.5)

project(tca9554)

add_library(tca9554 SHARED src/main.c)

target_include_directories(tca9554 PRIVATE src)
