if HAVE_LIBUSB
    usb_support = hci_transport_usb.c 
endif


bin_PROGRAMS = BTdaemon
BTdaemon_SOURCES =       \
    bt_control_iphone.c  \
    daemon.c             \
    hci.c                \
    hci_dump.c           \
    hci_transport_h4.c   \
    $(usb_support)       \
    l2cap.c              \
    main.c               \
    run_loop.c           \
    socket_server.c
    
include_HEADERS = \
    bt_control.h                      \
    bt_control_iphone.h               \
    daemon.h                          \
    hci.h                             \
    hci_dump.h                        \
    hci_transport.h                   \
    hci_transport_h4.h                \
    hci_transport_usb.h               \
    l2cap.h                           \
    run_loop.h                        \
    socket_server.h                     
