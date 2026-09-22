cmake_minimum_required(VERSION 3.15)

project(MultiplicacionMatrices LANGUAGES C)

find_package(OpenMP REQUIRED)

add_executable(matrices matrices.c)
target_compile_features(matrices PRIVATE c_std_11)
target_link_libraries(matrices PRIVATE OpenMP::OpenMP_C)

if(MSVC)
    target_compile_options(matrices PRIVATE /W4)
else()
    target_compile_options(matrices PRIVATE -Wall -Wextra -Wpedantic)
endif()
