# Helpers for composing BTstack source groups in a CMake project.
#
# This file deliberately has no project(), target, or platform policy. A port
# keeps those responsibilities in its own CMakeLists.txt.

include("${CMAKE_CURRENT_LIST_DIR}/BTstackSources.cmake")

# Explicit internal source-group dependencies. This is resolver metadata, not
# source-catalog metadata: the generator classifies files but never infers
# dependencies. Add entries incrementally as their dependency closures are
# reviewed.
set(btstack_group_dependencies_l2cap
    core
)
set(btstack_group_dependencies_ble
    l2cap
)
set(btstack_group_dependencies_classic_rfcomm
    l2cap
)

# Resolve a source-group dependency closure in dependency-first order.
#
# Usage:
#   btstack_resolve_groups(BTSTACK_GROUPS
#       GROUPS classic-rfcomm)
#
# The initial result is: core; l2cap; classic-rfcomm. The resolver only adds
# explicit internal dependencies; provider selection and port capabilities are
# intentionally handled by later resolver stages.
function(btstack_resolve_groups output_variable)
    cmake_parse_arguments(ARG "" "" "GROUPS" ${ARGN})

    if(NOT ARG_GROUPS)
        message(FATAL_ERROR "btstack_resolve_groups requires at least one GROUP")
    endif()

    # First calculate the complete closure and validate every group name.
    set(closure)
    set(pending ${ARG_GROUPS})
    while(pending)
        list(GET pending 0 group)
        list(REMOVE_AT pending 0)

        list(FIND btstack_source_groups "${group}" group_index)
        if(group_index EQUAL -1)
            string(REPLACE ";" ", " known_groups "${btstack_source_groups}")
            message(FATAL_ERROR
                "Unknown BTstack source group '${group}'. Known groups: ${known_groups}")
        endif()

        list(FIND closure "${group}" closure_index)
        if(closure_index EQUAL -1)
            list(APPEND closure "${group}")
            string(REPLACE "-" "_" group_suffix "${group}")
            set(dependency_variable "btstack_group_dependencies_${group_suffix}")
            if(DEFINED ${dependency_variable})
                list(APPEND pending ${${dependency_variable}})
            endif()
        endif()
    endwhile()

    # Emit dependencies before dependents and reject accidental cycles.
    set(resolved)
    set(remaining ${closure})
    while(remaining)
        set(progress FALSE)
        foreach(group IN LISTS remaining)
            string(REPLACE "-" "_" group_suffix "${group}")
            set(dependency_variable "btstack_group_dependencies_${group_suffix}")
            set(dependencies ${${dependency_variable}})
            set(blocked FALSE)
            foreach(dependency IN LISTS dependencies)
                list(FIND remaining "${dependency}" dependency_index)
                if(NOT dependency_index EQUAL -1)
                    set(blocked TRUE)
                    break()
                endif()
            endforeach()
            if(NOT blocked)
                list(APPEND resolved "${group}")
                list(REMOVE_ITEM remaining "${group}")
                set(progress TRUE)
            endif()
        endforeach()
        if(NOT progress)
            string(REPLACE ";" ", " unresolved_groups "${remaining}")
            message(FATAL_ERROR
                "Cyclic BTstack source-group dependencies: ${unresolved_groups}")
        endif()
    endwhile()

    set(${output_variable} "${resolved}" PARENT_SCOPE)
endfunction()

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
# The generator itself is always a dependency, so changes to compile_gatt.py
# regenerate every affected header.
# BTSTACK_ROOT must be set by the calling project.
function(btstack_target_gatt target gatt_file)
    cmake_parse_arguments(ARG "" "" "DEPENDS" ${ARGN})

    if(NOT TARGET ${target})
        message(FATAL_ERROR "btstack_target_gatt requires existing target '${target}'")
    endif()

    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    get_filename_component(gatt_file_name "${gatt_file}" NAME_WLE)
    set(output_header "${CMAKE_CURRENT_BINARY_DIR}/${gatt_file_name}.h")
    set(depfile "${output_header}.d")

    add_custom_command(
        OUTPUT "${output_header}"
        DEPFILE "${depfile}"
        DEPENDS "${gatt_file}" "${BTSTACK_ROOT}/tool/compile_gatt.py" ${ARG_DEPENDS}
        COMMAND "${Python3_EXECUTABLE}"
        ARGS "${BTSTACK_ROOT}/tool/compile_gatt.py" --depfile "${depfile}"
             "${gatt_file}" "${output_header}"
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
# configuration selects the source closure and validates configuration
# requirements before it creates a target.
#
# Usage:
#   btstack_declare_example(gatt_counter
#       SOURCES gatt_counter.c
#       GATT_DB gatt_counter.gatt
#       NEEDS ble ENABLE_LE_PERIPHERAL)
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

# Resolve the source-group and configuration requirements declared by one
# example. NEEDS entries that name catalog groups are expanded through the
# source-group dependency graph. ENABLE_* entries are returned unchanged for a
# port to validate against its active configuration; this helper deliberately
# does not configure those options.
#
# Usage:
#   btstack_resolve_example(gatt_counter BTSTACK_GROUPS BTSTACK_CONFIG_NEEDS)
function(btstack_resolve_example example groups_output_variable configuration_output_variable)
    list(FIND btstack_declared_examples "${example}" example_index)
    if(example_index EQUAL -1)
        message(FATAL_ERROR "BTstack example '${example}' has not been declared")
    endif()

    set(source_groups)
    set(configuration_requirements)
    foreach(requirement IN LISTS btstack_example_${example}_needs)
        list(FIND btstack_source_groups "${requirement}" group_index)
        if(NOT group_index EQUAL -1)
            list(APPEND source_groups "${requirement}")
        elseif(requirement MATCHES "^ENABLE_[A-Z0-9_]+$")
            list(APPEND configuration_requirements "${requirement}")
        else()
            message(FATAL_ERROR
                "BTstack example '${example}' has unknown requirement '${requirement}'. "
                "Requirements must be source groups or ENABLE_* configuration symbols")
        endif()
    endforeach()

    if(source_groups)
        btstack_resolve_groups(resolved_groups GROUPS ${source_groups})
    else()
        set(resolved_groups)
    endif()

    set(${groups_output_variable} "${resolved_groups}" PARENT_SCOPE)
    set(${configuration_output_variable} "${configuration_requirements}" PARENT_SCOPE)
endfunction()
