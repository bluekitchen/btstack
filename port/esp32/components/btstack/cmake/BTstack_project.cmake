idf_build_get_property(BTSTACK_ROOT BTSTACK_ROOT)
if( NOT DEFINED BTSTACK_ROOT )
    if( NOT DEFINED ENV{BTSTACK_ROOT} )
        set(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)
    else()
        set(BTSTACK_ROOT $ENV{BTSTACK_ROOT})
    endif()
    idf_build_set_property(BTSTACK_ROOT "${BTSTACK_ROOT}")
endif()

list(APPEND EXTRA_COMPONENT_DIRS "${BTSTACK_ROOT}/port/esp32/components/")
list(APPEND CMAKE_MODULE_PATH "${BTSTACK_ROOT}/tool/cmake/")

include(BTstackUtils)
