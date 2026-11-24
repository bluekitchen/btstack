# Compile-check target

Ports using `btstack_configure_port()` provide an on-demand `btstack` target:

```sh
cmake -S port/posix-h4 -B build-posix-h4
cmake --build build-posix-h4 --target btstack
```

It compiles the catalogued C sources under `src/` with the port's configuration
and warnings enabled, without linking them or adding them to the default build.
Classic and BLE source directories are omitted when the corresponding feature
is disabled. Preprocessor-disabled code is not checked; use multiple port
configurations to cover it. Platform, chipset and third-party catalog sources
need platform-specific dependencies and are checked by the port targets that
select them. Ports that do not use `btstack_configure_port()` do not get this
target automatically. An existing target named `btstack` is left untouched.

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

## SEGGER RTT

Embedded ports select the RTT implementation and its matching headers directly.
`third-party-segger-rtt-core`, `third-party-segger-rtt-printf`, and
`third-party-segger-rtt-syscalls` contain the bundled core, formatted output,
and GCC syscall retargeting respectively. Select only the pieces the port needs:
the embedded RTT stdin adapter needs the core, while the RTT HCI dump also
needs RTT printf. Syscall retargeting is optional; do not select it when the
port supplies its own `_write` implementation. Add the chosen RTT header and
configuration directories to the port's include directories for all relevant
source groups.

An SDK may supply the core instead of `third-party-segger-rtt-core`. For
example, Pico SDK's `pico_stdio_rtt` supplies the core but not RTT printf; a
port may select `third-party-segger-rtt-printf` with compatible SDK headers.
Pico SDK interface sources must be linked only once, and Pico stdio and
BTstack RTT stdin must not both consume RTT channel 0. The port must keep its
source selections consistent with `ENABLE_SEGGER_RTT`.

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
