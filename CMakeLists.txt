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
    "src/genotype/selfawaregenome.hpp"

    "src/random/dice.hpp"

    "src/settings/configfile.h"
    "src/settings/configfile.cpp"
    "src/settings/mutationbounds.hpp"
    "src/settings/prettyenums.hpp"
    "src/settings/prettystreamers.hpp"

    "src/utils/utils.h"
    "src/utils/utils.cpp"
    "src/utils/shell.hpp"
)
set (EXTERNALS
    "src/external/cxxopts.hpp"
    "src/external/json.hpp"
)
add_library(tools STATIC ${SOURCES} ${EXTERNALS})
target_link_libraries(tools ${FS_LIB})


if (BUILD_TESTS)
    # Test self-aware genomes
    add_executable(
        sag-test
        "src/genotype/selfawaregenome_test.cpp"
    )
    target_link_libraries(sag-test tools)
endif()


####################################################################################################
## Options
####################################################################################################

message(STATUS "")

option(WITH_DEBUG_INFO "Sets whether to maximize debug data collection" OFF)
message(STATUS "With debug info " ${WITH_DEBUG_INFO})
if(WITH_DEBUG_INFO)
    add_definitions(-DWITH_DEBUG_INFO)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g") # Enable debug info

    # Enable small memory error detector (fuse=gold fixes linker errors)
#    set(ASAN "-fsanitize=thread")
    set(ASAN "-fsanitize=address -fsanitize=undefined")
    set(CMAKE_CXX_FLAGS "${ASAN} -fno-omit-frame-pointer -fuse-ld=gold ${CMAKE_CXX_FLAGS}")
endif()

option(BUILD_TESTS "Sets whether to build the tests executables" OFF)
message(STATUS "Build tests " ${BUILD_TESTS})

####################################################################################################
## Make documentation
####################################################################################################

option(GENERATE_HELP "Whether or not to build the help through doxygen" OFF)
message(STATUS "Help generation is " ${GENERATE_HELP})

if (GENERATE_HELP)
    cmake_policy(SET CMP0057 NEW)
    find_package(Doxygen REQUIRED dot)

    set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/docs")

    set(DOXYGEN_EXAMPLE_PATH "src/")

    # Qt help
    set(DOXYGEN_GENERATE_QHP YES)
    set(DOXYGEN_QCH_FILE "../tools.qch")
    set(DOXYGEN_QHP_NAMESPACE "kgd.tools")
    set(DOXYGEN_QHP_VIRTUAL_FOLDER "kgd_docs")
    set(DOXYGEN_QHG_LOCATION "qhelpgenerator")

    # Debugging
    if (CMAKE_BUILD_TYPE MATCHES Debug)
        message(STATUS "Enabling full documentation")
        set(DOXYGEN_ENABLED_SECTIONS "internal")
        set(DOXYGEN_EXTRACT_PRIVATE YES)
    endif()

    doxygen_add_docs(docs ${SOURCES} COMMENT "Generating documentation")
endif()


####################################################################################################
## Export configuration
####################################################################################################

install(TARGETS tools ARCHIVE DESTINATION lib/kgd)

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/src/"
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

if (GENERATE_HELP)
    install(
        DIRECTORY "${CMAKE_SOURCE_DIR}/docs/"
        DESTINATION docs/tools
    )
endif()

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
