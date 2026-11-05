# Download one firmware artifact and verify its SHA-256 digest.
#
# Invoked by BTstackFirmware.cmake at build time with -DURL, -DOUTPUT, and
# -DSHA256. A valid existing output is retained; an invalid one is replaced
# atomically after a successful, verified download.

foreach(required URL OUTPUT SHA256)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "BTstackDownloadFile.cmake requires -D${required}=...")
    endif()
endforeach()

string(LENGTH "${SHA256}" sha256_length)
if(NOT sha256_length EQUAL 64 OR NOT SHA256 MATCHES "^[0-9A-Fa-f]+$")
    message(FATAL_ERROR "SHA256 must be a 64-character hexadecimal digest")
endif()
string(TOLOWER "${SHA256}" SHA256)

if(EXISTS "${OUTPUT}")
    file(SHA256 "${OUTPUT}" output_sha256)
    if(output_sha256 STREQUAL SHA256)
        message(STATUS "Firmware already verified: ${OUTPUT}")
        return()
    endif()
    message(STATUS "Firmware checksum mismatch; replacing ${OUTPUT}")
endif()

get_filename_component(output_directory "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")
set(temporary_output "${OUTPUT}.download")
file(REMOVE "${temporary_output}")

message(STATUS "Downloading firmware ${URL}")
file(DOWNLOAD "${URL}" "${temporary_output}"
    EXPECTED_HASH "SHA256=${SHA256}"
    STATUS download_status
    LOG download_log
)
list(GET download_status 0 download_result)
if(NOT download_result EQUAL 0)
    file(REMOVE "${temporary_output}")
    list(GET download_status 1 download_message)
    message(FATAL_ERROR
        "Firmware download failed for ${URL}: ${download_message}\n${download_log}")
endif()

file(RENAME "${temporary_output}" "${OUTPUT}")
message(STATUS "Downloaded and verified firmware: ${OUTPUT}")
