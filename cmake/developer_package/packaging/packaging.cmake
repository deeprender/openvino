# Copyright (C) 2018-2023 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

include(CPackComponent)

# we don't need RPATHs, because setupvars.sh is used
set(CMAKE_SKIP_INSTALL_RPATH ON)

#
# ov_install_static_lib(<target> <comp>)
#
macro(ov_install_static_lib target comp)
    if(NOT BUILD_SHARED_LIBS)
        get_target_property(target_type ${target} TYPE)
        if(target_type STREQUAL "STATIC_LIBRARY")
            set_target_properties(${target} PROPERTIES EXCLUDE_FROM_ALL OFF)
        endif()

        # save all internal installed targets to filter them later in 'ov_generate_dev_package_config'
        list(APPEND openvino_installed_targets ${target})
        set(openvino_installed_targets "${openvino_installed_targets}" CACHE INTERNAL
            "A list of OpenVINO internal targets" FORCE)

        install(TARGETS ${target} EXPORT OpenVINOTargets
                ARCHIVE DESTINATION ${OV_CPACK_ARCHIVEDIR} COMPONENT ${comp} ${ARGN})

        # export to local tree to build against static build tree
        export(TARGETS ${target} NAMESPACE openvino::
               APPEND FILE "${CMAKE_BINARY_DIR}/OpenVINOTargets.cmake")
    endif()
endmacro()

#
# ov_get_pyversion(<OUT pyversion>)
#
function(ov_get_pyversion pyversion)
    find_package(PythonInterp 3 QUIET)
    if(PYTHONINTERP_FOUND)
        set(${pyversion} "python${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}" PARENT_SCOPE)
    else()
        set(${pyversion} "NOT-FOUND" PARENT_SCOPE)
    endif()
endfunction()

#
# ov_cpack_add_component(NAME ...)
#
# Wraps original `cpack_add_component` and adds component to internal IE list
#
function(ov_cpack_add_component name)
    if(NOT ${name} IN_LIST IE_CPACK_COMPONENTS_ALL)
        cpack_add_component(${name} ${ARGN})

        # need to store informarion about cpack_add_component arguments in CMakeCache.txt
        # to restore it later
        set(_${name}_cpack_component_args "${ARGN}" CACHE INTERNAL "Argument for cpack_add_component for ${name} cpack component" FORCE)

        list(APPEND IE_CPACK_COMPONENTS_ALL ${name})
        set(IE_CPACK_COMPONENTS_ALL "${IE_CPACK_COMPONENTS_ALL}" CACHE INTERNAL "" FORCE)
    endif()
endfunction()

foreach(comp IN LISTS IE_CPACK_COMPONENTS_ALL)
    unset(_${comp}_cpack_component_args)
endforeach()
unset(IE_CPACK_COMPONENTS_ALL CACHE)

# create `tests` component
if(ENABLE_TESTS)
    cpack_add_component(tests HIDDEN)
endif()

#
#  ov_install_with_name(<FILE> <COMPONENT>)
#
# if <FILE> is a symlink, we resolve it, but install file with a name of symlink
#
function(ov_install_with_name file component)
    get_filename_component(actual_name "${file}" NAME)
    if((APPLE AND actual_name MATCHES "^[^\.]+\.[0-9]+${CMAKE_SHARED_LIBRARY_SUFFIX}$") OR
                (actual_name MATCHES "^.*\.${CMAKE_SHARED_LIBRARY_SUFFIX}\.[0-9]+$"))
        if(IS_SYMLINK "${file}")
            get_filename_component(file "${file}" REALPATH)
            set(install_rename RENAME "${actual_name}")
        endif()

        install(FILES "${file}"
                DESTINATION runtime/3rdparty/${component}/lib
                COMPONENT ${component}
                EXCLUDE_FROM_ALL
                ${install_rename})

        set("${component}_INSTALLED" ON PARENT_SCOPE)
    endif()
endfunction()

#
# List of public OpenVINO components
#

macro(ov_define_component_names)
    # core components
    set(OV_CPACK_COMP_CORE "core")
    set(OV_CPACK_COMP_CORE_C "core_c")
    set(OV_CPACK_COMP_CORE_DEV "core_dev")
    set(OV_CPACK_COMP_CORE_C_DEV "core_c_dev")
    # licensing
    set(OV_CPACK_COMP_LICENSING "licensing")
    # samples
    set(OV_CPACK_COMP_CPP_SAMPLES "cpp_samples")
    set(OV_CPACK_COMP_C_SAMPLES "c_samples")
    set(OV_CPACK_COMP_PYTHON_SAMPLES "python_samples")
    # python
    set(OV_CPACK_COMP_PYTHON_OPENVINO "pyopenvino")
    set(OV_CPACK_COMP_BENCHMARK_APP "benchmark_app")
    set(OV_CPACK_COMP_OVC "ovc")
    set(OV_CPACK_COMP_PYTHON_OPENVINO_PACKAGE "pyopenvino_package")
    set(OV_CPACK_COMP_PYTHON_WHEELS "python_wheels")
    set(OV_CPACK_COMP_OPENVINO_REQ_FILES "openvino_req_files")
    # tools
    set(OV_CPACK_COMP_OPENVINO_DEV_REQ_FILES "openvino_dev_req_files")
    set(OV_CPACK_COMP_DEPLOYMENT_MANAGER "deployment_manager")
    # scripts
    set(OV_CPACK_COMP_INSTALL_DEPENDENCIES "install_dependencies")
    set(OV_CPACK_COMP_SETUPVARS "setupvars")
endmacro()

ov_define_component_names()

if(NOT DEFINED CPACK_GENERATOR)
    set(CPACK_GENERATOR "TGZ")
elseif(NOT CPACK_GENERATOR)
    message(FATAL_ERROR "CPACK_GENERATOR cannot contain an empty value")
endif()

#
# Include generator specific configuration file:
# 1. Overrides directories set by ov_<debian | rpm | archive | common_libraries>_cpack_set_dirs()
#    This is requried, because different generator use different locations for installed files
# 2. Merges some components using ov_override_component_names()
#    This is required, because different generators have different set of components
#    (e.g. C and C++ API are separate components)
# 3. Exclude some components using ov_define_component_include_rules()
#    This steps exclude some files from installation by defining variables meaning EXCLUDE_ALL
# 4. Sets ov_<debian | rpm | ...>_specific_settings() with DEB generator variables
#    This 'callback' is later called from ov_cpack (wrapper for standard cpack) to set
#    per-component settings (e.g. package names, dependencies, versions and system dependencies)
# 5. (Optional) Defines the following helper functions, which can be used by 3rdparty modules:
#    Debian:
#     - ov_debian_add_changelog_and_copyright()
#     - ov_debian_add_lintian_suppression()
#     - ov_debian_generate_conflicts()
#     - ov_debian_add_latest_component()
#    RPM:
#     - ov_rpm_add_rpmlint_suppression()
#     - ov_rpm_generate_conflicts()
#     - ov_rpm_copyright()
#     - ov_rpm_add_latest_component()
#
if(CPACK_GENERATOR STREQUAL "DEB")
    include(packaging/debian/debian)
elseif(CPACK_GENERATOR STREQUAL "RPM")
    include(packaging/rpm/rpm)
elseif(CPACK_GENERATOR STREQUAL "NSIS")
    include(packaging/nsis)
elseif(CPACK_GENERATOR MATCHES "^(CONDA-FORGE|BREW|CONAN|VCPKG)$")
    include(packaging/common-libraries)
elseif(CPACK_GENERATOR MATCHES "^(7Z|TBZ2|TGZ|TXZ|TZ|TZST|ZIP)$")
    include(packaging/archive)
endif()

macro(ie_cpack)
    set(CPACK_SOURCE_GENERATOR "") # not used
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "OpenVINO™ Toolkit")
    set(CPACK_COMPONENT_UNSPECIFIED_REQUIRED OFF)
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
    set(CPACK_PACKAGE_VENDOR "Intel Corporation")
    set(CPACK_PACKAGE_CONTACT "OpenVINO Developers <openvino@intel.com>")
    set(CPACK_VERBATIM_VARIABLES ON)
    set(CPACK_COMPONENTS_ALL ${ARGN})
    # TODO: set proper license file for Windows installer
    set(CPACK_RESOURCE_FILE_LICENSE "${OpenVINO_SOURCE_DIR}/LICENSE")

    # default permissions for directories creation
    set(CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE OWNER_EXECUTE
        WORLD_READ WORLD_EXECUTE)

    # archive operations can be run in parallel since CMake 3.20
    set(CPACK_THREADS 8)

    if(NOT DEFINED CPACK_STRIP_FILES)
        set(CPACK_STRIP_FILES ON)
    endif()

    # TODO: replace with openvino and handle multi-config generators case
    if(WIN32)
        set(CPACK_PACKAGE_NAME inference-engine_${CMAKE_BUILD_TYPE})
    else()
        set(CPACK_PACKAGE_NAME inference-engine)
    endif()

    set(CPACK_PACKAGE_VERSION "${OpenVINO_VERSION}")
    # build version can be empty in case we are running cmake out of git repository
    if(NOT OpenVINO_VERSION_BUILD STREQUAL "000")
        set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION}.${OpenVINO_VERSION_BUILD}")
    endif()

    foreach(ver MAJOR MINOR PATCH)
        if(DEFINED OpenVINO_VERSION_${ver})
            set(CPACK_PACKAGE_VERSION_${ver} ${OpenVINO_VERSION_${ver}})
        else()
            message(FATAL_ERROR "Internal: OpenVINO_VERSION_${ver} variable is not defined")
        endif()
    endforeach()

    if(OS_FOLDER)
        set(CPACK_SYSTEM_NAME "${OS_FOLDER}")
    endif()

    # include GENERATOR dedicated per-component configuration file
    # NOTE: private modules need to define ov_cpack_settings macro
    # for custom packages configuration
    if(COMMAND ov_cpack_settings)
        ov_cpack_settings()
    endif()

    include(CPack)
endmacro()
