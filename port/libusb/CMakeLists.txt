cmake_minimum_required (VERSION 3.12)

SET(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(BTstack-libusb)

SET(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)

# Get main BTstack CMake helper
include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Set up port
btstack_declare_port(libusb)
btstack_collect_public_include_directories(BTSTACK_PUBLIC_INCLUDE_DIRECTORIES
 ROOT "${BTSTACK_ROOT}"
)
# Port specific files
set(SOURCES_PORT
    main.c
)

# Get libraries needed for this port

# libusb: needs pkgconfig
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

# pthread
find_package(Threads)

# portaudio
pkg_check_modules(PORTAUDIO portaudio-2.0)
if(PORTAUDIO_FOUND)
    message("HAVE_PORTAUDIO")
    add_compile_definitions(HAVE_PORTAUDIO)
    btstack_declare_port_provider(libusb
        CAPABILITY audio
        GROUPS platform-posix-audio-portaudio platform-posix-wav
    )
endif()
btstack_declare_port_provider(libusb
    CAPABILITY led
    GROUPS platform-posix-hal-led
    PUBLIC_INCLUDE_DIRECTORIES "${BTSTACK_ROOT}/platform/embedded"
)

# Use the C preprocessor to obtain the effective BTstack configuration macros.
# This lets future resolver work use the same conditional configuration that
# compilation sees, without requiring btstack_config.h to be generated.
set(BTSTACK_CONFIG_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${BTSTACK_CONFIG_HEADER}"
    "${BTSTACK_ROOT}/tool/extract_btstack_config.py"
)
set(BTSTACK_CONFIG_PREPROCESSOR_ARGS)
if(PORTAUDIO_FOUND)
    # Keep this invocation consistent with add_compile_definitions above.
    list(APPEND BTSTACK_CONFIG_PREPROCESSOR_ARGS --define HAVE_PORTAUDIO)
endif()
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${BTSTACK_ROOT}/tool/extract_btstack_config.py"
        --compiler "${CMAKE_C_COMPILER}"
        --config "${BTSTACK_CONFIG_HEADER}"
        ${BTSTACK_CONFIG_PREPROCESSOR_ARGS}
    RESULT_VARIABLE BTSTACK_CONFIG_PREPROCESS_RESULT
    OUTPUT_VARIABLE BTSTACK_CONFIG_MACROS
    ERROR_VARIABLE BTSTACK_CONFIG_PREPROCESS_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT BTSTACK_CONFIG_PREPROCESS_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Could not preprocess ${BTSTACK_CONFIG_HEADER}:\n${BTSTACK_CONFIG_PREPROCESS_ERROR}")
endif()
if(BTSTACK_CONFIG_MACROS)
    string(REPLACE "\n" ";" BTSTACK_CONFIG_MACROS "${BTSTACK_CONFIG_MACROS}")
    string(JOIN ", " BTSTACK_CONFIG_MACROS_DISPLAY ${BTSTACK_CONFIG_MACROS})
else()
    set(BTSTACK_CONFIG_MACROS)
    set(BTSTACK_CONFIG_MACROS_DISPLAY "(none)")
endif()
message(STATUS "BTstack config ENABLE_*/HAVE_* macros: ${BTSTACK_CONFIG_MACROS_DISPLAY}")


# Get the declared examples. Feature-based filtering will be provided by the
# future resolver; for now this port builds every declared example.
include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_examples})

# Common baseline selected by the libusb port's main.c and shared by every
# example. This dual-mode port enables BLE in btstack_config.h. All Classic
# examples support pairing, and cross-transport key derivation requires SM.
set(BTSTACK_LIBUSB_GROUPS
    platform-posix-run-loop
    platform-posix-signal
    platform-posix-stdin
    ble
    le-device-db-tlv
    link-key-db-tlv
    tlv-none
    platform-posix-tlv
    platform-posix-hci-dump
    platform-libusb-h2
    chipset-realtek
    chipset-zephyr
    third-party-micro-ecc
    third-party-rijndael
)
if(PORTAUDIO_FOUND)
    # main.c selects the PortAudio instances when HAVE_PORTAUDIO is enabled.
    list(APPEND BTSTACK_LIBUSB_GROUPS platform-posix-audio-portaudio)
endif()
# Every selected example is defined with its resolved source closure. EXAMPLES
# is derived from the shared requirements manifest and any port exclusions.
foreach(EXAMPLE IN LISTS EXAMPLES)
    btstack_add_scoped_example_library(
        TARGET btstack_${EXAMPLE}_library
        ROOT "${BTSTACK_ROOT}"
        PORT libusb
        EXAMPLE ${EXAMPLE}
        GROUP_OBJECT_TARGET_PREFIX btstack_libusb
        BASE_GROUPS ${BTSTACK_LIBUSB_GROUPS}
        PORT_SOURCES ${SOURCES_PORT}
        PUBLIC_INCLUDE_DIRECTORIES
            ${BTSTACK_PUBLIC_INCLUDE_DIRECTORIES}
            # btstack_config.h is the port configuration seen by applications.
            "${CMAKE_CURRENT_SOURCE_DIR}"
            ${LIBUSB_INCLUDE_DIRS}
            ${PORTAUDIO_INCLUDE_DIRS}
        LINK_DIRECTORIES
            ${LIBUSB_LIBRARY_DIRS}
            ${PORTAUDIO_LIBRARY_DIRS}
        LINK_LIBRARIES
            ${LIBUSB_LIBRARIES}
            ${CMAKE_THREAD_LIBS_INIT}
            m
            ${PORTAUDIO_LIBRARIES}
    )

    # Construct full path to the example sources from its declaration.
    set(SOURCES_EXAMPLE)
    foreach(EXAMPLE_SOURCE IN LISTS btstack_example_${EXAMPLE}_sources)
        list(APPEND SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/${EXAMPLE_SOURCE})
    endforeach()

    # Define example target
    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE} )

    # Compile GATT DB if needed
    set(EXAMPLE_GATT_DB ${btstack_example_${EXAMPLE}_gatt_db})
    if (EXAMPLE_GATT_DB)
        message("example ${EXAMPLE} -- with GATT DB")
        btstack_target_gatt(${EXAMPLE} "${BTSTACK_ROOT}/example/${EXAMPLE_GATT_DB}")
    else()
        message("example ${EXAMPLE}")
    endif()

    # Configure warnings
    btstack_target_warnings(${EXAMPLE} STRICT)

    # Add all libraries
    target_link_libraries(${EXAMPLE} btstack_${EXAMPLE}_library)
endforeach()
