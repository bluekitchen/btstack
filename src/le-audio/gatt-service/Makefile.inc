# Makefile to collect all C source files of src/le-audio/gatt-service

SRC_LE_AUDIO_GATT_SERVICE_FILES = \
    audio_input_control_service_client.h \
    audio_input_control_service_server.c \ 
    audio_input_control_service_util.c \ 
    audio_stream_control_service_client.c \ 
    audio_stream_control_service_server.c \ 
    audio_stream_control_service_util.c \   
    broadcast_audio_scan_service_client.c \ 
    broadcast_audio_scan_service_server.c \ 
    broadcast_audio_scan_service_util.c \   
    coordinated_set_identification_service_client.c \
    coordinated_set_identification_service_server.c \
    gatt_service_client_helper.c \    
    media_control_service_client.c \
    media_control_service_server.c \
    media_control_service_util.c \
    microphone_control_service_client.c \
    microphone_control_service_server.c \
    object_transfer_service_client.c \
    object_transfer_service_server.c \
    published_audio_capabilities_service_client.c \
    published_audio_capabilities_service_server.c \
    volume_control_service_server.c \
    volume_offset_control_service_server.c \