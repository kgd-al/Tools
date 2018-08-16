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
