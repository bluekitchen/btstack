# Helpers for composing BTstack source groups in a CMake project.
#
# This file deliberately has no project(), target, or platform policy. A port
# keeps those responsibilities in its own CMakeLists.txt.

get_filename_component(btstack_cmake_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/BTstackSources.cmake")
set(btstack_config_extractor "${CMAKE_CURRENT_LIST_DIR}/../extract_btstack_config.py")
set(btstack_list_examples_script "${CMAKE_CURRENT_LIST_DIR}/BTstackListExamples.cmake")
set(btstack_info_script "${CMAKE_CURRENT_LIST_DIR}/BTstackInfo.cmake")
set(btstack_group_graph_script "${CMAKE_CURRENT_LIST_DIR}/BTstackGroupGraph.cmake")
set(btstack_library_info_script "${CMAKE_CURRENT_LIST_DIR}/BTstackLibraryInfo.cmake")

# Configuration marker groups are resolver-only metadata and deliberately
# stay out of the generated source catalog.
list(APPEND btstack_source_groups core-classic core-le)
set(btstack_sources_core_classic)
set(btstack_sources_core_le)

# Explicit internal source-group dependencies. This is resolver metadata, not
# source-catalog metadata: the generator classifies files but never infers
# dependencies. Add entries incrementally as their dependency closures are
# reviewed.
set(btstack_group_dependencies_l2cap
    core
)
set(btstack_group_dependencies_ble
    l2cap
    core-le
)
set(btstack_group_configuration_requirements_core_classic
    ENABLE_CLASSIC
)
set(btstack_group_configuration_requirements_core_le
    ENABLE_BLE
)
set(btstack_group_configuration_requirements_le_device_db_tlv
    ENABLE_BLE
)
set(btstack_group_configuration_requirements_link_key_db_tlv
    ENABLE_CLASSIC
)
set(btstack_group_dependencies_classic
    core-classic
)
set(btstack_group_dependencies_classic_avdtp_util
    core-classic
)
set(btstack_group_dependencies_classic_avrcp_media_item_iterator
    core-classic
)
set(btstack_group_dependencies_classic_btstack_cvsd_plc
    core-classic
)
set(btstack_group_dependencies_classic_btstack_sbc
    core-classic
)
set(btstack_group_dependencies_classic_btstack_sbc_bluedroid
    core-classic
)
set(btstack_group_dependencies_classic_btstack_sbc_decoder_bluedroid
    core-classic
)
set(btstack_group_dependencies_classic_btstack_sbc_encoder_bluedroid
    core-classic
)
set(btstack_group_dependencies_classic_btstack_sbc_plc
    core-classic
)
set(btstack_group_dependencies_classic_gatt_sdp
    core-classic
)
set(btstack_group_dependencies_classic_goep_server
    core-classic
)
set(btstack_group_dependencies_classic_hfp_codec
    core-classic
)
set(btstack_group_dependencies_classic_hfp_gsm_model
    core-classic
)
set(btstack_group_dependencies_classic_hfp_msbc
    core-classic
)
set(btstack_group_dependencies_classic_obex_iterator
    core-classic
)
set(btstack_group_dependencies_classic_obex_message_builder
    core-classic
)
set(btstack_group_dependencies_classic_obex_parser
    core-classic
)
set(btstack_group_dependencies_classic_obex_srm_client
    core-classic
)
set(btstack_group_dependencies_classic_obex_srm_server
    core-classic
)
set(btstack_group_dependencies_classic_pan
    core-classic
)
set(btstack_group_dependencies_classic_sdp_util
    core-classic
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
    core-classic
)
set(btstack_group_dependencies_classic_sdp_server
    l2cap
    classic-sdp-util
    core-classic
)
set(btstack_group_dependencies_classic_sdp_client
    l2cap
    classic-sdp-util
    core-classic
)
set(btstack_group_dependencies_classic_sdp_client_rfcomm
    classic-sdp-client
    core-classic
)
set(btstack_group_dependencies_classic_hid_device
    l2cap
    classic-sdp-server
    core-classic
)
set(btstack_group_dependencies_classic_hid_host
    l2cap
    classic-sdp-client
    core-classic
)
set(btstack_group_dependencies_classic_device_id_server
    classic-sdp-util
    core-classic
)
set(btstack_group_dependencies_classic_goep_client
    classic-rfcomm
    classic-sdp-client
    classic-obex-message-builder
    classic-obex-iterator
    core-classic
)
set(btstack_group_dependencies_classic_pbap_client
    classic-goep-client
    classic-obex-parser
    third-party-md5
    classic-obex-srm-client
    third-party-yxml
    core-classic
)
set(btstack_group_dependencies_classic_hfp
    classic-rfcomm
    classic-sdp-client-rfcomm
    classic-sdp-server
    classic-hfp-codec
    audio-demo
    classic-btstack-cvsd-plc
    classic-btstack-sbc-encoder-bluedroid
    classic-btstack-sbc-decoder-bluedroid
    classic-btstack-sbc-bluedroid
    classic-btstack-sbc-plc
    third-party-bluedroid-sbc
    third-party-hxcmod
    core-classic
)
set(btstack_group_dependencies_classic_hfp_ag
    classic-hfp
    classic-hfp-gsm-model
    core-classic
)
set(btstack_group_dependencies_classic_hfp_hf
    classic-hfp
    core-classic
)
set(btstack_group_dependencies_classic_hsp_ag
    classic-rfcomm
    classic-sdp-client-rfcomm
    classic-sdp-server
    classic-hfp-codec
    audio-demo
    classic-btstack-cvsd-plc
    classic-btstack-sbc-bluedroid
    classic-btstack-sbc-encoder-bluedroid
    classic-btstack-sbc-plc
    third-party-bluedroid-sbc
    third-party-hxcmod
    core-classic
)
set(btstack_group_dependencies_classic_hsp_hs
    classic-rfcomm
    classic-sdp-client-rfcomm
    classic-sdp-server
    classic-hfp-codec
    audio-demo
    classic-btstack-cvsd-plc
    classic-btstack-sbc-bluedroid
    classic-btstack-sbc-encoder-bluedroid
    classic-btstack-sbc-plc
    third-party-bluedroid-sbc
    third-party-hxcmod
    core-classic
)
set(btstack_group_dependencies_classic_avrcp
    l2cap
    classic-sdp-client
    core-classic
)
set(btstack_group_dependencies_classic_avrcp_controller
    classic-avrcp
    core-classic
)
set(btstack_group_dependencies_classic_avrcp_browsing
    l2cap
    classic-sdp-client
    core-classic
)
set(btstack_group_dependencies_classic_avrcp_browsing_controller
    classic-avrcp-browsing
    classic-avrcp-controller
    core-classic
)
set(btstack_group_dependencies_classic_avdtp
    l2cap
    classic-sdp-client
    classic-avdtp-util
    core-classic
)
set(btstack_group_dependencies_classic_avdtp_acceptor
    classic-avdtp
    core-classic
)
set(btstack_group_dependencies_classic_avdtp_initiator
    classic-avdtp
    core-classic
)
set(btstack_group_dependencies_classic_avdtp_sink
    classic-avdtp-acceptor
    classic-avdtp-initiator
    core-classic
)
set(btstack_group_dependencies_classic_a2dp
    classic-avdtp
    core-classic
)
set(btstack_group_dependencies_classic_a2dp_sink
    classic-a2dp
    classic-avdtp-sink
    classic-btstack-sbc
    third-party-bluedroid-sbc
    core-classic
)
set(btstack_group_dependencies_classic_avdtp_source
    classic-avdtp-initiator
    classic-avdtp-acceptor
    core-classic
)
set(btstack_group_dependencies_classic_a2dp_source
    classic-a2dp
    classic-avdtp-source
    classic-btstack-sbc
    third-party-bluedroid-sbc
    core-classic
)
set(btstack_group_dependencies_classic_avrcp_cover_art_client
    classic-goep-client
    classic-obex-parser
    classic-obex-srm-client
    core-classic
)
set(btstack_group_dependencies_classic_avrcp_target
    classic-avrcp
    core-classic
)
set(btstack_group_dependencies_classic_avrcp_browsing_target
    classic-avrcp-browsing
    classic-avrcp-target
    core-classic
)
set(btstack_group_dependencies_le_audio
    ble
    lc3
    third-party-lc3-google
)
# The SPP server group supplies the record builder used by an SPP server.
# Its profile-level closure also selects the RFCOMM data channel and SDP
# registration server required to expose that record.
set(btstack_group_dependencies_classic_spp_server
    classic-rfcomm
    classic-sdp-server
    core-classic
)

# These embedded adapters use SEGGER RTT when ENABLE_SEGGER_RTT is selected
# in the port configuration.  Keep the header and implementation in the
# same scoped closure as the adapters that consume them.
set(btstack_group_dependencies_platform_embedded_hci_dump_segger_rtt
    third-party-segger-rtt
)
set(btstack_group_dependencies_platform_embedded_stdin
    third-party-segger-rtt
)

# During the incremental split, l2cap sources remain in the legacy core group
# as well as their l2cap leaf group. Group object targets give each source one
# owner, so exclude the compatibility copy from core there. Legacy source-list
# consumers continue to receive the overlapping core list unchanged.
set(btstack_group_object_excluded_sources_core
    ${btstack_sources_l2cap}
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
            set(dependencies ${${dependency_variable}})
            list(APPEND pending ${dependencies})
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
            # Marker groups express eligibility, not source ordering.
            list(REMOVE_ITEM dependencies core-classic core-le)
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

    # Marker groups only encode configuration prerequisites; they never own
    # source files and must not reach source collection/object-target creation.
    list(REMOVE_ITEM resolved core-classic core-le)
    set(${output_variable} "${resolved}" PARENT_SCOPE)
endfunction()

# Collect the ENABLE_* requirements implied by source groups and their
# dependency closure. This is shared by example eligibility checks and port
# baseline pruning. Configuration marker groups are intentionally removed by
# btstack_resolve_groups, so Classic and LE requirements are derived from the
# source groups that depend on them as well.
function(btstack_collect_group_configuration_requirements output_variable)
    cmake_parse_arguments(ARG "" "" "GROUPS" ${ARGN})
    if(NOT ARG_GROUPS)
        message(FATAL_ERROR "btstack_collect_group_configuration_requirements requires at least one GROUP")
    endif()

    set(configuration_requirements)
    foreach(group IN LISTS ARG_GROUPS)
        string(REPLACE "-" "_" group_suffix "${group}")
        list(APPEND configuration_requirements
            ${btstack_group_configuration_requirements_${group_suffix}})
    endforeach()

    btstack_resolve_groups(resolved_groups GROUPS ${ARG_GROUPS})
    foreach(group IN LISTS resolved_groups)
        string(REPLACE "-" "_" group_suffix "${group}")
        list(APPEND configuration_requirements
            ${btstack_group_configuration_requirements_${group_suffix}})
        if(group MATCHES "^classic(-|$)")
            list(APPEND configuration_requirements
                ${btstack_group_configuration_requirements_core_classic})
        elseif(group STREQUAL "ble")
            list(APPEND configuration_requirements
                ${btstack_group_configuration_requirements_core_le})
        endif()
    endforeach()
    list(REMOVE_DUPLICATES configuration_requirements)
    set(${output_variable} "${configuration_requirements}" PARENT_SCOPE)
endfunction()

# Select the port baseline groups whose configuration requirements are active.
# Unlike example requirements, unavailable baseline groups are omitted rather
# than making a target ineligible: they describe optional support that the
# port's main.c already guards with the corresponding feature macros.
function(btstack_filter_groups_for_port output_variable)
    cmake_parse_arguments(ARG "" "PORT" "GROUPS" ${ARGN})
    if(NOT ARG_PORT)
        message(FATAL_ERROR "btstack_filter_groups_for_port requires PORT")
    endif()
    if(NOT ARG_GROUPS)
        message(FATAL_ERROR "btstack_filter_groups_for_port requires at least one GROUP")
    endif()
    list(FIND btstack_declared_ports "${ARG_PORT}" port_index)
    if(port_index EQUAL -1)
        message(FATAL_ERROR "BTstack port '${ARG_PORT}' has not been declared")
    endif()

    string(REPLACE "-" "_" port_suffix "${ARG_PORT}")
    set(configuration_macros
        ${btstack_port_${port_suffix}_configuration_macros})
    set(selected_groups)
    foreach(group IN LISTS ARG_GROUPS)
        btstack_collect_group_configuration_requirements(group_configuration_requirements
            GROUPS ${group})
        set(group_enabled TRUE)
        foreach(configuration_requirement IN LISTS group_configuration_requirements)
            list(FIND configuration_macros "${configuration_requirement}" configuration_index)
            if(configuration_index EQUAL -1)
                set(group_enabled FALSE)
                break()
            endif()
        endforeach()
        if(group_enabled)
            list(APPEND selected_groups "${group}")
        endif()
    endforeach()
    set(${output_variable} "${selected_groups}" PARENT_SCOPE)
endfunction()

function(btstack_collect_group_paths output_variable)
    cmake_parse_arguments(ARG "ALLOW_UNDEFINED" "VARIABLE_PREFIX" "GROUPS" ${ARGN})

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
            if(ARG_ALLOW_UNDEFINED)
                continue()
            else()
                message(FATAL_ERROR
                    "BTstack source group '${group}' has no ${group_variable} definition")
            endif()
        endif()

        foreach(source IN LISTS ${group_variable})
            list(APPEND sources "${btstack_cmake_root}/${source}")
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

# Collect root-relative include directories that applications need when they
# directly use APIs supplied by selected source groups. These are intentionally
# distinct from private implementation include directories.
function(btstack_collect_consumer_include_directories output_variable)
    btstack_collect_group_paths(${output_variable}
        ALLOW_UNDEFINED
        VARIABLE_PREFIX btstack_consumer_include_directories
        ${ARGN})
    set(${output_variable} "${${output_variable}}" PARENT_SCOPE)
endfunction()

# Collect root-relative include directories required by BTstack's public
# headers. Apply these with PUBLIC visibility so applications can include
# btstack.h without also selecting optional third-party implementation sources.
function(btstack_collect_public_include_directories output_variable)
    set(include_directories)
    foreach(directory IN LISTS btstack_public_include_directories)
        list(APPEND include_directories "${btstack_cmake_root}/${directory}")
    endforeach()
    list(REMOVE_DUPLICATES include_directories)
    set(${output_variable} "${include_directories}" PARENT_SCOPE)
endfunction()

# Generate a GATT database header for a target.
#
# Usage:
#   btstack_target_gatt(my_example "${BTSTACK_ROOT}/example/gatt_counter.gatt"
#       DEPENDS ${service_gatt_files})
#
# DEPENDS is optional and is intended for imported .gatt service definitions.
# The generator itself is always a dependency, so changes to compile_gatt.py
# regenerate every affected header.
function(btstack_target_gatt target gatt_file)
    cmake_parse_arguments(ARG "" "" "DEPENDS" ${ARGN})

    if(NOT TARGET ${target})
        message(FATAL_ERROR "btstack_target_gatt requires existing target '${target}'")
    endif()

    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    get_filename_component(gatt_file_name "${gatt_file}" NAME_WLE)
    set(output_directory "${CMAKE_CURRENT_BINARY_DIR}/btstack-gatt/${target}")
    set(output_header "${output_directory}/${gatt_file_name}.h")
    set(depfile "${output_header}.d")

    add_custom_command(
        OUTPUT "${output_header}"
        DEPFILE "${depfile}"
        DEPENDS "${gatt_file}" "${btstack_cmake_root}/tool/compile_gatt.py" ${ARG_DEPENDS}
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_directory}"
        COMMAND "${Python3_EXECUTABLE}"
        ARGS "${btstack_cmake_root}/tool/compile_gatt.py" --depfile "${depfile}"
             "${gatt_file}" "${output_header}"
    )

    target_sources(${target} PRIVATE "${output_header}")
    target_include_directories(${target} PRIVATE "${output_directory}")
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

# Register the declarative requirements for a BTstack application.
#
# The declaration does not configure features or create a target. Port build
# configuration selects the source closure and validates configuration
# requirements before it creates a target.
#
# Usage:
#   btstack_declare_application(gatt_counter
#       SOURCES gatt_counter.c
#       GATT_DB gatt_counter.gatt
#       NEEDS ble ENABLE_LE_PERIPHERAL)
function(btstack_declare_application application)
    cmake_parse_arguments(ARG "" "GATT_DB" "SOURCES;NEEDS" ${ARGN})
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "btstack_declare_application(${application}) requires SOURCES")
    endif()
    if(application IN_LIST btstack_declared_applications)
        message(FATAL_ERROR "BTstack application '${application}' is declared more than once")
    endif()
    set(application_sources)
    foreach(source IN LISTS ARG_SOURCES)
        get_filename_component(source "${source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
        list(APPEND application_sources "${source}")
    endforeach()
    set(application_gatt_db "")
    if(ARG_GATT_DB)
        get_filename_component(application_gatt_db "${ARG_GATT_DB}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
    endif()
    list(APPEND btstack_declared_applications "${application}")
    set(btstack_declared_applications "${btstack_declared_applications}" PARENT_SCOPE)
    set("btstack_application_${application}_sources" "${application_sources}" PARENT_SCOPE)
    set("btstack_application_${application}_gatt_db" "${application_gatt_db}" PARENT_SCOPE)
    set("btstack_application_${application}_needs" "${ARG_NEEDS}" PARENT_SCOPE)
    # Preserve the legacy example metadata and its original relative paths.
    set(btstack_declared_examples "${btstack_declared_applications}" PARENT_SCOPE)
    set("btstack_example_${application}_sources" "${ARG_SOURCES}" PARENT_SCOPE)
    set("btstack_example_${application}_gatt_db" "${ARG_GATT_DB}" PARENT_SCOPE)
    set("btstack_example_${application}_needs" "${ARG_NEEDS}" PARENT_SCOPE)
endfunction()

# Read a declaration without depending on the metadata variable names.
function(btstack_get_application_property output_variable application property)
    if(NOT application IN_LIST btstack_declared_applications)
        message(FATAL_ERROR "BTstack application '${application}' has not been declared")
    endif()
    if(NOT property MATCHES "^(SOURCES|GATT_DB|NEEDS)$")
        message(FATAL_ERROR "Unknown BTstack application property '${property}'")
    endif()
    string(TOLOWER "${property}" property_name)
    set(${output_variable} "${btstack_application_${application}_${property_name}}" PARENT_SCOPE)
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

# Attach the final macro set obtained by preprocessing a port's
# btstack_config.h. The resolver uses these names to select groups whose
# configuration prerequisites are met.
function(btstack_set_port_configuration_macros port)
    list(FIND btstack_declared_ports "${port}" port_index)
    if(port_index EQUAL -1)
        message(FATAL_ERROR "BTstack port '${port}' must be declared before its configuration macros")
    endif()
    string(REPLACE "-" "_" port_suffix "${port}")
    set("btstack_port_${port_suffix}_configuration_macros" "${ARGN}" PARENT_SCOPE)
endfunction()

# Preprocess a port's btstack_config.h and return its final ENABLE_* and
# HAVE_* macro names. This is deliberately configure-time work: callers use
# the result to decide which targets to create.
function(btstack_collect_port_configuration_macros output_variable)
    cmake_parse_arguments(ARG "" "CONFIG" "DEFINITIONS;INCLUDE_DIRECTORIES" ${ARGN})
    if(NOT ARG_CONFIG)
        message(FATAL_ERROR "btstack_collect_port_configuration_macros requires CONFIG")
    endif()

    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${ARG_CONFIG}"
        "${btstack_config_extractor}")
    set(preprocessor_arguments)
    foreach(definition IN LISTS ARG_DEFINITIONS)
        list(APPEND preprocessor_arguments --define "${definition}")
    endforeach()
    foreach(include_directory IN LISTS ARG_INCLUDE_DIRECTORIES)
        list(APPEND preprocessor_arguments --include-directory "${include_directory}")
    endforeach()
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${btstack_config_extractor}"
            --compiler "${CMAKE_C_COMPILER}"
            --config "${ARG_CONFIG}"
            ${preprocessor_arguments}
        RESULT_VARIABLE preprocess_result
        OUTPUT_VARIABLE configuration_macros
        ERROR_VARIABLE preprocess_error
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT preprocess_result EQUAL 0)
        message(FATAL_ERROR "Could not preprocess ${ARG_CONFIG}:\n${preprocess_error}")
    endif()
    if(configuration_macros)
        string(REPLACE "\n" ";" configuration_macros "${configuration_macros}")
    endif()
    set(${output_variable} "${configuration_macros}" PARENT_SCOPE)
endfunction()

# Create a BTstack library for an existing application or project.
# CONFIG must name the btstack_config.h used to compile the library and its
# consumers. CONFIG_DEFINITIONS and CONFIG_INCLUDE_DIRECTORIES are also used
# when preprocessing it to validate the selected groups.
function(btstack_add_library target)
    cmake_parse_arguments(ARG "" "CONFIG" "GROUPS;CONFIG_DEFINITIONS;CONFIG_INCLUDE_DIRECTORIES" ${ARGN})
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "btstack_add_library: unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT ARG_CONFIG OR NOT ARG_GROUPS)
        message(FATAL_ERROR "btstack_add_library requires CONFIG and GROUPS")
    endif()
    get_filename_component(config "${ARG_CONFIG}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(config_name "${config}" NAME)
    if(NOT config_name STREQUAL "btstack_config.h" OR NOT EXISTS "${config}")
        message(FATAL_ERROR "btstack_add_library CONFIG must name an existing btstack_config.h: ${config}")
    endif()
    get_filename_component(config_directory "${config}" DIRECTORY)
    btstack_collect_port_configuration_macros(config_macros CONFIG "${config}"
        DEFINITIONS ${ARG_CONFIG_DEFINITIONS}
        INCLUDE_DIRECTORIES "${config_directory}" ${ARG_CONFIG_INCLUDE_DIRECTORIES})
    btstack_collect_group_configuration_requirements(required_macros GROUPS ${ARG_GROUPS})
    foreach(required IN LISTS required_macros)
        if(NOT required IN_LIST config_macros)
            message(FATAL_ERROR "btstack_add_library(${target}): ${config} does not define ${required}, required by the selected groups")
        endif()
    endforeach()
    btstack_resolve_groups(groups GROUPS ${ARG_GROUPS})
    btstack_collect_sources(sources GROUPS ${groups})
    btstack_collect_include_directories(private_includes GROUPS ${groups})
    btstack_collect_consumer_include_directories(consumer_includes GROUPS ${groups})
    btstack_collect_public_include_directories(public_includes)
    add_library(${target} STATIC ${sources})
    target_include_directories(${target}
        PUBLIC "${config_directory}" ${ARG_CONFIG_INCLUDE_DIRECTORIES} ${public_includes} ${consumer_includes}
        PRIVATE ${private_includes})
    target_compile_definitions(${target} PUBLIC ${ARG_CONFIG_DEFINITIONS})

    # Generate per-configuration target properties after callers have had a
    # chance to extend the library with their own sources and include paths.
    set(info_directory "${CMAKE_BINARY_DIR}/btstack-library-info")
    file(MAKE_DIRECTORY "${info_directory}")
    string(SHA256 info_key "${target}")
    set(info_content "set(BTSTACK_LIBRARY_NAME [==[${target}]==])\n")
    string(APPEND info_content "set(BTSTACK_LIBRARY_CONFIG [==[${config}]==])\n")
    string(APPEND info_content "set(BTSTACK_LIBRARY_REQUESTED_GROUPS [==[${ARG_GROUPS}]==])\n")
    string(APPEND info_content "set(BTSTACK_LIBRARY_RESOLVED_GROUPS [==[${groups}]==])\n")
    string(APPEND info_content "set(BTSTACK_LIBRARY_REQUIREMENTS [==[${required_macros}]==])\n")
    string(APPEND info_content "set(BTSTACK_LIBRARY_CONFIG_MACROS [==[${config_macros}]==])\n")
    string(APPEND info_content "set(BTSTACK_LIBRARY_SOURCES [==[$<TARGET_PROPERTY:${target},SOURCES>]==])\n")
    string(APPEND info_content "set(BTSTACK_LIBRARY_INCLUDES [==[$<TARGET_PROPERTY:${target},INCLUDE_DIRECTORIES>]==])\n")
    string(APPEND info_content "set(BTSTACK_LIBRARY_CONSUMER_INCLUDES [==[$<TARGET_PROPERTY:${target},INTERFACE_INCLUDE_DIRECTORIES>]==])\n")
    file(GENERATE OUTPUT "${info_directory}/$<CONFIG>/${info_key}.cmake" CONTENT "${info_content}")

    get_property(info_registered GLOBAL PROPERTY BTSTACK_LIBRARY_INFO_TARGETS SET)
    get_property(info_targets GLOBAL PROPERTY BTSTACK_LIBRARY_INFO_TARGETS)
    if(NOT info_registered)
        if(TARGET library-info)
            message(FATAL_ERROR "btstack_add_library cannot create library-info: target already exists")
        endif()
        add_custom_target(library-info
            COMMAND "${CMAKE_COMMAND}" "-DINFO_DIRECTORY=${info_directory}/$<CONFIG>"
                "-DINFO_REGISTRY=${info_directory}/libraries.cmake" -P "${btstack_library_info_script}"
            COMMENT "Display a BTstack library's build inputs" VERBATIM)
    endif()
    list(APPEND info_targets "${target}")
    set_property(GLOBAL PROPERTY BTSTACK_LIBRARY_INFO_TARGETS "${info_targets}")
    file(WRITE "${info_directory}/libraries.cmake" "set(BTSTACK_INFO_LIBRARIES [==[${info_targets}]==])\n")
endfunction()

# Register source groups, port-local sources, and public include directories
# that provide one port-neutral capability. A port may provide multiple groups
# for a capability, for example multiple runtime-selectable audio backends.
# Header-only capabilities are supported as well.
#
# Usage:
#   btstack_declare_port_capability(libusb
#       CAPABILITY audio
#       GROUPS platform-posix-audio-portaudio)
function(btstack_declare_port_capability port)
    cmake_parse_arguments(ARG "" "CAPABILITY" "GROUPS;SOURCES;PUBLIC_INCLUDE_DIRECTORIES" ${ARGN})

    list(FIND btstack_declared_ports "${port}" port_index)
    if(port_index EQUAL -1)
        message(FATAL_ERROR "BTstack port '${port}' must be declared before its providers")
    endif()
    if(NOT ARG_CAPABILITY)
        message(FATAL_ERROR "btstack_declare_port_capability(${port}) requires CAPABILITY")
    endif()
    if(NOT ARG_GROUPS AND NOT ARG_SOURCES AND NOT ARG_PUBLIC_INCLUDE_DIRECTORIES)
        message(FATAL_ERROR
            "btstack_declare_port_capability(${port}) requires GROUPS, SOURCES, or PUBLIC_INCLUDE_DIRECTORIES")
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

    set(provider_include_directories_variable
        "${provider_variable}_public_include_directories")
    set(provider_include_directories
        ${${provider_include_directories_variable}}
        ${ARG_PUBLIC_INCLUDE_DIRECTORIES})
    list(REMOVE_DUPLICATES provider_include_directories)
    set(${provider_include_directories_variable}
        "${provider_include_directories}" PARENT_SCOPE)

    set(provider_sources_variable "${provider_variable}_sources")
    set(provider_sources ${${provider_sources_variable}} ${ARG_SOURCES})
    list(REMOVE_DUPLICATES provider_sources)
    set(${provider_sources_variable} "${provider_sources}" PARENT_SCOPE)
    set(${provider_variable}_declared TRUE PARENT_SCOPE)
    get_property(port_capabilities GLOBAL PROPERTY "btstack_info_${port_suffix}_capabilities")
    list(APPEND port_capabilities "${ARG_CAPABILITY}")
    list(REMOVE_DUPLICATES port_capabilities)
    set_property(GLOBAL PROPERTY "btstack_info_${port_suffix}_capabilities" "${port_capabilities}")
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
    list(FIND btstack_declared_applications "${example}" example_index)
    if(example_index EQUAL -1)
        message(FATAL_ERROR "BTstack application '${example}' has not been declared")
    endif()

    set(source_groups)
    set(configuration_requirements)
    foreach(requirement IN LISTS btstack_application_${example}_needs)
        list(FIND btstack_source_groups "${requirement}" group_index)
        if(NOT group_index EQUAL -1)
            list(APPEND source_groups "${requirement}")
            string(REPLACE "-" "_" requirement_suffix "${requirement}")
            list(APPEND configuration_requirements
                ${btstack_group_configuration_requirements_${requirement_suffix}})
        elseif(requirement MATCHES "^ENABLE_[A-Z0-9_]+$")
            list(APPEND configuration_requirements "${requirement}")
        elseif(NOT "${port}" STREQUAL "")
            string(REPLACE "-" "_" port_suffix "${port}")
            string(REPLACE "-" "_" capability_suffix "${requirement}")
            set(provider_variable "btstack_port_${port_suffix}_provider_${capability_suffix}")
            if(DEFINED ${provider_variable}_declared)
                list(APPEND source_groups ${${provider_variable}})
            else()
                message(FATAL_ERROR
                    "BTstack application '${example}' needs capability '${requirement}', "
                    "but port '${port}' does not provide it")
            endif()
        else()
            message(FATAL_ERROR
                "BTstack application '${example}' has unknown requirement '${requirement}'. "
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

# Collect one metadata property supplied by port capabilities requested by an
# example. This supports header-only and port-local-source capabilities.
function(btstack_collect_example_port_provider_property output_variable port example property)
    list(FIND btstack_declared_ports "${port}" port_index)
    if(port_index EQUAL -1)
        message(FATAL_ERROR "BTstack port '${port}' has not been declared")
    endif()
    list(FIND btstack_declared_applications "${example}" example_index)
    if(example_index EQUAL -1)
        message(FATAL_ERROR "BTstack application '${example}' has not been declared")
    endif()

    string(REPLACE "-" "_" port_suffix "${port}")
    set(values)
    foreach(requirement IN LISTS btstack_application_${example}_needs)
        list(FIND btstack_source_groups "${requirement}" group_index)
        if(NOT group_index EQUAL -1 OR requirement MATCHES "^ENABLE_[A-Z0-9_]+$")
            continue()
        endif()

        string(REPLACE "-" "_" capability_suffix "${requirement}")
        set(provider_variable "btstack_port_${port_suffix}_provider_${capability_suffix}")
        if(NOT DEFINED ${provider_variable}_declared)
            message(FATAL_ERROR
                "BTstack application '${example}' needs capability '${requirement}', "
                "but port '${port}' does not provide it")
        endif()
        list(APPEND values ${${provider_variable}_${property}})
    endforeach()

    list(REMOVE_DUPLICATES values)
    set(${output_variable} "${values}" PARENT_SCOPE)
endfunction()

# Collect public include directories supplied by port capabilities requested by
# an example.
function(btstack_collect_example_port_public_include_directories output_variable port example)
    btstack_collect_example_port_provider_property(${output_variable}
        "${port}" "${example}" public_include_directories)
    set(${output_variable} "${${output_variable}}" PARENT_SCOPE)
endfunction()

# Collect port-local sources supplied by capabilities requested by an example.
function(btstack_collect_example_port_sources output_variable port example)
    btstack_collect_example_port_provider_property(${output_variable}
        "${port}" "${example}" sources)
    set(${output_variable} "${${output_variable}}" PARENT_SCOPE)
endfunction()

# Filter candidates to examples enabled by a port's preprocessed
# btstack_config.h and declared port providers. An unavailable port provider,
# such as optional audio support, also makes an example ineligible rather than
# an error.
#
# Usage:
#   btstack_filter_examples_for_port(EXAMPLES
#       PORT posix-h4-atwilc3000
#       CANDIDATES ${btstack_declared_examples})
function(btstack_filter_examples_for_port output_variable)
    cmake_parse_arguments(ARG "" "PORT" "CANDIDATES" ${ARGN})

    if(NOT ARG_PORT)
        message(FATAL_ERROR "btstack_filter_examples_for_port requires PORT")
    endif()
    list(FIND btstack_declared_ports "${ARG_PORT}" port_index)
    if(port_index EQUAL -1)
        message(FATAL_ERROR "BTstack port '${ARG_PORT}' has not been declared")
    endif()

    string(REPLACE "-" "_" port_suffix "${ARG_PORT}")
    set(configuration_macros
        ${btstack_port_${port_suffix}_configuration_macros})
    set(selected_examples)
    set_property(GLOBAL PROPERTY "btstack_info_${port_suffix}_examples" "${ARG_CANDIDATES}")
    foreach(example IN LISTS ARG_CANDIDATES)
        list(FIND btstack_declared_applications "${example}" example_index)
        if(example_index EQUAL -1)
            message(FATAL_ERROR "BTstack application '${example}' has not been declared")
        endif()

        set(supported TRUE)
        set(missing_capabilities)
        set(missing_configuration)
        foreach(requirement IN LISTS btstack_application_${example}_needs)
            list(FIND btstack_source_groups "${requirement}" group_index)
            if(NOT group_index EQUAL -1 OR requirement MATCHES "^ENABLE_[A-Z0-9_]+$")
                continue()
            endif()

            string(REPLACE "-" "_" capability_suffix "${requirement}")
            set(provider_variable
                "btstack_port_${port_suffix}_provider_${capability_suffix}")
            if(NOT DEFINED ${provider_variable}_declared)
                set(supported FALSE)
                list(APPEND missing_capabilities "${requirement}")
                break()
            endif()
        endforeach()

        if(supported)
            btstack_resolve_example_for_port(${ARG_PORT} ${example}
                example_groups example_configuration_requirements)
            btstack_collect_group_configuration_requirements(group_configuration_requirements
                GROUPS ${example_groups})
            list(APPEND example_configuration_requirements
                ${group_configuration_requirements})
            list(REMOVE_DUPLICATES example_configuration_requirements)
            foreach(configuration_requirement IN LISTS example_configuration_requirements)
                list(FIND configuration_macros "${configuration_requirement}" configuration_index)
                if(configuration_index EQUAL -1)
                    set(supported FALSE)
                    list(APPEND missing_configuration "${configuration_requirement}")
                endif()
            endforeach()
        endif()

        list(REMOVE_DUPLICATES missing_capabilities)
        list(REMOVE_DUPLICATES missing_configuration)
        string(REPLACE "-" "_" example_suffix "${example}")
        set_property(GLOBAL PROPERTY "btstack_info_${port_suffix}_${example_suffix}_selected" "${supported}")
        set_property(GLOBAL PROPERTY "btstack_info_${port_suffix}_${example_suffix}_missing_capabilities" "${missing_capabilities}")
        set_property(GLOBAL PROPERTY "btstack_info_${port_suffix}_${example_suffix}_missing_configuration" "${missing_configuration}")

        if(supported)
            list(APPEND selected_examples "${example}")
        endif()
    endforeach()

    set(${output_variable} "${selected_examples}" PARENT_SCOPE)
endfunction()

# Application terminology for external projects; example names remain supported.
function(btstack_filter_applications_for_port output_variable)
    btstack_filter_examples_for_port(result ${ARGN})
    set(${output_variable} "${result}" PARENT_SCOPE)
endfunction()

function(btstack_resolve_application application groups_output_variable configuration_output_variable)
    btstack_resolve_example("${application}" groups configuration)
    set(${groups_output_variable} "${groups}" PARENT_SCOPE)
    set(${configuration_output_variable} "${configuration}" PARENT_SCOPE)
endfunction()

function(btstack_resolve_application_for_port port application groups_output_variable configuration_output_variable)
    btstack_resolve_example_for_port("${port}" "${application}" groups configuration)
    set(${groups_output_variable} "${groups}" PARENT_SCOPE)
    set(${configuration_output_variable} "${configuration}" PARENT_SCOPE)
endfunction()

# Add configure-time introspection targets for one port. Invoke this after the
# port's example loop so example-info can report its complete source closure.
# Example usage:
#   cmake --build build --target port-info
#   EXAMPLE=gatt_counter cmake --build build --target example-info
function(btstack_add_info_targets)
    cmake_parse_arguments(ARG "" "PORT" "" ${ARGN})
    if(NOT ARG_PORT)
        message(FATAL_ERROR "btstack_add_info_targets requires PORT")
    endif()
    if(NOT TARGET group-graph)
        find_program(BTSTACK_GRAPHVIZ_DOT dot)
        set(graph_output "${CMAKE_CURRENT_BINARY_DIR}/btstack-groups.dot")
        set(graph_arguments "-DOUTPUT_FILE=${graph_output}")
        set(graph_byproducts "${graph_output}")
        if(BTSTACK_GRAPHVIZ_DOT)
            list(APPEND graph_arguments "-DSVG_FILE=${CMAKE_CURRENT_BINARY_DIR}/btstack-groups.svg"
                "-DBTSTACK_GRAPHVIZ_DOT=${BTSTACK_GRAPHVIZ_DOT}")
            list(APPEND graph_byproducts "${CMAKE_CURRENT_BINARY_DIR}/btstack-groups.svg")
        endif()
        add_custom_target(group-graph
            COMMAND "${CMAKE_COMMAND}" ${graph_arguments} -P "${btstack_group_graph_script}"
            BYPRODUCTS ${graph_byproducts}
            COMMENT "Export all BTstack resolver group dependencies"
            VERBATIM)
    endif()
    string(REPLACE "-" "_" port_suffix "${ARG_PORT}")
    get_property(examples GLOBAL PROPERTY "btstack_info_${port_suffix}_examples")
    if(NOT examples)
        message(FATAL_ERROR "No filtered examples registered for BTstack port '${ARG_PORT}'")
    endif()
    get_property(capabilities GLOBAL PROPERTY "btstack_info_${port_suffix}_capabilities")
    set(configuration_macros ${btstack_port_${port_suffix}_configuration_macros})

    set(info_file "${CMAKE_CURRENT_BINARY_DIR}/btstack-info-${ARG_PORT}.cmake")
    file(WRITE "${info_file}" "set(BTSTACK_INFO_PORT [==[${ARG_PORT}]==])\n")
    file(APPEND "${info_file}" "set(BTSTACK_INFO_CONFIGURATION_MACROS [==[${configuration_macros}]==])\n")
    file(APPEND "${info_file}" "set(BTSTACK_INFO_CAPABILITIES [==[${capabilities}]==])\n")
    file(APPEND "${info_file}" "set(BTSTACK_INFO_EXAMPLES [==[${examples}]==])\n")
    foreach(example IN LISTS examples)
        string(REPLACE "-" "_" example_suffix "${example}")
        get_property(selected GLOBAL PROPERTY "btstack_info_${port_suffix}_${example_suffix}_selected")
        get_property(missing_configuration GLOBAL PROPERTY
            "btstack_info_${port_suffix}_${example_suffix}_missing_configuration")
        get_property(missing_capabilities GLOBAL PROPERTY
            "btstack_info_${port_suffix}_${example_suffix}_missing_capabilities")
        get_property(sources GLOBAL PROPERTY "btstack_info_${port_suffix}_${example_suffix}_sources")
        file(APPEND "${info_file}" "set(BTSTACK_INFO_${example_suffix}_STATUS [==[${selected}]==])\n")
        file(APPEND "${info_file}" "set(BTSTACK_INFO_${example_suffix}_MISSING_CONFIGURATION [==[${missing_configuration}]==])\n")
        file(APPEND "${info_file}" "set(BTSTACK_INFO_${example_suffix}_MISSING_CAPABILITIES [==[${missing_capabilities}]==])\n")
        file(APPEND "${info_file}" "set(BTSTACK_INFO_${example_suffix}_SOURCES [==[${sources}]==])\n")
        get_property(include_directories GLOBAL PROPERTY
            "btstack_info_${port_suffix}_${example_suffix}_include_directories")
        file(APPEND "${info_file}" "set(BTSTACK_INFO_${example_suffix}_INCLUDE_DIRECTORIES [==[${include_directories}]==])\n")
    endforeach()

    if(NOT TARGET list-examples)
        add_custom_target(list-examples
            COMMAND "${CMAKE_COMMAND}" -P "${btstack_list_examples_script}"
            COMMENT "List BTstack examples grouped by radio requirements")
    endif()

    add_custom_target(port-info
        COMMAND "${CMAKE_COMMAND}" -DINFO_FILE="${info_file}" -DINFO_MODE=port -P "${btstack_info_script}"
        COMMENT "Display BTstack port configuration")
    add_custom_target(example-info
        COMMAND "${CMAKE_COMMAND}" -DINFO_FILE="${info_file}" -DINFO_MODE=example -P "${btstack_info_script}"
        COMMENT "Display BTstack example information")
    add_custom_target(list-applications
        COMMAND "${CMAKE_COMMAND}" -DINFO_FILE="${info_file}" -DINFO_MODE=list-applications -P "${btstack_info_script}"
        COMMENT "List declared BTstack applications for this port")
    add_custom_target(application-info
        COMMAND "${CMAKE_COMMAND}" -DINFO_FILE="${info_file}" -DINFO_MODE=application -P "${btstack_info_script}"
        COMMENT "Display BTstack application information")
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

# Create a reusable object target for one source group. This is an internal
# building block for btstack_add_application_library, but remains separate
# so groups shared by multiple example closures compile only once per port.
function(btstack_add_scoped_group_object_library)
    cmake_parse_arguments(ARG ""
        "OUTPUT_TARGET;TARGET_PREFIX;GROUP"
        "PUBLIC_INCLUDE_DIRECTORIES;PRIVATE_INCLUDE_DIRECTORIES;PRIVATE_COMPILE_OPTIONS"
        ${ARGN})

    foreach(required OUTPUT_TARGET TARGET_PREFIX GROUP)
        if(NOT ARG_${required})
            message(FATAL_ERROR "btstack_add_scoped_group_object_library requires ${required}")
        endif()
    endforeach()

    list(FIND btstack_source_groups "${ARG_GROUP}" group_index)
    if(group_index EQUAL -1)
        message(FATAL_ERROR
            "btstack_add_scoped_group_object_library has unknown source group '${ARG_GROUP}'")
    endif()

    string(REPLACE "-" "_" group_suffix "${ARG_GROUP}")
    set(target "${ARG_TARGET_PREFIX}_group_${group_suffix}_objects")
    if(NOT TARGET ${target})
        set(group_relative_sources ${btstack_sources_${group_suffix}})
        set(exclusion_variable "btstack_group_object_excluded_sources_${group_suffix}")
        if(DEFINED ${exclusion_variable})
            list(REMOVE_ITEM group_relative_sources ${${exclusion_variable}})
        endif()

        if(NOT group_relative_sources)
            set(${ARG_OUTPUT_TARGET} "" PARENT_SCOPE)
            return()
        endif()

        set(group_sources)
        set(group_is_third_party TRUE)
        foreach(source IN LISTS group_relative_sources)
            list(APPEND group_sources "${btstack_cmake_root}/${source}")
            if(NOT source MATCHES "^3rd-party/")
                set(group_is_third_party FALSE)
            endif()
        endforeach()
        # Object targets compile one group at a time, but that group's source
        # files may include headers owned by its declared dependencies.
        btstack_resolve_groups(group_include_groups GROUPS ${ARG_GROUP})
        btstack_collect_include_directories(group_include_directories
            GROUPS ${group_include_groups}
        )

        add_library(${target} OBJECT ${group_sources})
        # Vendored code should not impose its warning debt on BTstack users.
        # Derive this from the source paths so groups such as lwip-core are
        # covered too. Chipset and platform implementations remain strict.
        if(group_is_third_party)
            if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
                target_compile_options(${target} PRIVATE -w)
            elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
                target_compile_options(${target} PRIVATE /w)
            endif()
        else()
            btstack_target_warnings(${target} STRICT)
        endif()
        target_compile_options(${target} PRIVATE ${ARG_PRIVATE_COMPILE_OPTIONS})
        target_include_directories(${target} PRIVATE
            ${ARG_PUBLIC_INCLUDE_DIRECTORIES}
            ${ARG_PRIVATE_INCLUDE_DIRECTORIES}
            ${group_include_directories}
        )
    endif()
    set(${ARG_OUTPUT_TARGET} "${target}" PARENT_SCOPE)
endfunction()

# Create a static BTstack library for one example's resolved source closure.
# Port policy remains with the caller: it supplies the baseline source groups,
# port-local sources, and all target include/link settings.
#
# Usage:
#   btstack_add_application_library(
#       TARGET btstack_gatt_counter
#       PORT libusb
#       APPLICATION gatt_counter
#       BASE_GROUPS ${BTSTACK_LIBUSB_BASE_GROUPS}
#       PORT_SOURCES ${SOURCES_PORT}
#       PUBLIC_INCLUDE_DIRECTORIES ${BTSTACK_PUBLIC_INCLUDE_DIRECTORIES}
#       PRIVATE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}"
#       LINK_LIBRARIES ${CMAKE_THREAD_LIBS_INIT} m
#   )
#
# CONFIGURATION_REQUIREMENTS_OUTPUT is optional. If set, it receives the
# example's unresolved ENABLE_* requirements for future port validation.
# Each source group is compiled once per port and shared between applications.
# Calls for the same port must use the same compilation settings.
function(btstack_add_application_library)
    cmake_parse_arguments(ARG ""
        "TARGET;PORT;APPLICATION;CONFIGURATION_REQUIREMENTS_OUTPUT"
        "BASE_GROUPS;PORT_SOURCES;PUBLIC_INCLUDE_DIRECTORIES;PRIVATE_INCLUDE_DIRECTORIES;PRIVATE_COMPILE_OPTIONS;LINK_LIBRARIES;LINK_DIRECTORIES"
        ${ARGN})

    foreach(required TARGET PORT APPLICATION)
        if(NOT ARG_${required})
            message(FATAL_ERROR "btstack_add_application_library requires ${required}")
        endif()
    endforeach()

    # Shared group targets must not silently reuse a different configuration.
    # Directory properties also affect object compilation independently of the
    # explicit arguments, so include them in the per-port comparison.
    get_directory_property(configuration_directory_includes INCLUDE_DIRECTORIES)
    get_directory_property(configuration_directory_definitions COMPILE_DEFINITIONS)
    get_directory_property(configuration_directory_options COMPILE_OPTIONS)
    string(SHA256 port_key "${ARG_PORT}")
    foreach(setting IN ITEMS public_includes private_includes compile_options
            directory_includes directory_definitions directory_options)
        if(setting STREQUAL "public_includes")
            set(configuration_${setting} "${ARG_PUBLIC_INCLUDE_DIRECTORIES}")
        elseif(setting STREQUAL "private_includes")
            set(configuration_${setting} "${ARG_PRIVATE_INCLUDE_DIRECTORIES}")
        elseif(setting STREQUAL "compile_options")
            set(configuration_${setting} "${ARG_PRIVATE_COMPILE_OPTIONS}")
        endif()
        set(property "btstack_application_${port_key}_${setting}")
        get_property(configured GLOBAL PROPERTY "${property}" SET)
        if(configured)
            get_property(previous GLOBAL PROPERTY "${property}")
            if(NOT "${previous}" STREQUAL "${configuration_${setting}}")
                message(FATAL_ERROR
                    "btstack_add_application_library: conflicting ${setting} for port '${ARG_PORT}'")
            endif()
        else()
            set_property(GLOBAL PROPERTY "${property}" "${configuration_${setting}}")
        endif()
    endforeach()

    btstack_resolve_example_for_port(${ARG_PORT} ${ARG_APPLICATION}
        example_groups
        example_configuration_requirements
    )
    btstack_collect_example_port_public_include_directories(
        example_port_public_include_directories ${ARG_PORT} ${ARG_APPLICATION})
    btstack_collect_example_port_sources(example_port_sources
        ${ARG_PORT} ${ARG_APPLICATION})
    btstack_filter_groups_for_port(active_base_groups
        PORT ${ARG_PORT}
        GROUPS ${ARG_BASE_GROUPS})
    set(requested_library_groups
        ${example_groups}
        ${active_base_groups}
    )
    btstack_resolve_groups(library_groups GROUPS ${requested_library_groups})
    btstack_collect_include_directories(example_include_directories
        GROUPS ${library_groups}
    )
    btstack_collect_consumer_include_directories(example_consumer_include_directories
        GROUPS ${library_groups}
    )

    set(example_library_sources ${ARG_PORT_SOURCES} ${example_port_sources})
    foreach(group IN LISTS library_groups)
        btstack_add_scoped_group_object_library(
            OUTPUT_TARGET group_object_target
            TARGET_PREFIX "btstack_${ARG_PORT}"
            GROUP "${group}"
            PUBLIC_INCLUDE_DIRECTORIES ${ARG_PUBLIC_INCLUDE_DIRECTORIES}
            PRIVATE_INCLUDE_DIRECTORIES ${ARG_PRIVATE_INCLUDE_DIRECTORIES}
            PRIVATE_COMPILE_OPTIONS ${ARG_PRIVATE_COMPILE_OPTIONS}
        )
        if(group_object_target)
            list(APPEND example_library_sources "$<TARGET_OBJECTS:${group_object_target}>")
        endif()
    endforeach()

    # Retain the resolved source list for the example-info target. Include the
    # application sources as well as all port and BTstack implementation files.
    btstack_collect_sources(info_library_sources
        GROUPS ${library_groups})
    set(info_port_sources)
    foreach(source IN LISTS ARG_PORT_SOURCES example_port_sources)
        if(IS_ABSOLUTE "${source}")
            list(APPEND info_port_sources "${source}")
        else()
            list(APPEND info_port_sources "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
        endif()
    endforeach()
    set(info_sources ${info_port_sources} ${info_library_sources})
    btstack_get_application_property(application_sources "${ARG_APPLICATION}" SOURCES)
    list(APPEND info_sources ${application_sources})
    list(REMOVE_DUPLICATES info_sources)
    list(SORT info_sources)
    string(REPLACE "-" "_" info_port_suffix "${ARG_PORT}")
    string(REPLACE "-" "_" info_example_suffix "${ARG_APPLICATION}")
    set_property(GLOBAL PROPERTY
        "btstack_info_${info_port_suffix}_${info_example_suffix}_sources" "${info_sources}")

    add_library(${ARG_TARGET} STATIC ${example_library_sources})
    btstack_target_warnings(${ARG_TARGET} STRICT)
    target_compile_options(${ARG_TARGET} PRIVATE ${ARG_PRIVATE_COMPILE_OPTIONS})
    if(ARG_PUBLIC_INCLUDE_DIRECTORIES OR example_port_public_include_directories
            OR example_consumer_include_directories)
        target_include_directories(${ARG_TARGET} PUBLIC
            ${ARG_PUBLIC_INCLUDE_DIRECTORIES}
            ${example_port_public_include_directories}
            ${example_consumer_include_directories})
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

    get_target_property(info_include_directories ${ARG_TARGET} INCLUDE_DIRECTORIES)
    if(info_include_directories STREQUAL "info_include_directories-NOTFOUND")
        set(info_include_directories)
    endif()
    set(info_normalized_include_directories)
    foreach(include_directory IN LISTS info_include_directories)
        if(include_directory MATCHES "^\\$<")
            list(APPEND info_normalized_include_directories "${include_directory}")
        else()
            get_filename_component(normalized_include_directory
                "${include_directory}" ABSOLUTE)
            list(APPEND info_normalized_include_directories "${normalized_include_directory}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES info_normalized_include_directories)
    list(SORT info_normalized_include_directories)
    set_property(GLOBAL PROPERTY
        "btstack_info_${info_port_suffix}_${info_example_suffix}_include_directories"
        "${info_normalized_include_directories}")

    if(ARG_CONFIGURATION_REQUIREMENTS_OUTPUT)
        set(${ARG_CONFIGURATION_REQUIREMENTS_OUTPUT}
            "${example_configuration_requirements}" PARENT_SCOPE)
    endif()
endfunction()
