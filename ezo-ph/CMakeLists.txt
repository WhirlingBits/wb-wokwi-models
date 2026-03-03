cmake_minimum_required(VERSION 3.5)

project(ezo-ph)

add_library(ezo-ph SHARED src/main.c)

target_include_directories(ezo-ph PRIVATE src)
