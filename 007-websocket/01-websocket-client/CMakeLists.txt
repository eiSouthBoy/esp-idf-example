cmake_minimum_required(VERSION 3.16)

# 手动添加组件路径
set(EXTRA_COMPONENT_DIRS 
    "./components/protocol_examples_common")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(websocket_client)