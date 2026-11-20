# CMake Integration

BTstack's CMake helpers let you add the BTstack code needed by a Bluetooth application without maintaining hand-written source-file and include-directory lists. They support two workflows:

- add BTstack to an existing CMake project that already owns its application target; or
- author a BTstack port that builds one or more applications for a board.

The helpers do not define a top-level project or choose a toolchain, SDK, board, or Bluetooth controller. Those decisions remain with your project or port.

## Concepts

### Source groups

A source group is a named collection of related BTstack source files. Request the groups needed by an application instead of adding individual files. Most group names correspond to a Bluetooth protocol or profile, such as `gatt-server` or `classic-rfcomm`. Others describe infrastructure, such as `transport-h4`, `platform-posix-run-loop`, chipset support, or third-party C code. Group names use lowercase hyphenated words.

The generated catalog in `tool/cmake/BTstackSources.cmake` records the source files, private include directories, and public include directories associated with groups. It also records BTstack's common public include directories. BlueKitchen maintains the catalog with `tool/create_cmake_sources.py`.

Groups are reusable BTstack components, not complete board configurations. A project or port still chooses its run loop, HCI transport, storage backend, chipset support, SDK integration, and board-specific files.

To browse the catalog from the BTstack repository root:

```sh
python3 tool/btstack_groups.py list
python3 tool/btstack_groups.py list --filter run-loop
python3 tool/btstack_groups.py info gatt-server
```

`info` shows a group's direct dependencies, resolved group set, required configuration macros, source files, and consumer include directories. Both source and include paths are relative to the BTstack root. Some direct dependencies are internal configuration markers, so they are not selectable groups and do not appear in the resolved source closure. Run `python3 tool/btstack_groups.py --help` for all commands and options. The wrapper requires CMake on `PATH`, but can run from any directory.

### Macros in `btstack_config.h`

BTstack is configured by the C preprocessor macros visible while it is compiled. `btstack_config.h` defines most of them, although the build can also provide additional compiler definitions. This header file is owned by the project or port; the CMake helpers do not generate or modify it.

The macros fall into three broad categories:

- `HAVE_*` describes services supplied by the platform, such as dynamic memory or an audio HAL.
- `ENABLE_*` selects BTstack protocols and features, such as Classic Bluetooth or BLE.
- Other macros set sizes, structure limits, and implementation options such as static-memory and persistent-storage configuration.

For the complete list and their C-level meaning, see the [general configuration reference](how_to.md#sec:btstackConfigHowTo).

### Capabilities

A capability is an optional service a port provides to applications, such as `audio` or `led`. A port declares a capability with `btstack_declare_port_capability`, supplying its source groups, port-local sources, public include directories, and compile definitions. The public include directories contain headers needed by the application or other consumers of the capability; they are added to the application library and passed to targets that link it.

Capabilities and `HAVE_*` macros can describe related aspects of a port, but they solve different problems:

| Use | Meaning | Effect |
| --- | --- | --- |
| `HAVE_*` macro | A C-level fact about the selected platform implementation. | Lets C code conditionally compile for that fact. It does not add source files, headers, or libraries. |
| Capability | An application-facing service offered by the port. | Adds the build inputs required by that service when an application requests it. |

For example, `HAVE_PORTAUDIO` can let a port's `main.c` select its PortAudio backend. The `audio` capability lets an application request audio without knowing whether the port uses PortAudio, an embedded driver, or another implementation. Discovering PortAudio and defining `HAVE_PORTAUDIO` alone does not make the `audio` capability available: the port must declare and integrate the service.

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO portaudio-2.0)

if(PORTAUDIO_FOUND)
    btstack_declare_port_capability(CAPABILITY audio
        GROUPS platform-posix-audio-portaudio platform-posix-wav
        DEFINES HAVE_PORTAUDIO)
endif()
```

The capability declaration does not find or link an external library. The port must still add PortAudio's include paths and link libraries to the targets that need them.

### Application `NEEDS`

An application declares its dependencies with `NEEDS`. Each entry is one of the following:

- a source group, such as `gatt-server`;
- a configuration macro, such as `ENABLE_LE_PERIPHERAL` or `HAVE_MALLOC`; or
- a capability, such as `audio`.

Source groups express the BTstack implementation required by the application. Macro and capability requirements decide whether a port can support it. `NEEDS` only describes and validates requirements; it never enables a BTstack feature. Enable features in `btstack_config.h` and provide optional services through port capabilities.

For example:

```cmake
btstack_declare_application(gatt_counter
    SOURCES gatt_counter.c
    GATT_DB gatt_counter.gatt
    NEEDS gatt-server ENABLE_LE_PERIPHERAL)
```

BTstack's built-in application declarations are in `example/requirements.cmake`. An external project can use the same declaration format. Relative `SOURCES` and `GATT_DB` paths are resolved against the declaration file; absolute paths are unchanged. Query the resolved values with `btstack_get_application_property(output application SOURCES)` or `btstack_get_application_property(output application GATT_DB)`.

### Resolving groups

Requested groups are only a starting point. The resolver follows group dependencies to produce the complete source-group set. For example, `gatt-server` also selects the ATT database, ATT dispatch, and LE support; a Classic profile can select RFCOMM, SDP, and lower protocol layers. `btstack_resolve_groups` returns the resolved group set and identifies its required `ENABLE_*` macros.

When CMake resolves an application for a port, it also checks the macro and capability entries in `NEEDS`. An application whose requirements are unavailable is excluded before CMake creates its targets.

## Configuring a port

A port has a common baseline and then builds the applications it supports. Its `CMakeLists.txt` normally proceeds in these stages:

1. Set up the project, toolchain, and board SDK, then include `tool/cmake/BTstack.cmake`.
2. Declare the port with `btstack_declare_port`.
3. Define the baseline groups and board-local sources used by every application.
4. Register the port-wide configuration from `btstack_config.h` with `btstack_configure_port`.
5. Discover optional dependencies and declare the capabilities that the port provides.
6. Declare or include application requirements, filter them for the port, and create targets for the selected applications.
7. Optionally add diagnostic targets that show the resolved result.

### Define the baseline

The baseline contains the groups and board-local files required by every application. Typical baseline groups provide the run loop, HCI transport, persistent storage, chipset support, and common cryptography. Board-local files—such as `main.c`, UART glue, controller firmware, and SDK sources—remain explicit because they belong to the board or project rather than the BTstack catalog.

The baseline must also cover dependencies that port code uses unconditionally. For example, a backend initialized unconditionally by `main.c` belongs in the baseline. If `main.c` initializes a backend only when a capability supplies a definition such as `HAVE_PORTAUDIO`, the capability can instead supply that backend for applications that request it.

### Register the port-wide configuration

`btstack_configure_port` preprocesses `btstack_config.h` at CMake configure time and registers the resulting `ENABLE_*` and `HAVE_*` macro names. Do not duplicate that macro list manually in CMake. Preprocessing supports conditional definitions and included headers, so CMake evaluates the same configuration logic as the compiler.

Call it after declaring the port and before filtering applications:

```cmake
btstack_declare_port(my-port)
btstack_configure_port(
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h"
    DEFINITIONS ${MY_PORT_COMPILE_DEFINITIONS}
    INCLUDE_DIRECTORIES ${MY_PORT_CONFIG_INCLUDE_DIRECTORIES})
```

Pass the port-wide definitions and include directories needed to preprocess `btstack_config.h`. Apply the same settings to compilation yourself: `btstack_configure_port` uses them for configuration-time preprocessing, but does not add them to targets.

### Select applications safely

The application filter uses the port-wide macro set and declared capabilities to decide whether an application is eligible. Capability definitions are not used to make an otherwise unsupported application eligible.

For each eligible capability request, the filter preprocesses `btstack_config.h` again with the capability's definitions. This verifies that they do not change the `HAVE_*` or `ENABLE_*` macros used for selection. A change is a configuration error, not a different selection result. Later, selected targets receive the capability definitions and the compiler evaluates the final, potentially application-specific configuration.

## Workflow: add BTstack to an existing CMake project

Use this workflow when your project already has an executable target, board support, and toolchain configuration. `btstack_add_library` resolves the selected groups, validates the configuration, creates a static library, and links it to your application target.

For example, an LE GATT server using an HCI H4/UART controller and POSIX run loop can start with:

```cmake
# Point to the BTstack checkout.
set(BTSTACK_ROOT "${CMAKE_CURRENT_LIST_DIR}/third_party/btstack")
include("${BTSTACK_ROOT}/tool/cmake/BTstack.cmake")

# Create a BTstack library from the groups used by this project.
btstack_add_library(my_btstack
    CONFIG "${CMAKE_CURRENT_LIST_DIR}/config/btstack_config.h"
    GROUPS gatt-server transport-h4 platform-posix-run-loop)

target_link_libraries(my_application PRIVATE my_btstack)
```

This is deliberately a starting point. Select the storage, chipset, and other groups required by your board and controller, and provide a matching `btstack_config.h`. Keep board-local and SDK sources in your own targets. For a complete POSIX H4 example, see `port/posix-h4/gatt-counter-standalone` and its README.

At configure time, `btstack_add_library` checks that `btstack_config.h` defines every `ENABLE_*` macro required by the selected groups. If preprocessing the header requires definitions or include paths, pass `CONFIG_DEFINITIONS` and `CONFIG_INCLUDE_DIRECTORIES`; the helper uses them for both validation and compilation. The configuration check runs at CMake configure time using the C compiler, so target-specific compiler flags and generator expressions are not available. Use the lower-level helpers if your SDK requires those settings.

To inspect the library before building the application:

```sh
cmake --build build --target library-info
```

The output lists requested and resolved groups, required and detected configuration macros, sources, and include paths. If a project creates more than one BTstack library, select one with `LIBRARY=my_btstack cmake --build build --target library-info`. With a multi-configuration generator, also pass `--config`.

If `btstack_add_library` does not fit your target layout, compose your own target with `btstack_resolve_groups` and the `btstack_collect_*` helpers. `btstack_target_warnings` can optionally apply BTstack's warning policy; the convenience helper does not modify project-wide warnings or suppress warnings in third-party sources.

## Workflow: author a BTstack port

Use this workflow when one board configuration should build multiple BTstack applications. First complete the port-configuration stages above: declare the port, define its baseline, register its port-wide configuration, and declare its optional capabilities. Then include application declarations and filter them for the port.

```cmake
include("${BTSTACK_ROOT}/example/requirements.cmake")
set(APPLICATIONS ${btstack_declared_applications})
btstack_filter_applications_for_port(APPLICATIONS
    CANDIDATES ${APPLICATIONS})
```

For every selected application, `btstack_add_application_library` combines its resolved groups with the port baseline. Group objects are shared between applications with the same capability definitions; applications with different definition sets receive separate group objects. Other port compilation settings must remain compatible across applications.

The port still owns its final executable and board policy:

```cmake
foreach(APPLICATION IN LISTS APPLICATIONS)
    btstack_add_application_library(
        TARGET btstack_${APPLICATION}_library
        APPLICATION ${APPLICATION}
        BASE_GROUPS ${MY_PORT_BASE_GROUPS}
        PORT_SOURCES ${MY_PORT_SOURCES}
        PRIVATE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

    btstack_add_application_executable(${APPLICATION} APPLICATION ${APPLICATION})
    btstack_target_warnings(${APPLICATION} STRICT)
    target_link_libraries(${APPLICATION} PRIVATE btstack_${APPLICATION}_library)
endforeach()
```

For an application with a declared `GATT_DB`, `btstack_add_application_executable` automatically invokes `btstack_target_gatt`. It runs the GATT compiler, adds the generated header directory to the include path, and tracks imported `.gatt` files through a depfile. You can also call it directly for an existing target. The application library provides BTstack's standard public include paths; the port remains responsible for its configuration and SDK include paths.

### Inspect a configured port

After the application loop, add BTstack's diagnostic targets:

```cmake
btstack_add_info_targets()
```

After configuring the port, use these targets to inspect the result:

```sh
cmake --build build --target port-info
cmake --build build --target list-applications
APPLICATION=gatt_counter cmake --build build --target application-info
cmake --build build --target list-examples
EXAMPLE=gatt_counter cmake --build build --target example-info
cmake --build build --target group-graph
```

`port-info` prints the port-wide configuration macro names and declared capabilities. `list-applications` lists candidates for the current port, and `application-info` explains whether a named application is included. For an excluded application, it reports missing macros or capabilities; for an included one, it prints capability definitions and the complete source and include-directory lists. Capability definitions are passed to compilation, but are not a complete listing of the macros produced when the compiler preprocesses the application.

`list-examples` and `example-info` remain available for BTstack's built-in example workflow; `list-examples` groups examples by radio requirements. `group-graph` writes `btstack-groups.dot` to the build directory and, when Graphviz is installed, also writes an SVG.

## Reusing resolved data in another build system

Even if your production build does not use CMake—for example, IAR Embedded Workbench or Keil uVision—you can configure a representative BTstack port with CMake and use `example-info` as the authoritative resolved input list:

```sh
EXAMPLE=gatt_counter cmake --build build --target example-info
```

This output is a resolved snapshot of one port configuration, not a project generator. Use it when creating the project in the other build system:

1. Add every reported source file, including application files, port-local files, and generated firmware sources when present.
2. Add every reported include directory. Keep the port configuration directory containing `btstack_config.h` ahead of generic include paths.
3. Use the same preprocessor definitions and board SDK libraries as the reference CMake port.
4. Reproduce generated artifacts: run `compile_gatt.py` for a declared GATT database and perform any controller firmware conversion or download step required by the selected port.

Compiler flags, linker scripts, startup code, SDK integration, and debugger settings remain the responsibility of that build system.
