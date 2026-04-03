cmake_minimum_required(VERSION 3.22)

set(EXTRA_COMPONENT_DIRS
    "components"
    "../shared/components"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(liquid-machine)
