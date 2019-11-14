cmake_minimum_required(VERSION 3.11)

add_definitions(-DOE_API_VERSION=2)

# Paths to dependencies - currently explicit
set(CCF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/CCF)
set(EVM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/eEVM)

include(${CCF_DIR}/cmake/preproject.cmake)

project(evm_for_ccf C CXX)

include(${CCF_DIR}/cmake/common.cmake)

# TODO: This should not be duplicated here. Ideally it would be exposed independently
# as an optional target, but this requires significant CMake refactoring. When member
# commands are signed from Python in the CCF test infrastructure, this can be removed
# MemberClient executable
add_executable(memberclient ${CCF_DIR}/src/clients/memberclient.cpp)
use_client_mbedtls(memberclient)
target_link_libraries(memberclient PRIVATE
  ${CMAKE_THREAD_LIBS_INIT}
  ccfcrypto.host
  secp256k1.host
)

add_subdirectory(${EVM_DIR}/3rdparty)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tests/tests.sh ${CMAKE_CURRENT_BINARY_DIR}/tests.sh COPYONLY)

option(RECORD_TRACE "Record a detailed trace of EVM execution when transaction fails" OFF)
if(RECORD_TRACE)
  add_definitions(-DRECORD_TRACE)
endif(RECORD_TRACE)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/evm4ccf.app.cmake)

option(BUILD_TESTS "Build tests" ON)
option(BUILD_SAMPLES "Build samples" ON)

if(BUILD_TESTS)
  enable_testing()
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/evm4ccf.tests.cmake)
endif()
