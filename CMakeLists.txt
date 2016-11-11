cmake_minimum_required(VERSION 2.6)

## This file was partially copied from the Zero project, which is derived from Shore-MT

PROJECT(foster-btree)

IF(${CMAKE_SYSTEM_PROCESSOR} MATCHES ".*64.*")
  MESSAGE (STATUS "Yes, you are on 64bit OS/CPU!")
ELSE()
  MESSAGE (FATAL_ERROR "Foster B-tree can only be built on 64bit OS/CPU. Your system seems '${CMAKE_SYSTEM_PROCESSOR}'")
  RETURN()
ENDIF()

# generate clang compilation database
set( CMAKE_EXPORT_COMPILE_COMMANDS 1 )

#if you don't want the full compiler output, remove the following line
SET(CMAKE_VERBOSE_MAKEFILE OFF)

IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
        "Avialble build options: Release RelWithDebInfo(default) Profile Debug1 Debug2 Debug3"
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

## BUILD OPTIONS
SET(CMAKE_CXX_FLAGS_DEBUG5  "-O0 -g -DDEBUG_LEVEL=5")
SET(CMAKE_CXX_FLAGS_DEBUG4  "-O0 -g -DDEBUG_LEVEL=4")
SET(CMAKE_CXX_FLAGS_DEBUG3  "-O0 -g -DDEBUG_LEVEL=3")
SET(CMAKE_CXX_FLAGS_DEBUG2  "-O0 -g -DDEBUG_LEVEL=2")
SET(CMAKE_CXX_FLAGS_DEBUG1  "-Og -g -DDEBUG_LEVEL=1")
SET(CMAKE_CXX_FLAGS_DEBUG   ${CMAKE_CXX_FLAGS_DEBUG3})
SET(CMAKE_CXX_FLAGS_PROFILE "-O3 -g -pg -finline-functions -DDEBUG_LEVEL=0 -DNDEBUG -fPIC")
SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -fexpensive-optimizations -finline-functions -DDEBUG_LEVEL=0")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g ${CMAKE_CXX_FLAGS_RELEASE}")

list(APPEND CMAKE_CXX_FLAGS "-pthread")
# -pedantic requires -Wno-long-long
SET (ALWAYS_FLAGS "-std=c++11 -Wall -Wextra -Woverloaded-virtual -fno-strict-aliasing -Wpointer-arith")
SET (YES_INLINE_FLAGS "-finline-functions")

## enabled by config option (default)
## Now try to find out the compiler arguments needed
SET(TUNE_FLAGS "-mtune=nocona")
# TUNE_FLAGS = -mtune=generic
SET(TARGET_FLAGS "-m64")
## Assembler flags
SET(AM_LDFLAGS "-m64")

SET(ALL_FLAGS "${PEDANTIC} ${TUNE_FLAGS} ${DEBUGFLAGS} ${W_WARNINGS} ${OPTFLAGS} ${MANDATORY_FLAGS} ${ALWAYS_FLAGS} ${TARGET_FLAGS} ${TEMPLATEFLAGS}")
ADD_DEFINITIONS(${ALL_FLAGS})

## Whether to use spdlog logging library
# OPTION(USE_SPDLOG "Whether to use spdlog library" true)

## end of compiler options

####################   OS/architecture #####################
SET (ARCH_LP64  "true") # so far, always assume 64 bits
IF(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  SET (Linux "true")
ENDIF()
IF (CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)" )
  MESSAGE (FATAL_ERROR "Solaris/SunOs is not supported!")
  RETURN()
ENDIF ()
IF(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  SET (MacOSX "true")
ENDIF ()
IF (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" )
  SET (x86_64 "true")
  SET (amd64 "true")
ENDIF ()
IF (CMAKE_SYSTEM_PROCESSOR STREQUAL "sparc" )
  MESSAGE (FATAL_ERROR "SPARC is not supported!")
  RETURN()
ENDIF ()
INCLUDE (TestBigEndian)
TEST_BIG_ENDIAN(WORDS_BIGENDIAN)

# Subdirectories
add_subdirectory(src)
add_subdirectory(third-party/googletest EXCLUDE_FROM_ALL)
add_subdirectory(test EXCLUDE_FROM_ALL)
add_subdirectory(bench EXCLUDE_FROM_ALL)

# add a target to generate API documentation with Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile ${CMAKE_CURRENT_BINARY_DIR}/doc/Doxyfile @ONLY)
  add_custom_target(doc
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/doc/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating Doxygen documentation" VERBATIM
  )
endif(DOXYGEN_FOUND)
