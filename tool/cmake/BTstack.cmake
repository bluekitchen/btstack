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
set(btstack_group_dependencies_att_db
    core
)
set(btstack_group_dependencies_att_dispatch
    l2cap
)
set(btstack_group_dependencies_gatt_server
    att-db
    att-dispatch
    ble
)
set(btstack_group_dependencies_gatt_client
    att-dispatch
    ble
)
set(btstack_group_dependencies_gatt_service_client
    gatt-client
)
set(btstack_group_dependencies_gatt_service_hids_host
    gatt-service-client
)
set(btstack_group_dependencies_gatt_service_battery_service_client
    gatt-client
)
set(btstack_group_dependencies_gatt_service_ancs_client
    gatt-client
)
set(btstack_group_dependencies_gatt_service_device_information_service_client
    gatt-client
)
set(btstack_group_dependencies_gatt_service_immediate_alert_service_client
    gatt-service-client
)
set(btstack_group_dependencies_gatt_service_link_loss_service_client
    gatt-service-client
)
set(btstack_group_dependencies_gatt_service_object_transfer_service_client
    gatt-service-client
)
set(btstack_group_dependencies_gatt_service_scan_parameters_service_client
    gatt-service-client
)
set(btstack_group_dependencies_gatt_service_tx_power_service_client
    gatt-service-client
)
set(btstack_group_dependencies_gatt_service_battery_service_server
    gatt-server
)
set(btstack_group_dependencies_gatt_service_battery_service_v1_server
    gatt-server
)
set(btstack_group_dependencies_gatt_service_bond_management_service_server
    gatt-server
)
set(btstack_group_dependencies_gatt_service_cycling_power_service_server
    gatt-server
)
set(btstack_group_dependencies_gatt_service_cycling_speed_and_cadence_service_server
    gatt-server
)
set(btstack_group_dependencies_gatt_service_device_information_service_server
    gatt-server
)
set(btstack_group_dependencies_gatt_service_heart_rate_service_server
    gatt-server
)
set(btstack_group_dependencies_gatt_service_hids_device
    gatt-server
)
set(btstack_group_dependencies_gatt_service_immediate_alert_service_server
    gatt-server
)
set(btstack_group_dependencies_gatt_service_link_loss_service_server
    gatt-server
)
set(btstack_group_dependencies_gatt_service_nordic_spp_service_server
    gatt-server
)
set(btstack_group_dependencies_gatt_service_object_transfer_service_server
    gatt-server
)
set(btstack_group_dependencies_gatt_service_scan_parameters_service_server
    gatt-server
)
set(btstack_group_dependencies_gatt_service_tx_power_service_server
    gatt-server
)
set(btstack_group_dependencies_gatt_service_ublox_spp_service_server
    gatt-server
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

# Collect root-relative include directories required by BTstack's public
# headers. Apply these with PUBLIC visibility so applications can include
# btstack.h without also selecting optional third-party implementation sources.
function(btstack_collect_public_include_directories output_variable)
    cmake_parse_arguments(ARG "" "ROOT" "" ${ARGN})

    if(NOT ARG_ROOT)
        message(FATAL_ERROR "btstack_collect_public_include_directories requires ROOT")
    endif()

    set(include_directories)
    foreach(directory IN LISTS btstack_public_include_directories)
        list(APPEND include_directories "${ARG_ROOT}/${directory}")
    endforeach()
    list(REMOVE_DUPLICATES include_directories)
    set(${output_variable} "${include_directories}" PARENT_SCOPE)
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

# Register a port that can provide implementation groups for logical example
# capabilities. A port declaration is metadata only; it does not create a
# target or configure BTstack features.
function(btstack_declare_port port)
    list(FIND btstack_declared_ports "${port}" port_index)
    if(NOT port_index EQUAL -1)
        message(FATAL_ERROR "BTstack port '${port}' is declared more than once")
    endif()

    list(APPEND btstack_declared_ports "${port}")
    set(btstack_declared_ports "${btstack_declared_ports}" PARENT_SCOPE)
endfunction()

# Register concrete source groups that provide one port-neutral capability.
# A port may provide multiple groups for a capability, for example multiple
# runtime-selectable audio backends.
#
# Usage:
#   btstack_declare_port_provider(libusb
#       CAPABILITY audio
#       GROUPS platform-posix-audio-portaudio)
function(btstack_declare_port_provider port)
    cmake_parse_arguments(ARG "" "CAPABILITY" "GROUPS" ${ARGN})

    list(FIND btstack_declared_ports "${port}" port_index)
    if(port_index EQUAL -1)
        message(FATAL_ERROR "BTstack port '${port}' must be declared before its providers")
    endif()
    if(NOT ARG_CAPABILITY)
        message(FATAL_ERROR "btstack_declare_port_provider(${port}) requires CAPABILITY")
    endif()
    if(NOT ARG_GROUPS)
        message(FATAL_ERROR "btstack_declare_port_provider(${port}) requires at least one GROUP")
    endif()

    foreach(group IN LISTS ARG_GROUPS)
        list(FIND btstack_source_groups "${group}" group_index)
        if(group_index EQUAL -1)
            message(FATAL_ERROR
                "BTstack port '${port}' provider '${ARG_CAPABILITY}' has unknown source group '${group}'")
        endif()
    endforeach()

    string(REPLACE "-" "_" port_suffix "${port}")
    string(REPLACE "-" "_" capability_suffix "${ARG_CAPABILITY}")
    set(provider_variable "btstack_port_${port_suffix}_provider_${capability_suffix}")
    set(provider_groups ${${provider_variable}} ${ARG_GROUPS})
    list(REMOVE_DUPLICATES provider_groups)
    set(${provider_variable} "${provider_groups}" PARENT_SCOPE)
endfunction()

# Resolve the source-group and configuration requirements declared by one
# example. NEEDS entries that name catalog groups are expanded through the
# source-group dependency graph. ENABLE_* entries are returned unchanged for a
# port to validate against its active configuration; this helper deliberately
# does not configure those options.
#
# Usage:
#   btstack_resolve_example(gatt_counter BTSTACK_GROUPS BTSTACK_CONFIG_NEEDS)
function(btstack_resolve_example_requirements example port groups_output_variable configuration_output_variable)
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
        elseif(NOT "${port}" STREQUAL "")
            string(REPLACE "-" "_" port_suffix "${port}")
            string(REPLACE "-" "_" capability_suffix "${requirement}")
            set(provider_variable "btstack_port_${port_suffix}_provider_${capability_suffix}")
            if(DEFINED ${provider_variable})
                list(APPEND source_groups ${${provider_variable}})
            else()
                message(FATAL_ERROR
                    "BTstack example '${example}' needs capability '${requirement}', "
                    "but port '${port}' does not provide it")
            endif()
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

function(btstack_resolve_example example groups_output_variable configuration_output_variable)
    btstack_resolve_example_requirements("${example}" "" resolved_groups configuration_requirements)
    set(${groups_output_variable} "${resolved_groups}" PARENT_SCOPE)
    set(${configuration_output_variable} "${configuration_requirements}" PARENT_SCOPE)
endfunction()

# Resolve an example against a declared port. Alongside ordinary source groups
# and ENABLE_* configuration requirements, NEEDS may name a capability that
# the port provides through one or more concrete source groups.
#
# Usage:
#   btstack_resolve_example_for_port(libusb a2dp_source_demo
#       BTSTACK_GROUPS BTSTACK_CONFIG_NEEDS)
function(btstack_resolve_example_for_port port example groups_output_variable configuration_output_variable)
    list(FIND btstack_declared_ports "${port}" port_index)
    if(port_index EQUAL -1)
        message(FATAL_ERROR "BTstack port '${port}' has not been declared")
    endif()

    btstack_resolve_example_requirements("${example}" "${port}"
        resolved_groups configuration_requirements)
    set(${groups_output_variable} "${resolved_groups}" PARENT_SCOPE)
    set(${configuration_output_variable} "${configuration_requirements}" PARENT_SCOPE)
endfunction()

# Create a static BTstack library for one example's resolved source closure.
# Port policy remains with the caller: it supplies the baseline source groups,
# port-local sources, and all target include/link settings.
#
# Usage:
#   btstack_add_scoped_example_library(
#       TARGET btstack_gatt_counter
#       ROOT "${BTSTACK_ROOT}"
#       PORT libusb
#       EXAMPLE gatt_counter
#       BASE_GROUPS ${BTSTACK_LIBUSB_BASE_GROUPS}
#       PORT_SOURCES ${SOURCES_PORT}
#       PUBLIC_INCLUDE_DIRECTORIES ${BTSTACK_PUBLIC_INCLUDE_DIRECTORIES}
#       PRIVATE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}"
#       LINK_LIBRARIES ${CMAKE_THREAD_LIBS_INIT} m
#   )
#
# CONFIGURATION_REQUIREMENTS_OUTPUT is optional. If set, it receives the
# example's unresolved ENABLE_* requirements for future port validation.
function(btstack_add_scoped_example_library)
    cmake_parse_arguments(ARG ""
        "TARGET;ROOT;PORT;EXAMPLE;CONFIGURATION_REQUIREMENTS_OUTPUT"
        "BASE_GROUPS;PORT_SOURCES;PUBLIC_INCLUDE_DIRECTORIES;PRIVATE_INCLUDE_DIRECTORIES;LINK_LIBRARIES;LINK_DIRECTORIES"
        ${ARGN})

    foreach(required TARGET ROOT PORT EXAMPLE)
        if(NOT ARG_${required})
            message(FATAL_ERROR "btstack_add_scoped_example_library requires ${required}")
        endif()
    endforeach()

    btstack_resolve_example_for_port(${ARG_PORT} ${ARG_EXAMPLE}
        example_groups
        example_configuration_requirements
    )
    set(library_groups
        ${example_groups}
        ${ARG_BASE_GROUPS}
    )
    btstack_collect_sources(example_sources
        ROOT "${ARG_ROOT}"
        GROUPS ${library_groups}
    )
    btstack_collect_include_directories(example_include_directories
        ROOT "${ARG_ROOT}"
        GROUPS ${library_groups}
    )

    add_library(${ARG_TARGET} STATIC
        ${ARG_PORT_SOURCES}
        ${example_sources}
    )
    btstack_target_warnings(${ARG_TARGET} STRICT)
    if(ARG_PUBLIC_INCLUDE_DIRECTORIES)
        target_include_directories(${ARG_TARGET} PUBLIC
            ${ARG_PUBLIC_INCLUDE_DIRECTORIES})
    endif()
    target_include_directories(${ARG_TARGET} PRIVATE
        ${ARG_PRIVATE_INCLUDE_DIRECTORIES}
        ${example_include_directories}
    )
    if(ARG_LINK_DIRECTORIES)
        target_link_directories(${ARG_TARGET} PUBLIC ${ARG_LINK_DIRECTORIES})
    endif()
    if(ARG_LINK_LIBRARIES)
        target_link_libraries(${ARG_TARGET} PUBLIC ${ARG_LINK_LIBRARIES})
    endif()

    if(ARG_CONFIGURATION_REQUIREMENTS_OUTPUT)
        set(${ARG_CONFIGURATION_REQUIREMENTS_OUTPUT}
            "${example_configuration_requirements}" PARENT_SCOPE)
    endif()
endfunction()
