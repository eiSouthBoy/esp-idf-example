cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "../components/protocol_examples_common")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(mqtt_tcp)