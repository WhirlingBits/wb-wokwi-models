cmake_minimum_required(VERSION 3.5)

project(ads7142)

add_library(ads7142 SHARED src/main.c)

target_include_directories(ads7142 PRIVATE src)
