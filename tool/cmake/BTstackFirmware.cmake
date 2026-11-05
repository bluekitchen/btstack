# Build-time, checksum-verified firmware download support for BTstack ports.
#
# Usage:
#   btstack_add_firmware_download(
#       TARGET cyw55310_patchram
#       OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/CYW55310.hcd"
#       URL "https://example.invalid/CYW55310.hcd"
#       SHA256 "<64 lowercase or uppercase hexadecimal characters>"
#   )
#   add_dependencies(my_target cyw55310_patchram)

set(_btstack_firmware_module_directory "${CMAKE_CURRENT_LIST_DIR}")

function(btstack_add_firmware_download)
    cmake_parse_arguments(ARG "" "TARGET;OUTPUT;URL;SHA256" "" ${ARGN})

    foreach(required TARGET OUTPUT URL SHA256)
        if(NOT ARG_${required})
            message(FATAL_ERROR "btstack_add_firmware_download requires ${required}")
        endif()
    endforeach()
    if(TARGET "${ARG_TARGET}")
        message(FATAL_ERROR "Firmware target '${ARG_TARGET}' already exists")
    endif()

    string(LENGTH "${ARG_SHA256}" sha256_length)
    if(NOT sha256_length EQUAL 64 OR NOT ARG_SHA256 MATCHES "^[0-9A-Fa-f]+$")
        message(FATAL_ERROR
            "btstack_add_firmware_download(${ARG_TARGET}) requires a 64-character SHA256")
    endif()

    get_filename_component(output "${ARG_OUTPUT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    add_custom_target("${ARG_TARGET}"
        COMMAND "${CMAKE_COMMAND}"
            "-DURL=${ARG_URL}"
            "-DOUTPUT=${output}"
            "-DSHA256=${ARG_SHA256}"
            -P "${_btstack_firmware_module_directory}/BTstackDownloadFile.cmake"
        BYPRODUCTS "${output}"
        COMMENT "Verify firmware ${output}"
        VERBATIM
    )
    set_property(TARGET "${ARG_TARGET}" PROPERTY BTSTACK_FIRMWARE_OUTPUT "${output}")
endfunction()
