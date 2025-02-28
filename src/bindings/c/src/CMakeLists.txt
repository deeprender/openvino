# Copyright (C) 2018-2023 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET_NAME openvino_c)

# Suppress warnings due to catch macro with legacy exception types
ov_deprecated_no_errors()
add_definitions(-DIN_OV_COMPONENT)

file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE HEADERS ${OpenVINO_C_API_SOURCE_DIR}/include/*.h)

# create library
add_library(${TARGET_NAME} ${HEADERS} ${SOURCES})
add_library(openvino::runtime::c ALIAS ${TARGET_NAME})

target_link_libraries(${TARGET_NAME} PRIVATE openvino openvino::util)

target_include_directories(${TARGET_NAME} PUBLIC
    $<BUILD_INTERFACE:${OpenVINO_C_API_SOURCE_DIR}/include>)

if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(${TARGET_NAME} PUBLIC OPENVINO_STATIC_LIBRARY)
endif()

ov_add_clang_format_target(${TARGET_NAME}_clang FOR_TARGETS ${TARGET_NAME})

set_target_properties(${TARGET_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ${ENABLE_LTO})

ov_add_vs_version_file(NAME ${TARGET_NAME}
    FILEDESCRIPTION "OpenVINO C API Core Runtime library")

# export

set_target_properties(${TARGET_NAME} PROPERTIES EXPORT_NAME runtime::c)

ov_add_library_version(${TARGET_NAME})

export(TARGETS ${TARGET_NAME} NAMESPACE openvino::
       APPEND FILE "${CMAKE_BINARY_DIR}/OpenVINOTargets.cmake")

# install
ov_cpack_add_component(${OV_CPACK_COMP_CORE_C} HIDDEN)
ov_cpack_add_component(${OV_CPACK_COMP_CORE_C_DEV} HIDDEN)

install(TARGETS ${TARGET_NAME} EXPORT OpenVINOTargets
        RUNTIME DESTINATION ${OV_CPACK_RUNTIMEDIR} COMPONENT ${OV_CPACK_COMP_CORE_C}
        ARCHIVE DESTINATION ${OV_CPACK_ARCHIVEDIR} COMPONENT ${OV_CPACK_COMP_CORE_C}
        LIBRARY DESTINATION ${OV_CPACK_LIBRARYDIR} COMPONENT ${OV_CPACK_COMP_CORE_C}
        NAMELINK_COMPONENT ${OV_CPACK_COMP_CORE_C_DEV}
        INCLUDES DESTINATION ${OV_CPACK_INCLUDEDIR}
                             ${OV_CPACK_INCLUDEDIR}/ie)

install(DIRECTORY ${OpenVINO_C_API_SOURCE_DIR}/include/c_api
        DESTINATION ${OV_CPACK_INCLUDEDIR}/ie
        COMPONENT ${OV_CPACK_COMP_CORE_C_DEV})

install(DIRECTORY ${OpenVINO_C_API_SOURCE_DIR}/include/openvino/
        DESTINATION ${OV_CPACK_INCLUDEDIR}/openvino
        COMPONENT ${OV_CPACK_COMP_CORE_C_DEV})
