if HAVE_LIBUSB
    usb_support = hci_transport_usb.c 
endif

if USE_SPRINGBOARD
    springboard_support = ../SpringBoardAccess/SpringBoardAccess.c
endif


bin_PROGRAMS = BTdaemon
BTdaemon_SOURCES =          \
    bt_control_iphone.c     \
    daemon.c                \
    hci.c                   \
    hci_cmds.c              \
    hci_dump.c              \
    hci_transport_h4.c      \
    $(usb_support)          \
    l2cap.c                 \
    l2cap_signaling.c       \
    platform_iphone.c       \
    linked_list.c           \
    run_loop.c              \
    socket_connection.c     \
    $(springboard_support)  \
    utils.c                 
                            
include_HEADERS = \         
    bt_control.h            \
    bt_control_iphone.h     \
    hci.h                   \
    hci_cmds.h              \
    hci_dump.h              \
    hci_transport.h         \
    hci_transport_h4.h      \
    hci_transport_usb.h     \
    linked_list.h           \
    l2cap.h                 \
    l2cap_signaling.h       \
    platform_iphone.h       \
    run_loop.h              \
    socket_connection.h     \
    utils.h                 
