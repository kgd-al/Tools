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
## Managing g++ support of std 17 filesystem
####################################################################################################

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9")
    set(FS_LIB stdc++fs)
    message(STATUS "> Using experimental std 17 filesystem support")
  endif()
endif()

####################################################################################################
## Static library
####################################################################################################

set(SOURCES
    "src/utils/utils.cpp"
    "src/settings/configfile.cpp"
    "src/utils/utils.h"
    "src/settings/prettystreamers.hpp"
    "src/settings/prettyenums.hpp"
    "src/settings/configfile.h"
    "src/settings/mutationbounds.hpp"
    "src/random/dice.hpp"
)
set (EXTERNALS
    "src/external/cxxopts.hpp"
    "src/external/json.hpp"
)
add_library(tools STATIC ${SOURCES} ${EXTERNALS})
target_link_libraries(tools ${FS_LIB})


####################################################################################################
## Make documentation
####################################################################################################

cmake_policy(SET CMP0057 NEW)
find_package(Doxygen REQUIRED dot)

set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/docs")

set(DOXYGEN_ENABLED_SECTIONS "internal")
set(DOXYGEN_EXTRACT_PRIVATE YES)

doxygen_add_docs(docs ${SOURCES} COMMENT "Generating documentation")


####################################################################################################
## Export configuration
####################################################################################################

install(TARGETS tools ARCHIVE DESTINATION lib/kgd)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/src/"
        DESTINATION include/kgd
        FILES_MATCHING
            PATTERN "*.h"
            PERMISSIONS
                OWNER_EXECUTE OWNER_READ
                GROUP_EXECUTE GROUP_READ
            PATTERN "*.hpp"
            PERMISSIONS
                OWNER_EXECUTE OWNER_READ
                GROUP_EXECUTE GROUP_READ
)

set(CONFIG "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake")
file(GENERATE
    OUTPUT ${CONFIG}
    CONTENT
"# CMake configuration settings for project ${PROJECT_NAME} (${CMAKE_BUILD_TYPE})

set(${PROJECT_NAME}_INCLUDE_DIRS ${CMAKE_INSTALL_PREFIX}/include/)

set(${PROJECT_NAME}_LIBRARIES
    ${FS_LIB}
    ${CMAKE_INSTALL_PREFIX}/lib/kgd/$<TARGET_FILE_NAME:tools>)\n"
)
install(FILES ${CONFIG} DESTINATION cmake)
