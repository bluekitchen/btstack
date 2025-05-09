
function(target_gatt TARGETOBJ GATT_FILE)
    get_filename_component(GATT_FILE_NAME ${GATT_FILE} NAME_WLE)
    string(CONCAT OUTPUT_HEADER ${GATT_FILE_NAME} ".h")
    set(OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_HEADER}")

    add_custom_command(
        OUTPUT ${OUTPUT_HEADER}
        DEPENDS ${GATT_FILE}
        COMMAND ${Python_EXECUTABLE}
        ARGS ${BTSTACK_ROOT}/tool/compile_gatt.py ${GATT_FILE} ${OUTPUT_HEADER}
    )

    add_custom_target(MAKE_${GATT_FILE_NAME} ALL DEPENDS ${OUTPUT_HEADER})
    add_dependencies(${TARGETOBJ} MAKE_${GATT_FILE_NAME})
    target_include_directories(${TARGETOBJ} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endfunction()

