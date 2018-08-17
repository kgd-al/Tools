cmake_minimum_required(VERSION 2.8)

####################################################################################################
## General settings
####################################################################################################

project("tools")
set(CMAKE_CXX_FLAGS "-Wall -Wextra -pedantic")#"-ftime-report")

if (${CMAKE_VERSION} VERSION_LESS 3.1)
    set(CMAKE_CXX_FLAGS "-std=c++17 ${CMAKE_CXX_FLAGS}")
else ()
    set(CMAKE_CXX_STANDARD 17)
endif ()

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

####################################################################################################
## Static libraries
####################################################################################################

add_library(tools STATIC
    "src/utils/utils.cpp"
    "src/utils/utils.h"
    "src/settings/configfile.cpp"
    "src/settings/prettystreamers.hpp"
    "src/settings/prettyenums.hpp"
    "src/settings/configfile.h"
    "src/random/dice.hpp"
)

set(CONFIG "${PROJECT_SOURCE_DIR}/../packages/${PROJECT_NAME}-config.cmake")
file(GENERATE
    OUTPUT ${CONFIG}
    CONTENT
"# CMake configuration settings for project ${PROJECT_NAME}
set(${PROJECT_NAME}_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/src)
set(${PROJECT_NAME}_LIBRARIES $<TARGET_FILE:tools>)\n"
)
