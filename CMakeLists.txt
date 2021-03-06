cmake_minimum_required(VERSION 3.4)

################################################################################
## General settings
################################################################################

project("Tools")
message("\n####################################################################"
    "############\n## CMakeFile for ${PROJECT_NAME}")

set(CMAKE_CXX_FLAGS "-Wall -Wextra -pedantic")#"-ftime-report")
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")


################################################################################
## Multi-configuration installation path
################################################################################

if (${CMAKE_INSTALL_PREFIX} MATCHES "^$ENV{HOME}")
    string(TOLOWER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
    string(APPEND CMAKE_INSTALL_PREFIX "/${BUILD_TYPE}")
endif()
set(LIB_BASE "${CMAKE_INSTALL_PREFIX}/lib/kgd")
message("Install path is ${CMAKE_INSTALL_PREFIX}/")


################################################################################
## Managing uneven support of std 17 filesystem
################################################################################

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9")
    set(FS_LIB stdc++fs)
    list(APPEND KGD_DEFINITIONS -DEXPERIMENTAL_STD_FILESYSTEM)
    message("> Using experimental g++ std 17 filesystem support")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  link_directories(/usr/local/opt/llvm/lib/)
  set(FS_LIB c++fs)
  message("> Using experimental clang std 17 filesystem support")
endif()


################################################################################
## Process additional pre-processor definitions
################################################################################
add_definitions(${KGD_DEFINITIONS})
message("Using additionnal pre-processor definitions: ${KGD_DEFINITIONS}")


################################################################################
## Static library
################################################################################

set(SOURCES
    "src/genotype/selfawaregenome.hpp"

    "src/random/dice.hpp"
    "src/random/random_iterator.hpp"

    "src/settings/configfile.h"
    "src/settings/configfile.cpp"
    "src/settings/mutationbounds.hpp"
    "src/settings/prettyenums.hpp"
    "src/settings/prettystreamers.hpp"

    "src/utils/proc.h"
    "src/utils/proc.cpp"
    "src/utils/utils.h"
    "src/utils/utils.cpp"
    "src/utils/assertequal.hpp"
    "src/utils/shell.hpp"
    "src/utils/functions.h"
    "src/utils/functions.cpp"
    "src/utils/indentingostream.h"
    "src/utils/indentingostream.cpp"
)
set (EXTERNALS
    "src/external/cxxopts.hpp"
    "src/external/json.hpp"

    "src/external/gaga/gaga.hpp"
    "src/external/gaga/tinypool.hpp"
    "src/external/gaga/novelty.hpp"
)
add_library(tools STATIC ${SOURCES} ${EXTERNALS})
target_link_libraries(tools ${FS_LIB})
list(APPEND NEW_CORE_LIBS ${LIB_BASE}/$<TARGET_FILE_NAME:tools>)

if (BUILD_TESTS)
    # Test self-aware genomes
    add_executable(
        sag-test
        "src/genotype/selfawaregenome_test.cpp"
    )
    target_link_libraries(sag-test tools)
endif()


################################################################################
## Options
################################################################################

option(WITH_DEBUG_INFO "Sets whether to maximize debug data collection" OFF)
message("With debug info " ${WITH_DEBUG_INFO})
if(WITH_DEBUG_INFO)
    add_definitions(-DWITH_DEBUG_INFO)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g") # Enable debug info

    # Enable small memory error detector (fuse=gold fixes linker errors)
#    set(ASAN "-fsanitize=thread")
    set(ASAN "-fsanitize=address -fsanitize=undefined")
    string(APPEND CMAKE_CXX_FLAGS " ${ASAN} -fno-omit-frame-pointer")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        string(APPEND CMAKE_CXX_FLAGS " -fuse-ld=gold")
    endif()
endif()

option(EDNA_AGGREGATORS "Whether to provide aggregation functionality (and to
    require it from derived genomes)" OFF)
message("> Aggregation ${EDNA_AGGREGATORS}")
if (EDNA_AGGREGATORS)
    add_definitions(-DWITH_EDNA_AGGREGATORS)
endif()

option(BUILD_TESTS "Sets whether to build the tests executables" OFF)
message("Build tests " ${BUILD_TESTS})

################################################################################
## Make documentation
################################################################################

option(GENERATE_HELP "Whether or not to build the help through doxygen" OFF)
message("Help generation is " ${GENERATE_HELP})

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
        message("Enabling full documentation")
        set(DOXYGEN_ENABLED_SECTIONS "internal")
        set(DOXYGEN_EXTRACT_PRIVATE YES)
    endif()

    doxygen_add_docs(docs ${SOURCES} COMMENT "Generating documentation")
    set_target_properties(docs PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON)
endif()


################################################################################
## Export configuration
################################################################################

install(TARGETS tools ARCHIVE DESTINATION lib/kgd)

# Install headers
install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/src/"
    DESTINATION include/kgd
    FILES_MATCHING
        PATTERN "*.h"
        PERMISSIONS
            OWNER_EXECUTE OWNER_READ
            GROUP_EXECUTE GROUP_READ
                          WORLD_READ
        PATTERN "*.hpp"
        PERMISSIONS
            OWNER_EXECUTE OWNER_READ
            GROUP_EXECUTE GROUP_READ
                          WORLD_READ
        PATTERN "*/external/*" EXCLUDE
)

# Force symlink dereference during installation
foreach(sym_link ${EXTERNALS})
    get_filename_component(path ${sym_link} REALPATH)
    message("Installing ${path} instead of ${sym_link}")
    install(FILES ${path}
            DESTINATION include/kgd/external
            PERMISSIONS
                OWNER_EXECUTE OWNER_READ
                GROUP_EXECUTE GROUP_READ
    )
endforeach()

if (GENERATE_HELP)
    install(
        DIRECTORY "${CMAKE_SOURCE_DIR}/docs/"
        DESTINATION docs/tools
    )
endif()

set(CONFIG "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake")
file(GENERATE
    OUTPUT ${CONFIG}
    CONTENT
"# CMake configuration settings for project ${PROJECT_NAME} (${CMAKE_BUILD_TYPE})

set(${PROJECT_NAME}_INCLUDE_DIRS ${CMAKE_INSTALL_PREFIX}/include/)

set(${PROJECT_NAME}_LIBRARIES
    ${NEW_CORE_LIBS}
    ${FS_LIB})

set(${PROJECT_NAME}_KGD_DEFINITIONS ${KGD_DEFINITIONS})
set(DEPENDENCY_GENERATOR
    ${CMAKE_INSTALL_PREFIX}/cmake/generate_dependency_data.sh)

$<$<BOOL:${EDNA_AGGREGATORS}>:add_definitions(-DWITH_EDNA_AGGREGATORS)>

set(${PROJECT_NAME}_BUILD_TYPE ${CMAKE_BUILD_TYPE})
if(NOT \${${PROJECT_NAME}_BUILD_TYPE} STREQUAL \${CMAKE_BUILD_TYPE})
    message(FATAL_ERROR \"Mismatching build version!
        Linking against '\${${PROJECT_NAME}_BUILD_TYPE}' while using '\${CMAKE_BUILD_TYPE}'\")
endif()\n"
)
install(FILES ${CONFIG} DESTINATION cmake)

set(BUILDABLES "Tools")
set(DEPENDENCY_GENERATOR ${PROJECT_SOURCE_DIR}/scripts/generate_dependency_data.sh)
foreach(proj cxxopts json gaga Tools)
    set(PROJ_DIR "${PROJECT_SOURCE_DIR}/../${proj}")
    string(TOLOWER ${proj} proj_l)
    set(DEPENDENCY_FILE "${CMAKE_CURRENT_BINARY_DIR}/${proj_l}.dependency")
    list(APPEND DEPENDENCIES ${DEPENDENCY_FILE})
    if (${proj} IN_LIST BUILDABLES)
        set(BUILD_TYPE ${CMAKE_BUILD_TYPE})
    else()
        set(BUILD_TYPE "-")
    endif()
    add_custom_command(
        OUTPUT ${DEPENDENCY_FILE}
        COMMAND ${DEPENDENCY_GENERATOR}
            ${DEPENDENCY_FILE} Tools ${BUILD_TYPE}
        WORKING_DIRECTORY ${PROJ_DIR}
        COMMENT "Generating dependency data for ${proj} in ${PROJ_DIR}"
        DEPENDS always_check
    )
    install(FILES ${DEPENDENCY_FILE} DESTINATION cmake)
endforeach()
add_custom_target(dependencies ALL DEPENDS ${DEPENDENCIES})
add_custom_command(OUTPUT always_check COMMAND cmake -E echo)
install(FILES ${DEPENDENCY_GENERATOR} DESTINATION cmake
        PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE
)
