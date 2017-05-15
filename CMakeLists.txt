cmake_minimum_required(VERSION 2.8.12)

project(paracosm)

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

if (UNIX AND NOT APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif()

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

# Gives our library file a .node extension without any "lib" prefix
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

# Essential include files to build a node addon,
# You should add this line in every CMake.js based project
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_JS_INC})

# Essential library files to link to a node addon
# You should add this line in every CMake.js based project
target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB})

if (WIN32)
    set(prefix "")
    set(suffix ".lib")
elseif(APPLE)
    set(prefix "lib")
    set(suffix ".a")
else()
    set(prefix "lib")
    set(suffix ".a")
endif()

ExternalProject_Add(jsoncpp
    GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
    GIT_TAG d7347a2623707d71f8b21b18a46a1281ab79d45c
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/jsoncpp
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR> -DCMAKE_BUILD_TYPE=Release
)
ExternalProject_Get_Property(jsoncpp install_dir)
include_directories(${install_dir}/include)
target_link_libraries(${PROJECT_NAME} ${install_dir}/lib/${prefix}jsoncpp${suffix})
add_dependencies(${PROJECT_NAME} jsoncpp)

ExternalProject_Add(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 40398d67cd3e4f74b08649eda428dc411d801fd5
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/glm
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR> -DCMAKE_BUILD_TYPE=Release
)
ExternalProject_Get_Property(glm install_dir)
include_directories(${install_dir}/include)
add_dependencies(${PROJECT_NAME} glm)
