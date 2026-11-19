# CMake Integration

BTstack's CMake helpers let you add the BTstack code required by a particular Bluetooth application without maintaining a hand-written list of source files and include directories. They work in two situations: integrating BTstack into an existing CMake project, and creating a BTstack port that builds one or more examples for a board.

The helpers do not define a top-level project or select a toolchain, a SDK, a board, or a Bluetooth controller. Thse remain the responsibility of your project or port. This chapter first explains the building blocks used by the helpers, then walks through each workflow.

## Core concepts

### Source groups

A source group is a named collection of related BTstack source files. Instead of adding individual files, this allows to add the groups needed by your application. Most group names correspond to a Bluetooth protocol or profile, such as `gatt-server` or `classic-rfcomm`; others select infrastructure such as `transport-h4`, `platform-posix-run-loop`, a chipset support, or third-party C code. Group names use lowercase hyphenated words. The complete catalog is in `tool/cmake/BTstackSources.cmake`.

The catalog stores source lists separately from mappings for private and public include directories. CMake uses the private mapping for each selected group to find the headers needed to compile its sources. It applies BTstack's common public include paths independently of the selected groups, and adds group-specific public paths when an application needs to use that group's API. BlueKitchen maintains this generated catalog with `tool/create_cmake_sources.py`.

<-- already mentioned, should this go somewhere? A group is a reusable BTstack component, not a complete board configuration. Your project still chooses a run loop, HCI transport, storage backend, chipset support, SDK integration, and board-specific files. --/>

#### Finding groups

To choose groups for an application or port, you can inspect the catalog. From the BTstack repository root, list all groups or search by name:

```sh
python3 tool/btstack_groups.py list
python3 tool/btstack_groups.py list --filter run-loop
```

For all commands and options, run:

```sh
python3 tool/btstack_groups.py --help
```

The Python wrapper requires CMake on `PATH`, but can be run from any directory.

### Resolving dependencies

Requested groups are only the starting point. `tool/cmake/BTstack.cmake` resolves the dependencies that each group requires. For example, requesting `gatt-server` also selects the ATT database, ATT dispatch, and LE support; a Classic profile can select RFCOMM, SDP, and lower protocol layers.

To see what selecting a group implies, inspect it directly:

```sh
python3 tool/btstack_groups.py info gatt-server
```

The output shows its direct dependencies, the fully resolved group set, required configuration macros, source files, and public include directories required by applications. Source paths are relative to the BTstack root; public include paths are absolute. Some direct dependencies are internal configuration markers, so they are not selectable groups and do not appear in the resolved source closure.

`btstack_resolve_groups` returns this complete group set and identifies the `ENABLE_*` configuration macros it requires, such as `ENABLE_BLE` or `ENABLE_CLASSIC`.

### Port baseline

A port defines a baseline: the groups and board-local files required by every application it builds. Typical baseline groups provide the run loop, HCI transport, persistent storage, chipset support, and common cryptography. Board-local files — such as `main.c`, UART adapter, controller firmware, and SDK sources — remain explicit because they belong to the board or project rather than the BTstack catalog.

#### Typical port structure

A port's `CMakeLists.txt` mainly follows this order:

1. Set up the project, toolchain, and board SDK, then include `tool/cmake/BTstack.cmake`.
2. Declare the port with `btstack_declare_port`.
3. Define its baseline groups and board-local sources.
4. Register the effective `btstack_config.h` and optional capabilities.
5. Include application declarations, filter them for the port, and create the selected application libraries and executables.
6. Optionally add diagnostic targets for inspecting the resolved port.

The SDK setup, board sources, and final executable policy remain port-specific. This section introduces the baseline and configuration in steps 2–4. The [Port authoring](#port-authoring) section covers application declarations and building in step 5, while [Inspect a configured port](#inspect-a-configured-port) covers the optional diagnostics in step 6.

#### Configuration and port capabilities

`btstack_config.h` is the configuration header compiled with BTstack. It is the source of truth for platform properties (`HAVE_*`), enabled protocols and features (`ENABLE_*`), and options such as static-memory and persistent-storage configuration. CMake does not generate or modify this header; the project or port owns it. A port does not need to repeat its configuration macros manually in CMake: `btstack_configure_port` preprocesses the header and registers its effective macro set.

To keep CMake selection aligned with compilation, source groups must match the effective `btstack_config.h`. Call `btstack_configure_port` after declaring the port so CMake can reject or omit groups and applications whose required features are disabled.

A minimal port setup looks like this:

```cmake
btstack_declare_port(my-port)
btstack_configure_port(
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h"
    DEFINITIONS ${MY_PORT_COMPILE_DEFINITIONS})
```

Pass to `DEFINITIONS` the same preprocessor definitions used when compiling the port, especially when `btstack_config.h` enables features conditionally. `btstack_declare_port` makes that port current for helpers called in the same directory and its children. If you declare more than one port, the last declared port is current; use `PORT <name>` to select another one explicitly. Helpers report an error if no current port exists.

A capability is not a BTstack feature. It represents an optional service provided by a port, such as `audio` or `led`, and can include groups, sources, and public headers declared through `btstack_declare_port_capability`.

For example, a POSIX port should declare `audio` only when CMake finds the PortAudio library. Applications that require audio are then excluded from ports that do not provide it.

## Adding BTstack to an existing CMake project

Choose this workflow when your project already has an executable target, board support, and toolchain configuration. You add BTstack as a library and link it to that target.

For example, a project building an LE GATT server using a Bluetooth Controller over HCI H4/UART running on a POSIX-system can start with:

```cmake
# Point to the BTstack checkout.
set(BTSTACK_ROOT "${CMAKE_CURRENT_LIST_DIR}/third_party/btstack")
include("${BTSTACK_ROOT}/tool/cmake/BTstack.cmake")

# Select the BTstack groups used by this project.
btstack_add_library(my_btstack
    CONFIG "${CMAKE_CURRENT_LIST_DIR}/config/btstack_config.h"
    GROUPS gatt-server transport-h4 platform-posix-run-loop)

# Link BTstack to the application target.
target_link_libraries(my_application PRIVATE my_btstack)
```

The example is intentionally incomplete: `gatt-server`, H4 transport, and the POSIX run loop are selected, but you must also select the storage, chipset, and other groups required by your board and controller. You must supply a matching `btstack_config.h`. For a complete POSIX H4 example, see `port/posix-h4/gatt-counter-standalone` and its README.

`btstack_add_library` resolves dependent groups, creates a static library, and propagates BTstack's public and group-specific include directories—including the directory containing the configuration header—to targets that link it. Keep your board-local and SDK sources in your own targets.

At configure time, the helper checks that `btstack_config.h` defines every `ENABLE_*` macro required by the selected groups. If evaluating that header requires extra definitions or include paths, pass `CONFIG_DEFINITIONS` and `CONFIG_INCLUDE_DIRECTORIES`; they are used for both validation and compilation. The configuration check uses the C compiler during configuration, so target-specific compiler flags and generator expressions are unavailable. Use the lower-level helpers when your SDK needs those settings to preprocess the header.

Use `library-info` to verify the resolved library before compiling the application:

```sh
cmake --build build --target library-info
```

It prints requested and resolved groups, required and detected configuration macros, sources, and include paths. If several BTstack libraries exist, select one with `LIBRARY=my_btstack cmake --build build --target library-info`; with multi-configuration generators, also pass `--config`. The report reflects configuration-time target changes, but excludes application sources and transitive usage requirements from linked targets.

If `btstack_add_library` does not fit your target layout, compose your own target with `btstack_resolve_groups` and the `btstack_collect_*` helpers. Optionally apply BTstack's warning policy with `btstack_target_warnings`. The convenience helper deliberately does not alter project-wide warnings or suppress warnings from third-party code.

## Port authoring

Use port authoring when one board configuration should build multiple BTstack examples or applications. A port combines a common board baseline with each application's declared requirements, then filters out applications unsupported by its configuration or capabilities. If your project builds only one application, use `btstack_add_library` instead.

### Applications and requirements

A port can declare the applications it supports before creating targets for them. BTstack's examples are declared in `example/requirements.cmake`, but an external project can use the same declaration format. A declaration describes an application's sources, optional GATT database, and requirements; it does not create a target or enable features:

```cmake
btstack_declare_application(gatt_counter
    SOURCES gatt_counter.c
    GATT_DB gatt_counter.gatt
    NEEDS gatt-server ENABLE_LE_PERIPHERAL)
```

Each `NEEDS` entry is one of three things: a source group, a macro expected in `btstack_config.h`, or a capability supplied by the port. Groups express the BTstack implementation needed by the application; macros and capabilities let CMake decide whether the configured port can build it.

Relative paths in `SOURCES` and `GATT_DB` are interpreted relative to the declaration file; absolute paths are unchanged. Query the resolved values with `btstack_get_application_property(output application SOURCES)` or `btstack_get_application_property(output application GATT_DB)`.

Requirements describe and validate compatibility. They never turn on a BTstack feature for a target; that remains the responsibility of `btstack_config.h` and the project's build settings.

### Build applications declared by a port

For each supported application, ports normally create a small BTstack static library. `btstack_add_application_library` combines the application's requirements with the port baseline and compiles the resulting closure. Shared port components are reused; consequently, all selected applications must use compatible compilation settings.

The following assumes that the port has already declared itself, registered its effective configuration, and declared any optional capabilities. The port still owns the final executable and its board policy:

```cmake
include("${BTSTACK_ROOT}/example/requirements.cmake")
set(EXAMPLES ${btstack_declared_applications})
btstack_filter_applications_for_port(EXAMPLES
    CANDIDATES ${EXAMPLES})

foreach(EXAMPLE IN LISTS EXAMPLES)
    # Create the BTstack library for this application's resolved requirements.
    btstack_add_application_library(
        TARGET btstack_${EXAMPLE}_library
        APPLICATION ${EXAMPLE}
        BASE_GROUPS ${MY_PORT_BASE_GROUPS}
        PORT_SOURCES ${MY_PORT_SOURCES}
        PRIVATE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

    # Create and configure the final board executable.
    btstack_add_application_executable(${EXAMPLE} APPLICATION ${EXAMPLE})
    btstack_target_warnings(${EXAMPLE} STRICT)
    target_link_libraries(${EXAMPLE} PRIVATE btstack_${EXAMPLE}_library)
endforeach()
```

You need to call `btstack_declare_port(my-port)`, `btstack_configure_port(...)`, and any capability declarations before `btstack_filter_applications_for_port`. The filter quietly excludes applications the port intentionally cannot support. Use `application-info` later when you need to see why a particular application was excluded.

For an application with a declared `GATT_DB`, `btstack_add_application_executable` automatically invokes `btstack_target_gatt`. That helper runs the GATT compiler, adds the generated header's directory to the include path, and tracks imported `.gatt` files through a depfile. You can call it directly for an existing target. The application library provides BTstack's standard public includes; the port remains responsible for configuration and SDK include paths.

## Inspect a configured port

After the normal application loop, add BTstack's diagnostic targets to the port:

```cmake
btstack_add_info_targets()
```

Configure the port as usual, then use these targets to inspect the applications and source groups CMake selected:

```sh
cmake --build build --target port-info
cmake --build build --target list-applications
APPLICATION=gatt_counter cmake --build build --target application-info
cmake --build build --target list-examples
EXAMPLE=gatt_counter cmake --build build --target example-info
cmake --build build --target group-graph
```

`port-info` shows the effective BTstack configuration macros and declared capabilities. `list-applications` shows candidates for the current port, and `application-info` explains whether a selected application is included. For excluded applications, it reports missing macros or capabilities; for included applications, it prints the complete source and include-directory lists.

`list-examples` and `example-info` remain available for BTstack's built-in example workflow; `list-examples` groups examples by radio requirements. `group-graph` writes `btstack-groups.dot` to the build directory and, when Graphviz is installed, also writes an SVG.

## Reusing resolved example data in another build system

Even if your production build does not use CMake — for example, IAR Embedded Workbench or Keil uVision — you can configure a representative BTstack port on a system with CMake and then use `example-info` to get the authoritative resolved input list. First configure the reference port with CMake, then request the example to reproduce:

```sh
EXAMPLE=gatt_counter cmake --build build --target example-info
```

This output is a resolved snapshot of one port configuration, not a project generator. It provides the sources, include paths, generated artifacts, and definitions required by another build system; that environment remains responsible for compiler flags, linker scripts, startup code, SDK integration, and debugger settings.

Use the reported data when creating the project in the other build system:

1. Add every reported source file, including application files, port-local files, and generated firmware source when present.
2. Add every reported include directory. Keep the port configuration directory containing `btstack_config.h` ahead of generic include paths.
3. Use the same preprocessor definitions and board SDK libraries as the reference CMake port.
4. Reproduce generated artifacts: run `compile_gatt.py` for a declared GATT database and perform any controller firmware conversion or download step required by the selected port.


