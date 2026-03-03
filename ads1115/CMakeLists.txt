cmake_minimum_required(VERSION 3.5)

project(ads1115)

# Add the source file for the library
add_library(ads1115 SHARED src/main.c)

# We need to set the output to be a .wasm file is handled by the toolchain usually
# But for now mostly we just compile it. 
# Ensure -nostdlib is not used, but we don't link against standard libraries heavily.

# Include the current directory for healders
target_include_directories(ads1115 PRIVATE src .)
