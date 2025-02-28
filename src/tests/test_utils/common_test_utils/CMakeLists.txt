# Copyright (C) 2018-2023 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

function(add_common_utils ADD_TARGET_NAME)
    list(APPEND TARGET_EXCLUDED_SOURCE_PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )
    if(NOT ENABLE_CONFORMANCE_PGQL)
        list(APPEND TARGET_EXCLUDED_SOURCE_PATHS
            ${CMAKE_CURRENT_SOURCE_DIR}/include/common_test_utils/postgres_link.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/include/common_test_utils/postgres_helpers.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/postgres_link.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/postgres_helpers.cpp
        )
    endif()

    # create target
    addIeTarget(
            NAME ${ADD_TARGET_NAME}
            TYPE STATIC
            ROOT ${CMAKE_CURRENT_SOURCE_DIR}
            EXCLUDED_SOURCE_PATHS
                ${TARGET_EXCLUDED_SOURCE_PATHS}
            ADD_CLANG_FORMAT
            DEVELOPER_PACKAGE
                tests
            LINK_LIBRARIES
                PUBLIC
                    gtest
                    gtest_main
                    gmock
                    ngraphFunctions
                    openvino::runtime
                    openvino::runtime::dev
                PRIVATE
                    openvino::util
                    openvino::shape_inference
                INCLUDES
                    PUBLIC
                        "${CMAKE_CURRENT_SOURCE_DIR}/include"
                    PRIVATE
                        "${CMAKE_CURRENT_SOURCE_DIR}/src"

    )

    if(ENABLE_CONFORMANCE_PGQL)
        target_compile_definitions(${ADD_TARGET_NAME} PUBLIC ENABLE_CONFORMANCE_PGQL)
    endif()

    # USE_STATIC_IE is passed
    if(ARGN)
        target_link_libraries(${ADD_TARGET_NAME} PRIVATE inference_engine_s)
    endif()

    ov_build_target_faster(${ADD_TARGET_NAME}
        UNITY
        PCH PRIVATE "src/precomp.hpp"
    )

    # detecting regex support
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
        set(USE_BOOST_RE ON)
    endif()
    if (USE_BOOST_RE)
        target_compile_definitions(${ADD_TARGET_NAME} PUBLIC USE_BOOST_RE)

        debug_message(STATUS "Adding boost dependency")
        if (CMAKE_VERBOSE_MAKEFILE)
            set(Boost_DEBUG on)
        endif ()
        find_package(Boost REQUIRED COMPONENTS regex)
        target_link_libraries(${ADD_TARGET_NAME} PUBLIC ${Boost_REGEX_LIBRARY})
        target_include_directories(${ADD_TARGET_NAME} PUBLIC ${Boost_INCLUDE_DIRS})
    endif ()

    target_include_directories(${ADD_TARGET_NAME}
        PUBLIC
            $<TARGET_PROPERTY:openvino::runtime,INTERFACE_INCLUDE_DIRECTORIES>
        PRIVATE
            $<TARGET_PROPERTY:openvino::runtime::dev,INTERFACE_INCLUDE_DIRECTORIES>)
    target_include_directories(${ADD_TARGET_NAME} SYSTEM PUBLIC ${IE_TESTS_ROOT}/test_utils)

    target_compile_definitions(${ADD_TARGET_NAME} PUBLIC ${ARGN})

endfunction()

# Keep old name so that library can be used from NPU repo
set(TARGET_NAME commonTestUtils)
set(NEW_TARGET_NAME common_test_utils)

add_common_utils(${TARGET_NAME})
add_common_utils(${TARGET_NAME}_s USE_STATIC_IE)

# Add new names to use them from openvino repo
add_library(${NEW_TARGET_NAME} ALIAS ${TARGET_NAME})
add_library(${NEW_TARGET_NAME}_s ALIAS ${TARGET_NAME}_s)

add_subdirectory(tests)
