cmake_minimum_required(VERSION 3.5)
add_compile_options(-fdiagnostics-color=always)
if(NOT DEFINED IDF_TARGET)
    set(IDF_TARGET "esp32")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

string(REGEX REPLACE ".*/\(.*\)" "\\1" CURDIR ${CMAKE_CURRENT_SOURCE_DIR})
project(${CURDIR})

git_describe(PROJECT_VERSION ${COMPONENT_DIR})
message("Project commit: " ${PROJECT_VERSION})