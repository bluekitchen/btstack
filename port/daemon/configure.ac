#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.60)
AC_INIT([BTstack], 0.1)
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE

AC_ARG_WITH(hci-transport, [AS_HELP_STRING([--with-hci-transport=transportType], [Specify BT type to use: h4, usb])],      HCI_TRANSPORT=$withval,      HCI_TRANSPORT="h4")  
AC_ARG_WITH(uart-device,   [AS_HELP_STRING([--with-uart-device=uartDevice],      [Specify BT UART device to use])],        UART_DEVICE=$withval,        UART_DEVICE="DEFAULT")  
AC_ARG_WITH(uart-speed,    [AS_HELP_STRING([--with-uart-speed=uartSpeed],        [Specify BT UART speed to use])],         UART_SPEED=$withval,         UART_SPEED="115200")
AC_ARG_WITH(vendor-id,     [AS_HELP_STRING([--with-vendor-id=vendorID],          [Specify USB BT Dongle vendorID])],       USB_VENDOR_ID=$withval,      USB_VENDOR_ID="0")  
AC_ARG_WITH(product-id,    [AS_HELP_STRING([--with-product-id=productID],        [Specify USB BT Dongle productID])],      USB_PRODUCT_ID=$withval,     USB_PRODUCT_ID="0")  
AC_ARG_ENABLE(launchd,     [AS_HELP_STRING([--enable-launchd],                   [Compiles BTdaemon for use by launchd])], USE_LAUNCHD=$enableval,      USE_LAUNCHD="no")
AC_ARG_ENABLE(intel-usb,   [AS_HELP_STRING([--enable-intel-usb],                 [Enable Intel firmware support ])],       ENABLE_INTEL_USB=$enableval, ENABLE_INTEL_USB="no") 

# BUILD/HOST/TARGET
AC_CANONICAL_HOST

# Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AC_PROG_OBJC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# iPhone/iPod touch cross-compilation uses theos
SDK_PATH="$DEVELOPER_PATH/SDKs/iPhoneOS$SDK_VERSION.sdk"
if test "x$target" = xiphone; then
    echo "Cross-compiling for iPhone/iPod touch uses theos, please checkout port/ios"
    exit 0
fi

case "$host_os" in
    darwin*)
        btstack_run_loop_SOURCES="btstack_run_loop_posix.o btstack_run_loop_corefoundation.m"
        LDFLAGS+="-framework CoreFoundation -framework Foundation"
        BTSTACK_LIB_LDFLAGS="-dynamiclib -install_name \$(prefix)/lib/libBTstack.dylib"
        BTSTACK_LIB_EXTENSION="dylib"
        REMOTE_DEVICE_DB_SOURCES="btstack_device_name_db_corefoundation.m rfcomm_service_db_corefoundation.m"
        BTSTACK_DEVICE_NAME_DB_INSTANCE="btstack_device_name_db_corefoundation_instance"
        UNIX_SOCKETS=yes
        HCI_USB_LIB=libusb
        UART_DRIVER=posix
        ;;
    mingw*)
        echo "Building on mingw32"
        btstack_run_loop_SOURCES="btstack_run_loop_windows.o"
        LDFLAGS+="-lws2_32 -static-libgcc"
        BTSTACK_LIB_LDFLAGS="-shared"
        BTSTACK_LIB_EXTENSION="dll"
        REMOTE_DEVICE_DB_SOURCES="rfcomm_service_db_memory.o"
        # BTSTACK_DEVICE_NAME_DB_INSTANCE="btstack_device_name_db_memory_instance"
        UNIX_SOCKETS=no
        HCI_USB_LIB=winusb
        UART_DRIVER=block_windows
        ;;
    *)
        btstack_run_loop_SOURCES="btstack_run_loop_posix.o"
        BTSTACK_LIB_LDFLAGS="-shared -Wl,-rpath,\$(prefix)/lib"
        BTSTACK_LIB_EXTENSION="so"
        REMOTE_DEVICE_DB_SOURCES="rfcomm_service_db_memory.o"
        # BTSTACK_DEVICE_NAME_DB_INSTANCE="btstack_device_name_db_fs_instance"
        UNIX_SOCKETS=yes
        HCI_USB_LIB=libusb
        UART_DRIVER=posix
    ;;
esac


# use capitals for transport type
if test "x$HCI_TRANSPORT" = xusb; then
    HCI_TRANSPORT="USB"
fi
if test "x$HCI_TRANSPORT" = xh4; then
    HCI_TRANSPORT="H4"
fi

# set linker flags for winusb if selected
if test "x$HCI_TRANSPORT" = xUSB && test "x$HCI_USB_LIB" == xwinusb; then
    USB_LDFLAGS="-lsetupapi -lwinusb"
fi

# validate USB support on non-windows
if test "x$HCI_TRANSPORT" = xUSB && test "x$HCI_USB_LIB" == xlibusb; then
    # pkg-config needed
    PKG_PROG_PKG_CONFIG
    # libusb installed?
    PKG_CHECK_MODULES([LIBUSB], [libusb-1.0], HAVE_LIBUSB="yes", HAVE_LIBUSB="no")
    if test "$HAVE_LIBUSB" == "no" ; then
        AC_MSG_ERROR(USB Transport requested but libusb-1.0 not found using pkg-config. Please set PKG_CONFIG_PATH correctly and/or install libusb-1.0 from your distribution or from http://libusb.sourceforge.net/api-1.0/)
    fi
    echo $LIBUSB_CFLAGS
    echo $LIBUSB_LIBS
    USB_CFLAGS=$LIBUSB_CFLAGS
    USB_LDFLAGS=$LIBUSB_LIBS
fi

echo
echo "BTstack configured for HCI $HCI_TRANSPORT Transport"


# treat warnings seriously
CFLAGS="$CFLAGS -Werror -Wall -Wpointer-arith"
    
# 64-bit compilation requires position independent code (PIC) for libraries
CFLAGS+=" -fPIC"

# summary

echo "CC:                      $CC"
echo "CFLAGS:                  $CFLAGS"
echo "LDFLAGS:                 $LDFLAGS"
echo "BTSTACK_LIB_LDFLAGS:     $BTSTACK_LIB_LDFLAGS"

if test "x$HCI_TRANSPORT" = xUSB; then
    echo "USB_PRODUCT_ID:          $USB_PRODUCT_ID"
    echo "USB_VENDOR_ID:           $USB_VENDOR_ID"
    echo "USB_CFLAGS:              $USB_CFLAGS"
    echo "USB_LDFLAGS:             $USB_LDFLAGS"
else
    echo "UART_DEVICE:             $UART_DEVICE"
    echo "UART_SPEED:              $UART_SPEED"
fi

echo "Persistent storage:      $REMOTE_DEVICE_DB_SOURCES"
echo "UNIX_SOCKETS:            $UNIX_SOCKETS"
echo

# create btstack_config.h
rm -f btstack_config.h
echo "//"                                                  >> btstack_config.h
echo "// btstack_config.h"                                 >> btstack_config.h
echo "// created by configure for BTstack"                 >> btstack_config.h
echo "//" `date`                                           >> btstack_config.h
echo "//"                                                  >> btstack_config.h
echo                                                       >> btstack_config.h
echo "#ifndef BTSTACK_CONFIG_H"                            >> btstack_config.h
echo "#define BTSTACK_CONFIG_H"                            >> btstack_config.h
echo                                                       >> btstack_config.h

echo "// Port related features"                            >> btstack_config.h
echo "#define HAVE_POSIX_FILE_IO"                          >> btstack_config.h
echo "#define HAVE_POSIX_TIME"                             >> btstack_config.h
echo "#define HAVE_MALLOC"                                 >> btstack_config.h
if test "x$UNIX_SOCKETS" == xyes; then
    echo "#define HAVE_UNIX_SOCKETS"                       >> btstack_config.h
fi
echo                                                       >> btstack_config.h

echo "// BTstack features that can be enabled"             >> btstack_config.h
echo "#define ENABLE_BLE"                                  >> btstack_config.h
echo "#define ENABLE_CLASSIC"                              >> btstack_config.h
echo "#define ENABLE_GATT_CLIENT_PAIRING"                  >> btstack_config.h
echo "#define ENABLE_LE_CENTRAL"                           >> btstack_config.h
echo "#define ENABLE_LE_PERIPHERAL"                        >> btstack_config.h
echo "#define ENABLE_LE_SECURE_CONNECTIONS"                >> btstack_config.h
echo "#define ENABLE_LOG_ERROR"                            >> btstack_config.h
echo "#define ENABLE_LOG_INFO "                            >> btstack_config.h
echo "#define ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS"  >> btstack_config.h
echo "#define ENABLE_SOFTWARE_AES128"                      >> btstack_config.h
echo "#define ENABLE_PRINTF_HEXDUMP"                       >> btstack_config.h
echo "#define ENABLE_RFCOMM"                               >> btstack_config.h
echo "#define ENABLE_SDP"                                  >> btstack_config.h
echo                                                       >> btstack_config.h

echo "// BTstack configuration. buffers, sizes, .."        >> btstack_config.h
echo "#define HCI_ACL_PAYLOAD_SIZE 1021"                   >> btstack_config.h
echo "#define NVM_NUM_LINK_KEYS         20"                >> btstack_config.h
echo "#define NVM_NUM_DEVICE_DB_ENTRIES 16"                >> btstack_config.h
echo                                                       >> btstack_config.h

echo "// Daemon configuration" >> btstack_config.h
if test "x$HCI_TRANSPORT" = xUSB; then
    USB_SOURCES=hci_transport_h2_$HCI_USB_LIB.c
    echo "#define HAVE_TRANSPORT_USB" >> btstack_config.h
    echo "#define USB_PRODUCT_ID $USB_PRODUCT_ID" >> btstack_config.h
    echo "#define USB_VENDOR_ID $USB_VENDOR_ID" >> btstack_config.h

    if test "x$ENABLE_INTEL_USB" = xyes; then
        echo "#define HAVE_INTEL_USB" >> btstack_config.h
        FIRMWARE_FILES="\${INTEL_FILES}"
        USB_SOURCES+=" btstack_chipset_intel_firmware.c"
    fi
else
    UART_SOURCES=btstack_uart_$UART_DRIVER.o
    echo "#define HAVE_TRANSPORT_H4" >> btstack_config.h
    echo "#define UART_DEVICE \"$UART_DEVICE\"" >> btstack_config.h
    echo "#define UART_SPEED $UART_SPEED" >> btstack_config.h
fi
if test ! -z "$BTSTACK_DEVICE_NAME_DB_INSTANCE" ; then 
    echo "#define BTSTACK_DEVICE_NAME_DB_INSTANCE $BTSTACK_DEVICE_NAME_DB_INSTANCE" >> btstack_config.h
fi

echo                        >> btstack_config.h
echo "#endif"               >> btstack_config.h

AC_SUBST(FIRMWARE_FILES)
AC_SUBST(REMOTE_DEVICE_DB_SOURCES)
AC_SUBST(USB_SOURCES)
AC_SUBST(UART_SOURCES)
AC_SUBST(btstack_run_loop_SOURCES)
AC_SUBST(CFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(BTSTACK_LIB_LDFLAGS)
AC_SUBST(BTSTACK_LIB_EXTENSION)
AC_SUBST(USB_CFLAGS)
AC_SUBST(USB_LDFLAGS)
AC_OUTPUT(Makefile src/Makefile example/Makefile)
