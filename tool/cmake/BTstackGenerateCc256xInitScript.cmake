# Extract a verified CC256x service-pack archive and convert its BTS script
# into a C source file. Invoked at build time by chipset/cc256x/cc256x.cmake.

foreach(required ARCHIVE WORK_DIRECTORY MAIN_SCRIPT OUTPUT PYTHON_EXECUTABLE CONVERSION_SCRIPT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "BTstackGenerateCc256xInitScript.cmake requires -D${required}=...")
    endif()
endforeach()

if(NOT EXISTS "${ARCHIVE}")
    message(FATAL_ERROR "CC256x service-pack archive does not exist: ${ARCHIVE}")
endif()

file(REMOVE_RECURSE "${WORK_DIRECTORY}")
file(MAKE_DIRECTORY "${WORK_DIRECTORY}")
get_filename_component(archive_extension "${ARCHIVE}" LAST_EXT)
if("${archive_extension}" STREQUAL ".zip")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E tar xf "${ARCHIVE}"
        WORKING_DIRECTORY "${WORK_DIRECTORY}"
        RESULT_VARIABLE extract_result
        ERROR_VARIABLE extract_error
    )
    if(NOT extract_result EQUAL 0)
        message(FATAL_ERROR "Could not extract ${ARCHIVE}: ${extract_error}")
    endif()
else()
    # WL init scripts are downloaded as individual BTS files rather than ZIPs.
    get_filename_component(main_bts_directory "${WORK_DIRECTORY}/${MAIN_SCRIPT}" DIRECTORY)
    file(MAKE_DIRECTORY "${main_bts_directory}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy "${ARCHIVE}" "${WORK_DIRECTORY}/${MAIN_SCRIPT}"
        RESULT_VARIABLE copy_result
        ERROR_VARIABLE copy_error
    )
    if(NOT copy_result EQUAL 0)
        message(FATAL_ERROR "Could not stage ${ARCHIVE}: ${copy_error}")
    endif()
endif()

set(main_bts "${WORK_DIRECTORY}/${MAIN_SCRIPT}")
if(NOT EXISTS "${main_bts}")
    message(FATAL_ERROR "CC256x main BTS file does not exist: ${main_bts}")
endif()

set(converter_arguments "${main_bts}")
if(DEFINED OPTIONAL_SCRIPT AND NOT "${OPTIONAL_SCRIPT}" STREQUAL "")
    set(optional_bts "${WORK_DIRECTORY}/${OPTIONAL_SCRIPT}")
    if(NOT EXISTS "${optional_bts}")
        message(FATAL_ERROR "CC256x add-on BTS file does not exist: ${optional_bts}")
    endif()
    list(APPEND converter_arguments "${optional_bts}")
endif()

set(temporary_output "${WORK_DIRECTORY}/init_script.c")
execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" "${CONVERSION_SCRIPT}"
        ${converter_arguments} "${temporary_output}"
    RESULT_VARIABLE convert_result
)
if(NOT convert_result EQUAL 0)
    message(FATAL_ERROR "Could not generate CC256x init script ${OUTPUT}")
endif()

get_filename_component(output_directory "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")
file(RENAME "${temporary_output}" "${OUTPUT}")
