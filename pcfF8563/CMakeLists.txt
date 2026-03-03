cmake_minimum_required(VERSION 3.5)

project(pcf8563)

add_library(pcf8563 SHARED src/main.c)

target_include_directories(pcf8563 PRIVATE src)
