cmake_minimum_required(VERSION 3.13)
project(bk_tree_api)

set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_COMPILER /home/alexander/intel/compilers_and_libraries_2020.0.166/linux/bin/intel64/icpc)

add_executable(corrector_app src/main.cpp)
target_include_directories(corrector_app PUBLIC src/)
target_compile_options(corrector_app PUBLIC "-g")
target_link_libraries(corrector_app PocoFoundation PocoNet PocoJSON PocoUtil)