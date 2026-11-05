# Standalone tests for the checksum-verifying firmware download script.

get_filename_component(BTSTACK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(test_directory "${CMAKE_CURRENT_LIST_DIR}/build")
file(REMOVE_RECURSE "${test_directory}")
file(MAKE_DIRECTORY "${test_directory}")

set(source "${test_directory}/source.bin")
set(output "${test_directory}/output.bin")
file(REMOVE "${output}" "${output}.download")
file(WRITE "${source}" "BTstack firmware test payload\n")
file(SHA256 "${source}" sha256)
file(TO_CMAKE_PATH "${source}" source_path)
set(url "file://${source_path}")

function(run_download expected_result expected_text)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DURL=${url}"
            "-DOUTPUT=${output}"
            "-DSHA256=${ARGN}"
            -P "${BTSTACK_ROOT}/tool/cmake/BTstackDownloadFile.cmake"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE command_output
        ERROR_VARIABLE command_error
    )
    if(NOT result EQUAL expected_result)
        message(FATAL_ERROR "Unexpected downloader result ${result}: ${command_output}${command_error}")
    endif()
    string(FIND "${command_output}${command_error}" "${expected_text}" text_index)
    if(text_index EQUAL -1)
        message(FATAL_ERROR "Downloader output lacks '${expected_text}': ${command_output}${command_error}")
    endif()
endfunction()

run_download(0 "Downloaded and verified firmware" "${sha256}")
file(SHA256 "${output}" output_sha256)
if(NOT output_sha256 STREQUAL sha256)
    message(FATAL_ERROR "Downloaded firmware checksum differs from source")
endif()

file(WRITE "${output}" "corrupt cache\n")
run_download(0 "checksum mismatch" "${sha256}")
file(SHA256 "${output}" output_sha256)
if(NOT output_sha256 STREQUAL sha256)
    message(FATAL_ERROR "Corrupt cached firmware was not replaced")
endif()

run_download(1 "HASH mismatch" "0000000000000000000000000000000000000000000000000000000000000000")

file(REMOVE_RECURSE "${test_directory}")
