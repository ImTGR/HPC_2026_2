cmake_minimum_required(VERSION 3.15)

project(MultiplicacionMatrices LANGUAGES C)

add_executable(matrices matrices.c)
target_compile_features(matrices PRIVATE c_std_11)

if(MSVC)
    target_compile_options(matrices PRIVATE /W4)
else()
    target_compile_options(matrices PRIVATE -Wall -Wextra -Wpedantic)
endif()
