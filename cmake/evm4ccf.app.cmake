# This is not standalone - it expects to be included with options and variables configured

# Build Keccak library for use inside the enclave
file(GLOB KECCAK_SOURCES
    ${EVM_DIR}/3rdparty/keccak/*.c
)
enable_language(ASM)
add_enclave_library_c(keccak_enclave "${KECCAK_SOURCES}")


# Build eEVM library for use inside the enclave
set(EVM_CPP_FILES
  ${EVM_DIR}/src/disassembler.cpp
  ${EVM_DIR}/src/stack.cpp
  ${EVM_DIR}/src/transaction.cpp
  ${EVM_DIR}/src/util.cpp
  ${EVM_DIR}/src/processor.cpp)
add_library(enclave_evm STATIC
  ${EVM_CPP_FILES})
target_compile_definitions(enclave_evm PRIVATE
  INSIDE_ENCLAVE
  _LIBCPP_HAS_THREAD_API_PTHREAD)
# Not setting -nostdinc in order to pick up compiler specific xmmintrin.h.
target_compile_options(enclave_evm PRIVATE
  -nostdinc++
  -U__linux__)
target_include_directories(enclave_evm SYSTEM PRIVATE
  ${OE_LIBCXX_INCLUDE_DIR}
  ${OE_LIBC_INCLUDE_DIR}
  ${EVM_DIR}/include
  ${EVM_DIR}/3rdparty)
target_link_libraries(enclave_evm PRIVATE
  intx::intx)
set_property(TARGET enclave_evm PROPERTY POSITION_INDEPENDENT_CODE ON)


# Build app
add_enclave_lib(evm4ccf
  ${CMAKE_CURRENT_LIST_DIR}/../src/app/oe_sign.conf
  ${CCF_DIR}/src/apps/sample_key.pem
  SRCS
    ${CMAKE_CURRENT_LIST_DIR}/../src/app/evm_for_ccf.cpp
    ${EVM_CPP_FILES}
  INCLUDE_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${EVM_DIR}/include
  LINK_LIBS
    keccak_enclave
    intx::intx
)
