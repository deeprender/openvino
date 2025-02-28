# Copyright (C) 2018-2023 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

if (NOT ENABLE_HETERO)
    return()
endif()

set(TARGET_NAME openvino_hetero_plugin)

file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp)

ov_add_plugin(NAME ${TARGET_NAME}
              DEVICE_NAME "HETERO"
              PSEUDO_DEVICE
              SOURCES ${SOURCES} ${HEADERS}
              VERSION_DEFINES_FOR src/version.cpp
              ADD_CLANG_FORMAT)

ov_build_target_faster(${TARGET_NAME}
    UNITY
)

target_link_libraries(${TARGET_NAME} PRIVATE openvino::pugixml)

# must be called after all target_link_libraries
ov_add_api_validator_post_build_step(TARGET ${TARGET_NAME})

set_target_properties(${TARGET_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ${ENABLE_LTO})

if(BUILD_SHARED_LIBS)
    set(OBJ_NAME ${TARGET_NAME}_obj)

    add_library(${OBJ_NAME} OBJECT ${SOURCES} ${HEADERS})
    ov_link_system_libraries(${OBJ_NAME} PUBLIC openvino::pugixml)

    ov_add_version_defines(src/version.cpp ${OBJ_NAME})

    target_include_directories(${OBJ_NAME}
        PRIVATE
            $<TARGET_PROPERTY:openvino::runtime::dev,INTERFACE_INCLUDE_DIRECTORIES>
            $<TARGET_PROPERTY:openvino::itt,INTERFACE_INCLUDE_DIRECTORIES>
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            $<TARGET_PROPERTY:openvino::conditional_compilation,INTERFACE_INCLUDE_DIRECTORIES>)

    ov_set_threading_interface_for(${OBJ_NAME})

    target_compile_definitions(${OBJ_NAME}
        PRIVATE
            USE_STATIC_IE IMPLEMENT_INFERENCE_ENGINE_PLUGIN IMPLEMENT_INFERENCE_EXTENSION_API
            $<TARGET_PROPERTY:ngraph,INTERFACE_COMPILE_DEFINITIONS>
            $<TARGET_PROPERTY:inference_engine_plugin_api,INTERFACE_COMPILE_DEFINITIONS>)

    set_target_properties(${TARGET_NAME}_obj PROPERTIES EXCLUDE_FROM_ALL ON)
    set_target_properties(${TARGET_NAME}_obj PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ${ENABLE_LTO})
endif()

if(ENABLE_TESTS)
    add_subdirectory(tests/unit)
endif()

if(ENABLE_FUNCTIONAL_TESTS)
    add_subdirectory(tests/functional)
endif()
