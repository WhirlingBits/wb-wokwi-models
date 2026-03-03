cmake_minimum_required(VERSION 3.5)

project(r2221)

add_library(r2221 SHARED src/main.c)

target_include_directories(r2221 PRIVATE src)
