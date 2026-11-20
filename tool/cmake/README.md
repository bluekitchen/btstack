# Resolver dependency graph

Generate the complete source-group dependency graph without configuring a port:

```sh
cmake -DOUTPUT_FILE=btstack-groups.dot -P tool/cmake/BTstackGroupGraph.cmake
dot -Tsvg btstack-groups.dot -o btstack-groups.svg
```

The first command needs only CMake. The second needs Graphviz. Alternatively,
pass `-DSVG_FILE=btstack-groups.svg` to the first command to render immediately.

Ports that register the introspection targets also provide:

```sh
cmake --build <build-dir> --target group-graph
```

This writes `btstack-groups.dot` in the port's build directory, and also writes
`btstack-groups.svg` when `dot` was found at configure time.

The graph includes every catalog group, including groups with no dependencies,
and the resolver's configuration marker groups. Arrows point from a group to its
direct requirements; transitive edges are not added. Dashed boxes are marker
groups, and dashed arrows to ellipses show `ENABLE_*` configuration requirements.

The exporter reads `btstack_group_dependencies_*` and
`btstack_group_configuration_requirements_*` directly from `BTstack.cmake`.
It shows the complete port-independent internal group graph, including legacy
aggregate groups. It does not infer dependencies from source includes, expand
aggregate source lists into new edges, or select port capability providers.

## lwIP providers

`bnep-lwip` depends on BTstack's `bnep` group and the port's `lwip`
capability. After declaring the port, a port using the bundled NO_SYS
integration and its HTTP and DHCP services can register all three with:

```cmake
include(${BTSTACK_ROOT}/platform/lwip/btstack_lwip.cmake)
```

Alternatively, a port can register just the lwIP stack:

```cmake
btstack_declare_port_capability(CAPABILITY lwip
    GROUPS third-party-lwip lwip-platform
    PUBLIC_INCLUDE_DIRECTORIES
        "${BTSTACK_ROOT}/3rd-party/lwip/core/src/include"
        "${BTSTACK_ROOT}/platform/lwip/port")
```

`third-party-lwip` contains the bundled lwIP sources. `lwip-platform` adds
BTstack's NO_SYS `sys_now()` implementation and configuration. A port can use
the bundled sources with a different system integration by choosing its own
configuration and system implementation instead of `lwip-platform`.

A port using an SDK can instead register its lwIP library with
`LINK_LIBRARIES` and expose its lwIP headers and configuration directories
with `PUBLIC_INCLUDE_DIRECTORIES`. These directories must be provided explicitly
for the separately compiled BTstack source groups. The provider must integrate
lwIP timers and execution with the port's run loop and configure the APIs used
by the BNEP adapter. HTTP and DHCP servers are separate from this capability.
Applications requesting `bnep-lwip` are filtered out when the port does not
provide `lwip`. `pan_lwip_http_server` additionally requires the
`lwip-httpd-service` and `lwip-dhcp-service` capabilities. Their providers must
expose compatible lwIP HTTP server APIs and the `dhserver.h` DHCP server API,
respectively, and use the same lwIP configuration as the selected provider.
