@@
expression dest, src;
@@
- bt_flip_addr(dest,src)
+ reverse_bd_addr(src, dest)

@@
expression handle;
@@
- hci_remote_eSCO_supported(handle)
+ hci_remote_esco_supported(handle)

@@
expression event, cmd;
@@
- COMMAND_COMPLETE_EVENT(event,cmd) 
+ HCI_EVENT_IS_COMMAND_COMPLETE(event,cmd) 

@@
expression event, cmd;
@@
- COMMAND_STATUS_EVENT(event,cmd) 
+ HCI_EVENT_IS_COMMAND_STATUS(event,cmd) 
