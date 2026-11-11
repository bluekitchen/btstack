# Exercise paths owned by a project outside BTstack's example directory.
btstack_declare_application(project_counter
    SOURCES src/main.c "${BTSTACK_ROOT}/example/gatt_counter.c"
    GATT_DB gatt/service.gatt
    NEEDS gatt-server ENABLE_LE_PERIPHERAL)
