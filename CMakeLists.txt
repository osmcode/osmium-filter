#-----------------------------------------------------------------------------
#
#  CMake Config
#
#  Osmium Filter
#
#-----------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Remove the warning that WIN32 is not defined in Cygwin any more.
set(CMAKE_LEGACY_CYGWIN_WIN32 0)


#-----------------------------------------------------------------------------
#
#  Project version
#
#-----------------------------------------------------------------------------

# It is important that this setting remains before the "project" call.
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;MinSizeRel;Dev"
    CACHE STRING
    "List of available configuration types"
    FORCE)

project(osmium-filter)

set(OSMIUM_FILTER_VERSION_MAJOR 0)
set(OSMIUM_FILTER_VERSION_MINOR 0)
set(OSMIUM_FILTER_VERSION_PATCH 1)

set(OSMIUM_FILTER_VERSION ${OSMIUM_FILTER_VERSION_MAJOR}.${OSMIUM_FILTER_VERSION_MINOR}.${OSMIUM_FILTER_VERSION_PATCH})

set(AUTHOR "Jochen Topf <jochen@topf.org>")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


#-----------------------------------------------------------------------------
#
#  Find external dependencies
#
#-----------------------------------------------------------------------------

find_package(Boost 1.55.0 REQUIRED COMPONENTS program_options)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

find_package(Osmium 2.9.0 REQUIRED COMPONENTS io)
include_directories(SYSTEM ${OSMIUM_INCLUDE_DIRS})


#-----------------------------------------------------------------------------
#
#  Optional "cppcheck" target that checks C++ code
#
#-----------------------------------------------------------------------------
message(STATUS "Looking for cppcheck")
find_program(CPPCHECK cppcheck)

if(CPPCHECK)
    message(STATUS "Looking for cppcheck - found")
    set(CPPCHECK_OPTIONS --enable=warning,style,performance,portability,information,missingInclude)

    # cpp doesn't find system includes for some reason, suppress that report
    set(CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} --suppress=missingIncludeSystem)

    add_custom_target(cppcheck ${CPPCHECK} --std=c++11 ${CPPCHECK_OPTIONS} ${CMAKE_SOURCE_DIR}/src/*pp)
else()
    message(STATUS "Looking for cppcheck - not found")
    message(STATUS "  Build target 'cppcheck' will not be available")
endif()


#-----------------------------------------------------------------------------
#
#  Optional "iwyu" target to check headers
#  http://include-what-you-use.org/
#
#-----------------------------------------------------------------------------
find_program(IWYU_TOOL iwyu_tool.py)

if(IWYU_TOOL)
    message(STATUS "Looking for iwyu_tool.py - found")
    add_custom_target(iwyu ${IWYU_TOOL} -p ${CMAKE_BINARY_DIR} -- --mapping_file=${CMAKE_SOURCE_DIR}/iwyu.imp)
else()
    message(STATUS "Looking for iwyu_tool.py - not found")
    message(STATUS "  Make target 'iwyu' will not be available")
endif()


#-----------------------------------------------------------------------------
#
#  Decide which C++ version to use (Minimum/default: C++11).
#
#-----------------------------------------------------------------------------
if(NOT MSVC)
    if(NOT USE_CPP_VERSION)
        set(USE_CPP_VERSION c++11)
    endif()
    message(STATUS "Use C++ version: ${USE_CPP_VERSION}")
    # following only available from cmake 2.8.12:
    #   add_compile_options(-std=${USE_CPP_VERSION})
    # so using this instead:
    add_definitions(-std=${USE_CPP_VERSION})
endif()


#-----------------------------------------------------------------------------
#
#  Compiler and Linker flags
#
#-----------------------------------------------------------------------------
if(MSVC)
    set(USUAL_COMPILE_OPTIONS "/Ox")
else()
    set(USUAL_COMPILE_OPTIONS "-O3 -g")
endif()

if(WIN32)
    add_definitions(-DWIN32 -D_WIN32 -DMSWIN32 -DBGDWIN32
                    -DWINVER=0x0500 -D_WIN32_WINNT=0x0500 -D_WIN32_IE=0x0600)
endif()

set(CMAKE_CXX_FLAGS_DEV "${USUAL_COMPILE_OPTIONS}"
    CACHE STRING "Flags used by the compiler during developer builds."
    FORCE)

set(CMAKE_EXE_LINKER_FLAGS_DEV ""
    CACHE STRING "Flags used by the linker during developer builds."
    FORCE)
mark_as_advanced(
    CMAKE_CXX_FLAGS_DEV
    CMAKE_EXE_LINKER_FLAGS_DEV
)

set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${USUAL_COMPILE_OPTIONS}"
    CACHE STRING "Flags used by the compiler during RELWITHDEBINFO builds."
    FORCE)


#-----------------------------------------------------------------------------
#
#  Build Type
#
#-----------------------------------------------------------------------------

# In 'Dev' mode: compile with very strict warnings and turn them into errors.
if(CMAKE_BUILD_TYPE STREQUAL "Dev")
    if(NOT MSVC)
        add_definitions(-Werror -Wno-unused-variable)
    endif()
#    add_definitions(${OSMIUM_WARNING_OPTIONS})
endif()

# Force RelWithDebInfo build type if none was given
if(CMAKE_BUILD_TYPE)
    set(build_type ${CMAKE_BUILD_TYPE})
else()
    set(build_type "RelWithDebInfo")
endif()

set(CMAKE_BUILD_TYPE ${build_type}
    CACHE STRING
    "Choose the type of build, options are: ${CMAKE_CONFIGURATION_TYPES}."
    FORCE)


#-----------------------------------------------------------------------------

include_directories(include)
add_subdirectory(src)

enable_testing()
add_subdirectory(test)


#-----------------------------------------------------------------------------
