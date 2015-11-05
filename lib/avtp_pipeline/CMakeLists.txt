cmake_minimum_required ( VERSION 2.6 ) 
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
project ( AVB ) 

# point to AVB SRC directory
set ( AVB_SRC_DIR ${CMAKE_SOURCE_DIR} )

# point to HAL directory
set ( AVB_HAL_DIR ${AVB_SRC_DIR}/platform/${OPENAVB_HAL} )

# point to OSAL directory
set ( AVB_OSAL_DIR ${AVB_SRC_DIR}/platform/${OPENAVB_OSAL} )

# point to TCAL directory
set ( AVB_TCAL_DIR ${AVB_SRC_DIR}/platform/platTCAL/${OPENAVB_TCAL} )

# Directory to install binaries to
set ( AVB_INSTALL_DIR ${CMAKE_BINARY_DIR}/bin )

# CORE_TODO: There may be additional common CMakeLists.txt functionality that can be migrated out of the platform specific area.

# CMake is happier when the CMakeLists.txt is in a top level directory working down. Therefore the platform specific CMakeLists.txt
# is included here. The common CMake command uage will take this form below when run from a build directory that is at the same 
# directory level as the repo:
# cmake -DCMAKE_TOOLCHAIN_FILE=../<avb_repo>/platform/<platform name>/<target>.cmake -DCMAKE_BUILD_TYPE=Release ../<avb_repo>
# for example:
# cmake -DCMAKE_TOOLCHAIN_FILE=../avbrepo/platform/Linux/x86_i210_linux.cmake -DCMAKE_BUILD_TYPE=Release ../avbrepo

# Used to hold lists of source files from common code subdirectories
SET (SRC_FILES "")

# Suppress the policy warning when including a CMakeLists.txt file
if(POLICY CMP0011)
  cmake_policy(SET CMP0011 OLD)
endif(POLICY CMP0011)

include(${AVB_OSAL_DIR}/CMakeLists.txt)



