# Main CMakeLists.txt cmake file for RUNTIMEOSS project
cmake_minimum_required(VERSION 2.8)

# Set CMAKE_BUILD_TYPE to Release by default.
# Must be done before calling project()
if(CMAKE_BUILD_TYPE MATCHES "^CMAKE_BUILD_TYPE$")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "" Release Debug RelWithDebInfo MinSizeRel)
endif()

# Set BUILD_SHARED_LIBS to ON by default.
# Must be done before calling project()
if(BUILD_SHARED_LIBS MATCHES "^BUILD_SHARED_LIBS$")
    if(NOT WIN32)
        # __declspec(dllexport) are missing in runtimeoss so it's currently not possible
        # to build it as a shared library on win32
        set(BUILD_SHARED_LIBS "ON" CACHE BOOL "Build shared libraries." FORCE)
    endif()
endif()

project(runtimeoss C CXX)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-Wall -Wno-sign-compare -Wliteral-suffix" HAVE_GCC_WARNING_FLAGS)
if(HAVE_GCC_WARNING_FLAGS)
  SET(CMAKE_CXX_FLAGS "-Wall -Wno-sign-compare -Wno-literal-suffix ${CMAKE_CXX_FLAGS}")
  SET(CMAKE_C_FLAGS "-Wall -Wno-sign-compare -Wno-literal-suffix ${CMAKE_C_FLAGS}")
endif()

# C++11
set (CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
#set (CMAKE_CXX_FLAGS_DEBUG "-std=c++11 ${CMAKE_CXX_FLAGS_DEBUG}")
#set (CMAKE_CXX_FLAGS_RELEASE "-std=c++11 ${CMAKE_CXX_FLAGS_RELEASE}")

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake ${CMAKE_MODULE_PATH})

include(GitVersion)
git_version(RUNTIMEOSS 0.1.0)
set(RUNTIMEOSS_SO_VERSION 1)

# Offer the user the choice of overriding the installation directories
set(INSTALL_LIB_DIR     lib        CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR     bin        CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR include    CACHE PATH "Installation directory for header files")
set(INSTALL_DATA_DIR    share/runtimeoss CACHE PATH "Installation directory for data files")


# Make relative paths absolute (needed later on)
foreach(p LIB BIN INCLUDE DATA)
    set(var INSTALL_${p}_DIR)
    set(RELATIVE_INSTALL_${p}_DIR ${INSTALL_${p}_DIR})
    if(NOT IS_ABSOLUTE "${${var}}")
        set(${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
    endif()
endforeach(p LIB BIN INCLUDE DATA)

# CMake 2.x does not define MSVC when using Intel compiler on Windows, see
# http://public.kitware.com/Bug/view.php?id=14476
if ( WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Intel" )
  set(WINTEL TRUE)
  set(CMAKE_C_FLAGS "/Qstd=c99 ${CMAKE_C_FLAGS}")
endif()

# ============================
# MPI
# ============================
include(IntelMPI)
find_package(MPI REQUIRED)

#This is required by Intel MPI and harmless with other MPI
add_definitions(-DMPICH_IGNORE_CXX_SEEK)

#OpenMPI CXX binding seems to break python (also needed to work on OSX)
add_definitions(-DOMPI_SKIP_MPICXX)

include_directories(${MPI_INCLUDE_PATH})
link_directories(${MPI_LIBRARY_DIRS})

# ========================
# C
# ========================
include(CheckIncludeFile)
check_include_file("unistd.h" HAVE_UNISTD_H)

# ========================
# Configuration file
# ========================
configure_file("${PROJECT_SOURCE_DIR}/CMake/config.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.h")
configure_file("${PROJECT_SOURCE_DIR}/CMake/runtimeoss-config.h.in" "${CMAKE_CURRENT_BINARY_DIR}/runtimeoss/config.h")

include_directories(${CMAKE_CURRENT_BINARY_DIR})

# ============================
# Generation des bibliotheques
# ============================

# Sources
file(GLOB_RECURSE RUNTIMEOSS_SOURCES RELATIVE ${PROJECT_SOURCE_DIR} runtime/*.*pp)

link_directories(${PROJECT_BINARY_DIR})

add_definitions(-D_GNU_SOURCE)
#  We want to enforce assertions even in Release mode
add_definitions(-UNDEBUG)

if (MSVC)
    add_definitions(-D__func__=__FUNCTION__)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif (MSVC)

# lib RUNTIMEOSS
add_library(runtimeoss ${RUNTIMEOSS_SOURCES})
set_target_properties(runtimeoss PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS} -Wl,-undefined,warning" COMPILE_DEFINITIONS "${RUNTIMEOSS_DEFINITIONS}")
set_target_properties(runtimeoss PROPERTIES VERSION ${RUNTIMEOSS_VERSION} SOVERSION ${RUNTIMEOSS_SO_VERSION})
set_target_properties(runtimeoss PROPERTIES INSTALL_NAME_DIR ${INSTALL_LIB_DIR}    )

if(BUILD_SHARED_LIBS )
    # Programs using runtimeoss do not need to link directly with starpu, so we strip all second level
    # dependencies.
    set(_LINK_PRIVATE LINK_PRIVATE)
    set(_LINK_PUBLIC LINK_PUBLIC)
endif()

target_link_libraries(runtimeoss
            ${MPI_C_LIBRARIES}
            ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})

install(TARGETS runtimeoss
    EXPORT    RUNTIMEOSSLibraryDepends
    RUNTIME DESTINATION "${RELATIVE_INSTALL_BIN_DIR}" COMPONENT Runtime
    LIBRARY DESTINATION "${RELATIVE_INSTALL_LIB_DIR}" COMPONENT Runtime
    ARCHIVE DESTINATION "${RELATIVE_INSTALL_LIB_DIR}" COMPONENT Development
    )

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/runtimeoss DESTINATION "${INSTALL_INCLUDE_DIR}" COMPONENT Development)
install(FILES runtime/dependencies.hpp;runtime/data.hpp;runtime/disk.hpp;runtime/lru.hpp;runtime/mpi.hpp;runtime/scheduler.hpp;runtime/task.hpp;runtime/task_timeline.hpp;runtime/worker.hpp DESTINATION "${INSTALL_INCLUDE_DIR}/runtimeoss" COMPONENT Development)

# Examples
include_directories(
  ${PROJECT_SOURCE_DIR}/runtime/
  ${PROJECT_SOURCE_DIR}/
  )

# To install, for example, MSVC runtime libraries:
######include (InstallRequiredSystemLibraries)

# To install other/non-system 3rd party required libraries:
#install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/ FixBundle.cmake)
#

#
# BUILD-TREE
#
# Add all targets to the build-tree export set
export(TARGETS runtimeoss ${RUNTIMEOSS_EXPORT_DEPS} FILE "${PROJECT_BINARY_DIR}/RUNTIMEOSSLibraryDepends.cmake")

# Create a RUNTIMEOSSConfig.cmake file for use from the build tree
set(RUNTIMEOSS_INCLUDE_DIRS
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_BINARY_DIR}")
set(RUNTIMEOSS_LIB_DIR "${PROJECT_BINARY_DIR}")
set(RUNTIMEOSS_CMAKE_DIR "${PROJECT_BINARY_DIR}")

set(RUNTIMEOSS_DEFINITIONS HAVE_RUNTIME)

#
# INSTALL-TREE
#
# Install the export set for use with the install-tree
install(EXPORT RUNTIMEOSSLibraryDepends DESTINATION
    "${RELATIVE_INSTALL_DATA_DIR}/CMake"
    COMPONENT    Development)

# Create a RUNTIMEOSSConfig.cmake file for the use from the install tree
# and install it
set(RUNTIMEOSS_CMAKE_DIR "${INSTALL_DATA_DIR}/CMake")
file(RELATIVE_PATH rel_include_dir "${RUNTIMEOSS_CMAKE_DIR}" "${INSTALL_INCLUDE_DIR}")
list(APPEND RELATIVE_RUNTIMEOSS_INCLUDE_DIRS ${rel_include_dir})

file(RELATIVE_PATH rel_lib_dir "${RUNTIMEOSS_CMAKE_DIR}" "${INSTALL_LIB_DIR}")
list(APPEND RELATIVE_RUNTIMEOSS_LIB_DIR ${rel_lib_dir})

configure_file(CMake/RUNTIMEOSSConfig.cmake.in "${PROJECT_BINARY_DIR}/InstallFiles/RUNTIMEOSSConfig.cmake" @ONLY)
install(FILES
    "${PROJECT_BINARY_DIR}/InstallFiles/RUNTIMEOSSConfig.cmake"
    DESTINATION "${RUNTIMEOSS_CMAKE_DIR}" COMPONENT    Development)

# ========================
# final LOG
# ========================
include(FeatureSummary)
feature_summary(WHAT ALL)
