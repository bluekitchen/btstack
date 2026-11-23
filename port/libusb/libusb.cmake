# Temporary workaround for macOS USB issues with libusb 1.0.28 and newer.
# Remove this file and restore the pkg-config dependency when no longer needed.
include(ExternalProject)

set(BTSTACK_LIBUSB "auto" CACHE STRING "libusb selection: auto, bundled, or system")
set_property(CACHE BTSTACK_LIBUSB PROPERTY STRINGS auto bundled system)

function(btstack_configure_libusb target)
    if(NOT BTSTACK_LIBUSB MATCHES "^(auto|bundled|system)$")
        message(FATAL_ERROR "BTSTACK_LIBUSB must be auto, bundled, or system")
    endif()

    set(use_bundled FALSE)
    if(BTSTACK_LIBUSB STREQUAL "bundled")
        if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" OR NOT APPLE OR CMAKE_CROSSCOMPILING)
            message(FATAL_ERROR "Bundled libusb is supported only for native macOS builds")
        endif()
        set(use_bundled TRUE)
        set(reason "explicitly requested")
    elseif(BTSTACK_LIBUSB STREQUAL "auto" AND CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND APPLE AND NOT CMAKE_CROSSCOMPILING)
        execute_process(COMMAND sw_vers -productVersion
            OUTPUT_VARIABLE macos_version OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE sw_vers_result)
        if(NOT sw_vers_result EQUAL 0 OR NOT macos_version MATCHES "^[0-9]+(\\.[0-9]+)*$")
            message(FATAL_ERROR "Could not determine the macOS host version using sw_vers")
        endif()
        if(macos_version VERSION_GREATER_EQUAL "26")
            set(use_bundled TRUE)
            set(reason "macOS ${macos_version}")
        endif()
    endif()

    if(NOT use_bundled)
        message(STATUS "libusb: system pkg-config (${BTSTACK_LIBUSB})")
        pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
        target_include_directories(${target} PUBLIC ${LIBUSB_INCLUDE_DIRS})
        target_link_directories(${target} PUBLIC ${LIBUSB_LIBRARY_DIRS})
        target_link_libraries(${target} ${LIBUSB_LIBRARIES})
        return()
    endif()

    message(STATUS "libusb: bundled static 1.0.27 (${reason})")
    list(LENGTH CMAKE_OSX_ARCHITECTURES arch_count)
    if(arch_count GREATER 1)
        message(FATAL_ERROR "Bundled libusb does not support multiple CMAKE_OSX_ARCHITECTURES; build each architecture separately")
    endif()
    # Autoconf runs independently of CMake, so pass Apple toolchain settings.
    set(usb_flags "")
    if(CMAKE_OSX_ARCHITECTURES)
        string(APPEND usb_flags " -arch ${CMAKE_OSX_ARCHITECTURES}")
    endif()
    if(CMAKE_OSX_SYSROOT)
        string(APPEND usb_flags " -isysroot ${CMAKE_OSX_SYSROOT}")
    endif()
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        string(APPEND usb_flags " -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()

    find_program(BTSTACK_LIBUSB_MAKE NAMES gmake make)
    if(NOT BTSTACK_LIBUSB_MAKE)
        message(FATAL_ERROR "make is required to build bundled libusb")
    endif()
    set(usb_prefix "${CMAKE_CURRENT_BINARY_DIR}/libusb-1.0.27")
    set(usb_archive "${usb_prefix}/lib/libusb-1.0.a")
    # The imported target's include path must exist at CMake generation time.
    file(MAKE_DIRECTORY "${usb_prefix}/include/libusb-1.0")
    # Preserve the release tarball's Autotools timestamps so make does not
    # attempt to regenerate the supplied files using automake.
    set(extract_timestamp_option)
    if(POLICY CMP0135)
        list(APPEND extract_timestamp_option DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
    endif()
    ExternalProject_Add(btstack_libusb_build
        URL https://github.com/libusb/libusb/releases/download/v1.0.27/libusb-1.0.27.tar.bz2
        URL_HASH SHA256=ffaa41d741a8a3bee244ac8e54a72ea05bf2879663c098c82fc5757853441575
        ${extract_timestamp_option}
        PREFIX "${CMAKE_CURRENT_BINARY_DIR}/libusb-build"
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env
            "CC=${CMAKE_C_COMPILER}" "CFLAGS=${usb_flags}" "LDFLAGS=${usb_flags}"
            ac_cv_func_pipe2=no
            <SOURCE_DIR>/configure --prefix=${usb_prefix} --enable-static --disable-shared
        # The release archive already includes generated Autotools files.
        # Existing build directories may have extracted them with equal or
        # reordered timestamps, which otherwise triggers regeneration.
        BUILD_COMMAND ${BTSTACK_LIBUSB_MAKE} ACLOCAL=true AUTOCONF=true AUTOHEADER=true AUTOMAKE=true
        INSTALL_COMMAND ${BTSTACK_LIBUSB_MAKE} ACLOCAL=true AUTOCONF=true AUTOHEADER=true AUTOMAKE=true install
        BUILD_BYPRODUCTS "${usb_archive}"
    )
    add_library(btstack_libusb STATIC IMPORTED GLOBAL)
    set_target_properties(btstack_libusb PROPERTIES
        IMPORTED_LOCATION "${usb_archive}"
        INTERFACE_INCLUDE_DIRECTORIES "${usb_prefix}/include/libusb-1.0"
        INTERFACE_LINK_LIBRARIES "-framework IOKit;-framework CoreFoundation;-framework Security;-lobjc")
    add_dependencies(btstack_libusb btstack_libusb_build)
    add_dependencies(${target} btstack_libusb_build)
    target_link_libraries(${target} btstack_libusb)
endfunction()
