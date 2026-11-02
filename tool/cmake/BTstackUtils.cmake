
# Compatibility wrapper. New CMake ports should include BTstack.cmake and use
# btstack_target_gatt directly.
include("${CMAKE_CURRENT_LIST_DIR}/BTstack.cmake")

function(target_gatt target gatt_file)
    btstack_target_gatt(${target} "${gatt_file}" ${ARGN})
endfunction()

