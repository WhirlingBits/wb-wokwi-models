cmake_minimum_required(VERSION 3.5)

project(ezo-rtd)

add_library(ezo-rtd SHARED src/main.c)

target_include_directories(ezo-rtd PRIVATE src)
