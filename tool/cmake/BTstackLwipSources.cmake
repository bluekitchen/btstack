# Hand-maintained lwIP source groups shared by CMake ports.
#
# lwIP itself is third-party code. The lwip platform group contains BTstack's
# integration files required by the selected lwIP configuration.

set(btstack_lwip_source_groups
    lwip-core
    lwip-ipv4
    lwip-netif
    lwip-httpd
    lwip-dhcp-server
    lwip-platform
    lwip
)

set(btstack_lwip_sources_lwip_core
    3rd-party/lwip/core/src/core/def.c
    3rd-party/lwip/core/src/core/inet_chksum.c
    3rd-party/lwip/core/src/core/init.c
    3rd-party/lwip/core/src/core/ip.c
    3rd-party/lwip/core/src/core/mem.c
    3rd-party/lwip/core/src/core/memp.c
    3rd-party/lwip/core/src/core/netif.c
    3rd-party/lwip/core/src/core/pbuf.c
    3rd-party/lwip/core/src/core/tcp.c
    3rd-party/lwip/core/src/core/tcp_in.c
    3rd-party/lwip/core/src/core/tcp_out.c
    3rd-party/lwip/core/src/core/timeouts.c
    3rd-party/lwip/core/src/core/udp.c
)

set(btstack_lwip_sources_lwip_ipv4
    3rd-party/lwip/core/src/core/ipv4/acd.c
    3rd-party/lwip/core/src/core/ipv4/dhcp.c
    3rd-party/lwip/core/src/core/ipv4/etharp.c
    3rd-party/lwip/core/src/core/ipv4/icmp.c
    3rd-party/lwip/core/src/core/ipv4/ip4.c
    3rd-party/lwip/core/src/core/ipv4/ip4_addr.c
    3rd-party/lwip/core/src/core/ipv4/ip4_frag.c
)

set(btstack_lwip_sources_lwip_netif
    3rd-party/lwip/core/src/netif/ethernet.c
)

set(btstack_lwip_sources_lwip_httpd
    3rd-party/lwip/core/src/apps/http/altcp_proxyconnect.c
    3rd-party/lwip/core/src/apps/http/fs.c
    3rd-party/lwip/core/src/apps/http/httpd.c
)

set(btstack_lwip_sources_lwip_dhcp_server
    3rd-party/lwip/dhcp-server/dhserver.c
)

set(btstack_lwip_sources_lwip_platform
    platform/lwip/port/sys_arch.c
    platform/lwip/bnep_lwip.c
)

set(btstack_lwip_sources_lwip
    ${btstack_lwip_sources_lwip_core}
    ${btstack_lwip_sources_lwip_ipv4}
    ${btstack_lwip_sources_lwip_netif}
    ${btstack_lwip_sources_lwip_httpd}
    ${btstack_lwip_sources_lwip_dhcp_server}
    ${btstack_lwip_sources_lwip_platform}
)
