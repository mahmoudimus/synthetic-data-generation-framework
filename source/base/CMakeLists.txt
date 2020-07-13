cmake_minimum_required(VERSION 3.1)

set (CMAKE_CXX_STANDARD 11)

project(sgfbase CXX)

add_library(sgfbase STATIC source/fileutils.cpp  source/risaac.cpp	source/rng.cpp  source/store.cpp)

include_directories(include)

target_link_libraries(sgfbase m)

install(TARGETS sgfbase DESTINATION lib)
install(FILES GLOB *.h DESTINATION include)
