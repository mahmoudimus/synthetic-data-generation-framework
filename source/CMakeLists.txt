cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_STANDARD 11)

project(sgf CXX)

add_subdirectory(base)

include_directories(base/include)

include_directories(include)

add_executable(sgfinit init.cpp)
add_executable(sgfgen gen.cpp)
add_executable(sgfextract extract.cpp)

install(TARGETS sgfinit sgfgen sgfextract DESTINATION bin)

target_link_libraries(sgfinit sgfbase)
target_link_libraries(sgfgen sgfbase)
target_link_libraries(sgfextract sgfbase)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX) # extra warnings if using gcc/g++
	target_compile_options(sgfgen PRIVATE -Wextra -Wall)
endif()
