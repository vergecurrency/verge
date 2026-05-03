set(CMAKE_SYSTEM_NAME @cmake_system_name@)

if(NOT DEFINED CMAKE_SYSTEM_PROCESSOR)
    if("@target@" MATCHES "^i686")
        set(CMAKE_SYSTEM_PROCESSOR x86)
    elseif("@target@" MATCHES "^(x86_64|amd64)")
        set(CMAKE_SYSTEM_PROCESSOR x86_64)
    elseif("@target@" MATCHES "^(aarch64|arm64)")
        set(CMAKE_SYSTEM_PROCESSOR arm64)
    elseif("@target@" MATCHES "^arm")
        set(CMAKE_SYSTEM_PROCESSOR arm)
    endif()
endif()

set(CMAKE_C_COMPILER @target@-gcc)
set(CMAKE_CXX_COMPILER @target@-g++)
set(CMAKE_FIND_ROOT_PATH @host_prefix@)

set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

set(CMAKE_FIND_ROOT_PATH @host_prefix@)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CMAKE_FIND_ROOT_PATH @host_prefix@;@wmf_libs@)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(TARGET_SYSROOT @host_prefix@/native/SDK)
    set(CMAKE_SYSROOT ${TARGET_SYSROOT})
    set(CMAKE_OSX_SYSROOT ${TARGET_SYSROOT})

    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_COMPILER clang++)

    set(CMAKE_C_FLAGS "@cmake_c_flags@")
    set(CMAKE_CXX_FLAGS "@cmake_cxx_flags@")
    set(CMAKE_OBJC_FLAGS "@cmake_c_flags@")
    set(CMAKE_OBJCXX_FLAGS "@cmake_cxx_flags@")
    set(CMAKE_EXE_LINKER_FLAGS "@cmake_ld_flags@")
    set(CMAKE_MODULE_LINKER_FLAGS "@cmake_ld_flags@")
    set(CMAKE_SHARED_LINKER_FLAGS "@cmake_ld_flags@")
    SET(CMAKE_OSX_DEPLOYMENT_TARGET "14.0")

    set(CMAKE_INSTALL_NAME_TOOL @target@-install_name_tool)
endif()
