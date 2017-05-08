cmake_minimum_required(VERSION 2.8)

project(ProceduralWorld)

include(ExternalProject)

set(${PROJECT_NAME}_VERSION_MAJOR 0 CACHE STRING "major version" FORCE)
set(${PROJECT_NAME}_VERSION_MINOR 1 CACHE STRING "minor version" FORCE)
set(${PROJECT_NAME}_VERSION ${${PROJECT_NAME}_VERSION_MAJOR}.${${PROJECT_NAME}_VERSION_MINOR} CACHE STRING "version" FORCE)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SOURCE_FILES 
    "src/*.cc" 
    "src/*.h"
    "src/noise/*.cc" 
    "src/noise/*.h"
)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

# Gives our library file a .node extension without any "lib" prefix
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

# Essential include files to build a node addon,
# You should add this line in every CMake.js based project
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_JS_INC})

# Essential library files to link to a node addon
# You should add this line in every CMake.js based project
target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB})

ExternalProject_Add(jsoncpp
    GIT_REPOSITORY git@github.com:open-source-parsers/jsoncpp.git
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/jsoncpp
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
)

ExternalProject_Get_Property(jsoncpp install_dir)
include_directories(${install_dir}/include)
add_dependencies(${PROJECT_NAME} jsoncpp)
target_link_libraries(${PROJECT_NAME} ${install_dir}/lib/jsoncpp.lib)