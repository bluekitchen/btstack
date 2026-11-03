# Standalone tests for the BTstack CMake source-group resolver.
#
# Run directly with:
#   cmake -P test/cmake-resolver/resolver_test.cmake

get_filename_component(BTSTACK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include("${BTSTACK_ROOT}/tool/cmake/BTstack.cmake")

# btstack_declare_example writes its declarations to the caller scope. A macro
# preserves this script's scope while loading the manifest.
macro(load_example_requirements)
    include("${BTSTACK_ROOT}/example/requirements.cmake")
endmacro()
load_example_requirements()

function(expect_failure mode expected_diagnostic)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DTEST_MODE=${mode}" "-P" "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(result EQUAL 0)
        message(FATAL_ERROR "Resolver ${mode} test unexpectedly succeeded")
    endif()

    set(diagnostic "${output}${error}")
    string(FIND "${diagnostic}" "${expected_diagnostic}" diagnostic_index)
    if(diagnostic_index EQUAL -1)
        message(FATAL_ERROR
            "Resolver ${mode} test failed without the expected diagnostic: ${expected_diagnostic}\n"
            "Actual output:\n${diagnostic}")
    endif()
endfunction()

if(DEFINED TEST_MODE)
    if(TEST_MODE STREQUAL "unknown")
        btstack_resolve_groups(resolved GROUPS missing-group)
    elseif(TEST_MODE STREQUAL "cycle")
        # classic-rfcomm -> l2cap -> core. Add the final edge only for this test.
        set(btstack_group_dependencies_core classic-rfcomm)
        btstack_resolve_groups(resolved GROUPS classic-rfcomm)
    else()
        message(FATAL_ERROR "Unknown resolver test mode: ${TEST_MODE}")
    endif()
else()
    btstack_resolve_groups(resolved GROUPS classic-rfcomm)
    if(NOT "${resolved}" STREQUAL "core;l2cap;classic-rfcomm")
        message(FATAL_ERROR "Unexpected resolver closure: ${resolved}")
    endif()

    # Repeated requested groups must not occur in the resolved closure.
    btstack_resolve_groups(resolved GROUPS classic-rfcomm l2cap classic-rfcomm)
    if(NOT "${resolved}" STREQUAL "core;l2cap;classic-rfcomm")
        message(FATAL_ERROR "Resolver did not remove duplicate groups: ${resolved}")
    endif()

    btstack_resolve_example(gatt_counter gatt_counter_groups gatt_counter_configurations)
    if(NOT "${gatt_counter_groups}" STREQUAL "core;l2cap;ble")
        message(FATAL_ERROR "Unexpected gatt_counter source groups: ${gatt_counter_groups}")
    endif()
    if(NOT "${gatt_counter_configurations}" STREQUAL "ENABLE_LE_PERIPHERAL")
        message(FATAL_ERROR
            "Unexpected gatt_counter configuration requirements: ${gatt_counter_configurations}")
    endif()

    expect_failure(unknown "Unknown BTstack source group 'missing-group'")
    expect_failure(cycle "Cyclic BTstack source-group dependencies")
endif()
