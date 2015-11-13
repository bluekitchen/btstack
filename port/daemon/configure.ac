#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.60)
AC_INIT([BTstack], 0.1)
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE

AC_ARG_WITH(hci-transport, [AS_HELP_STRING([--with-hci-transport=transportType], [Specify BT type to use: h4, usb])], HCI_TRANSPORT=$withval, HCI_TRANSPORT="h4")  
AC_ARG_WITH(uart-device, [AS_HELP_STRING([--with-uart-device=uartDevice], [Specify BT UART device to use])], UART_DEVICE=$withval, UART_DEVICE="DEFAULT")  
AC_ARG_WITH(uart-speed, [AS_HELP_STRING([--with-uart-speed=uartSpeed], [Specify BT UART speed to use])], UART_SPEED=$withval, UART_SPEED="115200")
AC_ARG_ENABLE(powermanagement, [AS_HELP_STRING([--disable-powermanagement],[Disable powermanagement])], USE_POWERMANAGEMENT=$enableval, USE_POWERMANAGEMENT="yes")
AC_ARG_ENABLE(launchd, [AS_HELP_STRING([--enable-launchd],[Compiles BTdaemon for use by launchd])], USE_LAUNCHD=$enableval, USE_LAUNCHD="no")
AC_ARG_WITH(vendor-id, [AS_HELP_STRING([--with-vendor-id=vendorID], [Specify USB BT Dongle vendorID])], USB_VENDOR_ID=$withval, USB_VENDOR_ID="0")  
AC_ARG_WITH(product-id, [AS_HELP_STRING([--with-product-id=productID], [Specify USB BT Dongle productID])], USB_PRODUCT_ID=$withval, USB_PRODUCT_ID="0")  
 
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
    echo "Cross-compiling for iPhone/iPod touch uses theos"
    ./config-iphone.sh
    exit 0
fi

# use capitals for transport type
if test "x$HCI_TRANSPORT" = xusb; then
    HCI_TRANSPORT="USB"
fi
if test "x$HCI_TRANSPORT" = xh4; then
    HCI_TRANSPORT="H4"
fi

# validate USB support
if test "x$HCI_TRANSPORT" = xUSB; then
    # pkg-config needed
    PKG_PROG_PKG_CONFIG
    # libusb installed?
    PKG_CHECK_MODULES([LIBUSB], [libusb-1.0], HAVE_LIBUSB="yes", HAVE_LIBUSB="no")
    if test "$HAVE_LIBUSB" == "no" ; then 
        AC_MSG_ERROR(USB Transport requested but libusb-1.0 not found using pkg-config. Please set PKG_CONFIG_PATH correctly and/or install libusb-1.0 from your distribution or from http://libusb.sourceforge.net/api-1.0/)
    fi
    LIBUSB_LDFLAGS=$LIBUSB_LIBS
fi
AM_CONDITIONAL(HAVE_LIBUSB, [test "x$HAVE_LIBUSB" == "xyes"])

echo
echo "BTstack configured for HCI $HCI_TRANSPORT Transport"

HAVE_SO_NOSIGPIPE="no"

RUN_LOOP_SOURCES="run_loop_posix.c"
case "$host_os" in
    darwin*)
        RUN_LOOP_SOURCES="$RUN_LOOP_SOURCES run_loop_cocoa.m"
        LDFLAGS="$LDFLAGS -framework CoreFoundation -framework Foundation"
        USE_COCOA_RUN_LOOP="yes"
        BTSTACK_LIB_LDFLAGS="-dynamiclib -install_name \$(prefix)/lib/libBTstack.dylib"
        BTSTACK_LIB_EXTENSION="dylib"
        REMOTE_DEVICE_DB_SOURCES="remote_device_db_cocoa.m"
        REMOTE_DEVICE_DB="remote_device_db_iphone"
        HAVE_SO_NOSIGPIPE="yes";
        ;;
    mingw*)
        echo "Building on mingw32"
        USE_COCOA_RUN_LOOP="no"
        LDFLAGS="$LDFLAGS -lws2_32"
        BTSTACK_LIB_LDFLAGS="-shared"
        BTSTACK_LIB_EXTENSION="dll"
        REMOTE_DEVICE_DB_SOURCES="remote_device_db_memory.c"
        REMOTE_DEVICE_DB="remote_device_db_memory"
        ;;
    *)
        USE_COCOA_RUN_LOOP="no"
        BTSTACK_LIB_LDFLAGS="-shared -Wl,-rpath,\$(prefix)/lib"
        BTSTACK_LIB_EXTENSION="so"
        REMOTE_DEVICE_DB_SOURCES="remote_device_db_memory.c"
        REMOTE_DEVICE_DB="remote_device_db_memory"
    ;;
esac

# treat warnings seriously
CFLAGS="$CFLAGS -Werror -Wall -Wpointer-arith"
    
# 64-bit compilation requires position independent code (PIC) for libraries
BTSTACK_LIB_LDFLAGS+=" -fpic"

AM_CONDITIONAL(USE_POWERMANAGEMENT, [test "x$USE_POWERMANAGEMENT" == "xyes"])

# summary

echo "CC:                  $CC"
echo "CFLAGS:              $CFLAGS"
echo "LDFLAGS:             $LDFLAGS"
echo "BTSTACK_LIB_LDFLAGS: $BTSTACK_LIB_LDFLAGS"

if test "x$HCI_TRANSPORT" = xUSB; then
    echo "USB_PRODUCT_ID:      $USB_PRODUCT_ID"
    echo "USB_VENDOR_ID:       $USB_VENDOR_ID"
    echo "LIBUSB_CFLAGS:       $LIBUSB_CFLAGS"
    echo "LIBUSB_LDFLAGS:      $LIBUSB_LDFLAGS"
else
    echo "UART_DEVICE:         $UART_DEVICE"
    echo "UART_SPEED:          $UART_SPEED"
fi

echo "USE_POWERMANAGEMENT: $USE_POWERMANAGEMENT"
echo "USE_COCOA_RUN_LOOP:  $USE_COCOA_RUN_LOOP"
echo "REMOTE_DEVICE_DB:    $REMOTE_DEVICE_DB"
echo "HAVE_SO_NOSIGPIPE:   $HAVE_SO_NOSIGPIPE"
echo
echo

# create btstack-config.h
rm -f btstack-config.h
echo "//"                                       >> btstack-config.h
echo "// btstack-config.h"                      >> btstack-config.h
echo "// created by configure for BTstack "     >> btstack-config.h
echo "//" `date`                                >> btstack-config.h
echo "//"                                       >> btstack-config.h
echo                                            >> btstack-config.h
echo "#ifndef __BTSTACK_CONFIG"                 >> btstack-config.h
echo "#define __BTSTACK_CONFIG"                 >> btstack-config.h
echo                                            >> btstack-config.h
if test "x$HCI_TRANSPORT" = xUSB; then
    USB_SOURCES=hci_transport_h2_libusb.c
    echo "#define HAVE_TRANSPORT_USB" >> btstack-config.h
    echo "#define USB_PRODUCT_ID $USB_PRODUCT_ID" >> btstack-config.h
    echo "#define USB_VENDOR_ID $USB_VENDOR_ID" >> btstack-config.h
else
    echo "#define HAVE_TRANSPORT_H4" >> btstack-config.h
    echo "#define UART_DEVICE \"$UART_DEVICE\"" >> btstack-config.h
    echo "#define UART_SPEED $UART_SPEED" >> btstack-config.h
    if test "x$USE_BLUETOOL" = xyes; then
        echo "#define USE_BLUETOOL" >> btstack-config.h
    fi
    if test "x$USE_POWERMANAGEMENT" = xyes; then
        echo "#define USE_POWERMANAGEMENT" >> btstack-config.h
    fi
fi
if test "x$USE_COCOA_RUN_LOOP" = xyes; then
    echo "#define USE_COCOA_RUN_LOOP" >> btstack-config.h
fi
echo "#define USE_POSIX_RUN_LOOP" >> btstack-config.h
echo "#define HAVE_SDP" >> btstack-config.h
echo "#define HAVE_RFCOMM" >> btstack-config.h
if test ! -z "$REMOTE_DEVICE_DB" ; then 
    echo "#define REMOTE_DEVICE_DB $REMOTE_DEVICE_DB" >> btstack-config.h
fi
if test "x$HAVE_SO_NOSIGPIPE" == xyes ; then
    echo "#define HAVE_SO_NOSIGPIPE" >> btstack-config.h
fi

# often not present for embedded
echo "#define HAVE_TIME" >> btstack-config.h
echo "#define HAVE_MALLOC" >> btstack-config.h
echo "#define HAVE_BZERO" >> btstack-config.h
echo "#define HAVE_BLE" >> btstack-config.h
echo "#define ENABLE_LOG_INFO " >> btstack-config.h
echo "#define ENABLE_LOG_ERROR" >> btstack-config.h
echo "#define HCI_ACL_PAYLOAD_SIZE 1021" >> btstack-config.h
echo "#define SDP_DES_DUMP" >> btstack-config.h
echo >> btstack-config.h
echo "#endif" >> btstack-config.h

AC_SUBST(HAVE_LIBUSB)
AC_SUBST(REMOTE_DEVICE_DB_SOURCES)
AC_SUBST(USB_SOURCES)
AC_SUBST(RUN_LOOP_SOURCES)
AC_SUBST(CFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(BTSTACK_LIB_LDFLAGS)
AC_SUBST(BTSTACK_LIB_EXTENSION)
AC_SUBST(LIBUSB_CFLAGS)
AC_SUBST(LIBUSB_LDFLAGS)
AC_OUTPUT(Makefile src/Makefile example/Makefile)
