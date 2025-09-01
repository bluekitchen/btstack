
idf_component_register(
    SRCS main.c your_project.c
    REQUIRES btstack esp_phy bt)

include(BTstackUtils)

#if gatt files are required, add them here
#target_gatt(${COMPONENT_LIB} ${CMAKE_CURRENT_LIST_DIR}/your_project.gatt)
