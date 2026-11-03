# Helpers for composing BTstack source groups in a CMake project.
#
# This file deliberately has no project(), target, or platform policy. A port
# keeps those responsibilities in its own CMakeLists.txt.

include("${CMAKE_CURRENT_LIST_DIR}/BTstackSources.cmake")

function(btstack_collect_group_paths output_variable)
    cmake_parse_arguments(ARG "" "ROOT;VARIABLE_PREFIX" "GROUPS" ${ARGN})

    if(NOT ARG_ROOT)
        message(FATAL_ERROR "btstack_collect_group_paths requires ROOT")
    endif()

    if(NOT ARG_GROUPS)
        message(FATAL_ERROR "btstack_collect_group_paths requires at least one GROUP")
    endif()

    if(NOT ARG_VARIABLE_PREFIX)
        message(FATAL_ERROR "btstack_collect_group_paths requires VARIABLE_PREFIX")
    endif()

    set(sources)
    foreach(group IN LISTS ARG_GROUPS)
        list(FIND btstack_source_groups "${group}" group_index)
        if(group_index EQUAL -1)
            string(REPLACE ";" ", " known_groups "${btstack_source_groups}")
            message(FATAL_ERROR
                "Unknown BTstack source group '${group}'. Known groups: ${known_groups}")
        endif()

        string(REPLACE "-" "_" group_suffix "${group}")
        set(group_variable "${ARG_VARIABLE_PREFIX}_${group_suffix}")
        if(NOT DEFINED ${group_variable})
            message(FATAL_ERROR
                "BTstack source group '${group}' has no ${group_variable} definition")
        endif()

        foreach(source IN LISTS ${group_variable})
            list(APPEND sources "${ARG_ROOT}/${source}")
        endforeach()
    endforeach()

    list(REMOVE_DUPLICATES sources)
    set(${output_variable} "${sources}" PARENT_SCOPE)
endfunction()

# Collect root-relative source groups into absolute source paths.
#
# Usage:
#   include("${BTSTACK_ROOT}/tool/cmake/BTstack.cmake")
#   btstack_collect_sources(BTSTACK_SOURCES
#       ROOT "${BTSTACK_ROOT}"
#       GROUPS core ble le-audio lc3 transport-h5)
#
# The requested group IDs must be listed in btstack_source_groups. Group IDs
# use lower-case hyphenated names; their backing variables use underscores.
function(btstack_collect_sources output_variable)
    btstack_collect_group_paths(${output_variable}
        VARIABLE_PREFIX btstack_sources
        ${ARGN})
    set(${output_variable} "${${output_variable}}" PARENT_SCOPE)
endfunction()

# Collect root-relative private include directories required by source groups.
# Port configuration, SDK, and generated-header include directories remain
# explicit in the port CMakeLists.txt.
function(btstack_collect_include_directories output_variable)
    btstack_collect_group_paths(${output_variable}
        VARIABLE_PREFIX btstack_include_directories
        ${ARGN})
    set(${output_variable} "${${output_variable}}" PARENT_SCOPE)
endfunction()

# Generate a GATT database header and make a target depend on it.
#
# Usage:
#   btstack_target_gatt(my_example "${BTSTACK_ROOT}/example/gatt_counter.gatt"
#       DEPENDS ${service_gatt_files})
#
# DEPENDS is optional and is intended for imported .gatt service definitions.
# BTSTACK_ROOT must be set by the calling project.
function(btstack_target_gatt target gatt_file)
    cmake_parse_arguments(ARG "" "" "DEPENDS" ${ARGN})

    if(NOT TARGET ${target})
        message(FATAL_ERROR "btstack_target_gatt requires existing target '${target}'")
    endif()

    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    get_filename_component(gatt_file_name "${gatt_file}" NAME_WLE)
    set(output_header "${CMAKE_CURRENT_BINARY_DIR}/${gatt_file_name}.h")

    add_custom_command(
        OUTPUT "${output_header}"
        DEPENDS "${gatt_file}" ${ARG_DEPENDS}
        COMMAND "${Python3_EXECUTABLE}"
        ARGS "${BTSTACK_ROOT}/tool/compile_gatt.py" "${gatt_file}" "${output_header}"
    )

    add_custom_target("MAKE_${gatt_file_name}" ALL DEPENDS "${output_header}")
    add_dependencies(${target} "MAKE_${gatt_file_name}")
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
endfunction()

# Apply BTstack's baseline warning policy to a target.
#
# Usage:
#   btstack_target_warnings(btstack)
#   btstack_target_warnings(btstack STRICT)
#
# STRICT promotes enabled warnings to errors. The helper is opt-in and uses
# PRIVATE options, so it never changes an embedding project's warning policy.
function(btstack_target_warnings target)
    cmake_parse_arguments(ARG "STRICT" "" "" ${ARGN})

    if(NOT TARGET ${target})
        message(FATAL_ERROR "btstack_target_warnings requires existing target '${target}'")
    endif()

    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        set(warnings
            -Wunused-variable
            -Wswitch-default
            -Wall
            -Wextra)
        if(ARG_STRICT)
            list(APPEND warnings -Werror -Werror=uninitialized)
        endif()
    elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set(warnings
            -Wunused-but-set-variable
            -Wunused-variable
            -Wswitch-default
            -Wall
            -Wextra)
        if(ARG_STRICT)
            list(APPEND warnings -Werror)
        endif()
    else()
        return()
    endif()

    target_compile_options(${target} PRIVATE ${warnings})
endfunction()

# Register the declarative requirements for one BTstack example.
#
# The declaration does not configure features or create a target. Port build
# configuration selects the source closure; a future resolver will use this
# metadata to validate whether the configured port can build the example.
#
# Usage:
#   btstack_declare_example(gatt_counter
#       SOURCES gatt_counter.c
#       GATT_DB gatt_counter.gatt
#       NEEDS le-peripheral)
function(btstack_declare_example example)
    cmake_parse_arguments(ARG "" "GATT_DB" "SOURCES;NEEDS" ${ARGN})

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "btstack_declare_example(${example}) requires SOURCES")
    endif()

    list(FIND btstack_declared_examples "${example}" example_index)
    if(NOT example_index EQUAL -1)
        message(FATAL_ERROR "BTstack example '${example}' is declared more than once")
    endif()

    list(APPEND btstack_declared_examples "${example}")
    set(btstack_declared_examples "${btstack_declared_examples}" PARENT_SCOPE)
    set("btstack_example_${example}_sources" "${ARG_SOURCES}" PARENT_SCOPE)
    set("btstack_example_${example}_gatt_db" "${ARG_GATT_DB}" PARENT_SCOPE)
    set("btstack_example_${example}_needs" "${ARG_NEEDS}" PARENT_SCOPE)
endfunction()
