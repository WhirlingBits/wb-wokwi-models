cmake_minimum_required(VERSION 3.5)

project(bq27441)

add_library(bq27441 SHARED src/main.c)

target_include_directories(bq27441 PRIVATE src)
