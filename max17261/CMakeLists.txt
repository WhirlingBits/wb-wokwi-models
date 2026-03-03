cmake_minimum_required(VERSION 3.5)

project(max17261)

add_library(max17261 SHARED src/main.c)

target_include_directories(max17261 PRIVATE src)
