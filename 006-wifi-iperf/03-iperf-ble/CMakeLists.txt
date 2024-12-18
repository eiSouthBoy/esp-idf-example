# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# 手动添加组件路径
set(EXTRA_COMPONENT_DIRS 
    "../components/cmd_system"
    "../components/iperf")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(iperf)
