# Register the bundled NO_SYS lwIP stack and the services used by the PAN
# HTTP example. Include after BTstack.cmake and btstack_declare_port().
btstack_declare_port_capability(CAPABILITY lwip
    GROUPS third-party-lwip lwip-platform
    PUBLIC_INCLUDE_DIRECTORIES
        "${BTSTACK_ROOT}/3rd-party/lwip/core/src/include"
        "${BTSTACK_ROOT}/platform/lwip/port"
)
btstack_declare_port_capability(CAPABILITY lwip-httpd-service
    GROUPS lwip-httpd
)
btstack_declare_port_capability(CAPABILITY lwip-dhcp-service
    GROUPS lwip-dhcp-server
    PUBLIC_INCLUDE_DIRECTORIES "${BTSTACK_ROOT}/3rd-party/lwip/dhcp-server"
)
