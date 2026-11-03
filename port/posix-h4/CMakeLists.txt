cmake_minimum_required (VERSION 3.18)

project(BTstack-posix-h4)

SET(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)

# Enable undefined and address sanitizers
#  add_compile_options(
#      -fsanitize=undefined -fsanitize=address
#  )
#  add_link_options(
#      -fsanitize=undefined -fsanitize=address
#  )

# BTstack library source groups. Port-local and example utility sources remain
# below because they are selected by this port.
include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)
btstack_declare_port(posix-h4)
btstack_collect_public_include_directories(BTSTACK_PUBLIC_INCLUDE_DIRECTORIES
  ROOT "${BTSTACK_ROOT}"
)
set(BTSTACK_LIBRARY_GROUPS
    audio-demo
    ble
    bnep
    classic
    core
    gatt-services
    lc3
    le-audio
    le-device-db-tlv
    link-key-db-tlv
    lwip
    mesh
    tlv-none
    transport-h4
    platform-posix
    chipset-bcm
    chipset-csr
    chipset-em9301
    chipset-realtek
    chipset-stlc2500d
    chipset-tc3566x
    chipset-zephyr
    third-party-bluedroid-sbc
    third-party-hxcmod
    third-party-lc3-google
    third-party-md5
    third-party-micro-ecc
    third-party-qr-code-generator
    third-party-rijndael
    third-party-yxml
)
btstack_collect_sources(SOURCES_BTSTACK
 ROOT "${BTSTACK_ROOT}"
 GROUPS ${BTSTACK_LIBRARY_GROUPS}
)
btstack_collect_include_directories(BTSTACK_INCLUDE_DIRECTORIES
 ROOT "${BTSTACK_ROOT}"
 GROUPS ${BTSTACK_LIBRARY_GROUPS}
)

# Port specific files
set(SOURCES_PORT
    main.c
    bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c
    bluetooth_init_cc2564C_1.5.c
)

# Complete list of sources
set(SOURCES
  ${SOURCES_PORT}
  ${SOURCES_BTSTACK}
)
list(SORT SOURCES)

# create static 'btstack' lib
add_library(btstack STATIC ${SOURCES})
btstack_target_warnings(btstack STRICT)
target_include_directories(btstack
  PUBLIC
    ${BTSTACK_PUBLIC_INCLUDE_DIRECTORIES}
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${BTSTACK_ROOT}/platform/embedded"
    "${BTSTACK_ROOT}/chipset/cc256x"
    ${BTSTACK_INCLUDE_DIRECTORIES}
)

# Get libraries needed for this port

# portaudio: needs pkgconfig
find_package(PkgConfig)

if (PkgConfig_FOUND)
    pkg_check_modules(PORTAUDIO portaudio-2.0)
    if(PORTAUDIO_FOUND)
        message("HAVE_PORTAUDIO")
        target_include_directories(btstack PUBLIC ${PORTAUDIO_INCLUDE_DIRS})
        target_link_directories(btstack PUBLIC ${PORTAUDIO_LIBRARY_DIRS})
        target_link_libraries(btstack ${PORTAUDIO_LIBRARIES})
        add_compile_definitions(HAVE_PORTAUDIO)
        btstack_declare_port_provider(posix-h4
            CAPABILITY audio
            GROUPS platform-posix-audio-portaudio
        )
    endif()
endif()

# pthread
find_package(Threads)
target_link_libraries(btstack ${CMAKE_THREAD_LIBS_INIT})

# libm
target_link_libraries(btstack m)


# Add CC256x Support and specify init script
set (CC256X_INIT_SCRIPT bluetooth_init_cc2564C_1.5.c)
include(${BTSTACK_ROOT}/chipset/cc256x/cc256x.cmake)
target_sources(btstack PRIVATE
    ${BTSTACK_ROOT}/chipset/cc256x/btstack_chipset_cc256x.c
    ${CC256X_INIT_SCRIPT_SOURCES}
)
add_dependencies(btstack ${CC256X_INIT_SCRIPT_TARGETS})

# Add BCM Support and download PatchRAM files during the build.
include(${BTSTACK_ROOT}/chipset/bcm/btstack_bcm_patchram.cmake)
btstack_add_firmware_download(
    TARGET bcm43430a1_patchram
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${BTSTACK_BCM43430A1_PATCHRAM_FILE}"
    URL "${BTSTACK_BCM43430A1_PATCHRAM_URL}"
    SHA256 "${BTSTACK_BCM43430A1_PATCHRAM_SHA256}"
)
btstack_add_firmware_download(
    TARGET cyw55310_patchram
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${BTSTACK_CYW55310_PATCHRAM_FILE}"
    URL "${BTSTACK_CYW55310_PATCHRAM_URL}"
    SHA256 "${BTSTACK_CYW55310_PATCHRAM_SHA256}"
)
add_dependencies(btstack bcm43430a1_patchram cyw55310_patchram)

# Get the declared examples. Feature-based filtering will be provided by the
# future resolver; for now this port builds every declared example.
include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_examples})
list(REMOVE_ITEM EXAMPLES "mesh_node_demo")

# create targets
foreach(EXAMPLE ${EXAMPLES})
    # Get the example sources from its declaration.
    set(SOURCES_EXAMPLE)
    foreach(EXAMPLE_SOURCE IN LISTS btstack_example_${EXAMPLE}_sources)
        list(APPEND SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/${EXAMPLE_SOURCE})
    endforeach()

    # define example target
    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE})

    # add includes
    target_include_directories(${EXAMPLE}
      PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${BTSTACK_ROOT}/platform/embedded"
        "${BTSTACK_ROOT}/chipset/cc256x"
        ${BTSTACK_INCLUDE_DIRECTORIES}
    )

    # Compile GATT DB if needed
    set(EXAMPLE_GATT_DB ${btstack_example_${EXAMPLE}_gatt_db})
    if (EXAMPLE_GATT_DB)
        message("example ${EXAMPLE} -- with GATT DB")
        btstack_target_gatt(${EXAMPLE} "${BTSTACK_ROOT}/example/${EXAMPLE_GATT_DB}")
    else()
        message("example ${EXAMPLE}")
    endif()
    # configure warnings
    btstack_target_warnings(${EXAMPLE} STRICT)

    # link target with btstack library
    target_link_libraries(${EXAMPLE} btstack pthread)
endforeach(EXAMPLE)
