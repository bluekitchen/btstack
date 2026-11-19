# CMake Integration

BTstack's CMake helpers let you add the BTstack code required by a particular Bluetooth application without maintaining a hand-written list of source files and include directories. They work in two situations: integrating BTstack into an existing CMake project, and creating a BTstack port that builds one or more examples for a board.

The helpers do not define a top-level project or select a toolchain, a SDK, a board, or a Bluetooth controller. Thse remain the responsibility of your project or port. This chapter first explains the building blocks used by the helpers, then walks through each workflow.

## Core concepts

### Source groups

A source group is a named collection of related BTstack source files. Instead of adding individual files, this allows to add the groups needed by your application. Most group names correspond to a Bluetooth protocol or profile, such as `gatt-server` or `classic-rfcomm`; others refer to infrastructure such as `transport-h4`, `platform-posix-run-loop`, a chipset support, or third-party C code. Group names use lowercase hyphenated words. The complete catalog is in `tool/cmake/BTstackSources.cmake`.

The catalog also declares:
- private include directories needed for each source group, and
- BTstack’s common public include directories.

BlueKitchen maintains this generated catalog with `tool/create_cmake_sources.py`.

The CMake helpers use catalog to add the neccessary group-specific public include directories when an application uses that group’s API. 

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

### Configuration

BTstack is configured by the effective set of C preprocessor macros used for compilation. `btstack_config.h` normally defines most of them, but a port's CMake code can add definitions after detecting the SDK or host system. The effective configuration—not `btstack_config.h` alone—controls the build.

The macros fall into three broad categories:

- `HAVE_*` describes services supplied by the platform, such as dynamic memory or an audio HAL.
- `ENABLE_*` selects BTstack protocols and features, such as Classic Bluetooth.
- Other configuration macros set sizes, structure limits, and other implementation options.

The helpers use the C preprocessor to evaluate the effective macros, rather than parsing `btstack_config.h` as plain text. This supports any valid C preprocessor constructs in the header, including conditional definitions and included headers. Supply the same preprocessor definitions and include directories used by compilation when these affect the configuration. The [general configuration reference](how_to.md#sec:btstackConfigHowTo) lists the available macros.

### Capabilities

A capability is a CMake-level name for an optional service a port makes available to applications, such as `audio` or `led`. A port declares it with `btstack_declare_port_capability`, associating the service with the source groups, port-local sources, and public include directories that provide it.

Capabilities and `HAVE_*` macros can describe related parts of a port, but they serve different purposes:

- A `HAVE_*` macro is a configuration fact available to C code. For example, `HAVE_PORTAUDIO` can select the PortAudio implementation in `main.c`. A dependency on that macro asks whether the exact macro is defined; it does not add the implementation's build inputs.
- A capability describes a service available to an application. An application that needs `audio` can request it without knowing whether the port uses PortAudio, an embedded audio driver, or another backend. The capability declaration supplies the build inputs for the port's implementation.

Both can appear in an application's `NEEDS`, but they are not interchangeable. Use a macro when the application needs that particular configuration; use a capability when it needs a port-provided service independent of its implementation.

#### Example: PortAudio and the `audio` capability

A POSIX port can detect PortAudio and use the result to configure both C compilation and the resolver:

```cmake
btstack_declare_port(my-port)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO portaudio-2.0)
if(PORTAUDIO_FOUND)
    btstack_declare_port_capability(CAPABILITY audio
        GROUPS platform-posix-audio-portaudio platform-posix-wav
        DEFINES HAVE_PORTAUDIO)
endif()

btstack_configure_port(CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h")
```

When an application requests `audio`, its library, its consumers, and its source groups are compiled with `HAVE_PORTAUDIO`. Port code can use that definition to initialize the backend. Applications that do not request `audio` do not receive the definition or provider groups. The port must additionally provide PortAudio's include paths and link libraries to targets that need them; the capability declaration does not find or link the external library.

Capability definitions may affect conditional compilation and configuration details, but must not change the `ENABLE_*` or `HAVE_*` macros used to select applications or baseline groups. Selection uses the port-wide configuration, before capability definitions are applied. When the port uses `btstack_configure_port`, the application filter checks this restriction against declared application and group requirements to prevent circular dependencies. Ports that register macros manually must ensure the same invariant themselves.

For example, an application declaration can use `NEEDS core audio`. When the port declares `audio`, the resolver includes its provider groups. When the capability is absent, the port's application filter excludes the application. An embedded port can satisfy the same request with different groups or port-local sources and need not define `HAVE_PORTAUDIO`.

The port owns the relationship between its detected configuration and its capabilities. The shared resolver does not infer `audio` from `HAVE_PORTAUDIO`: finding a library alone does not establish that a port has integrated and initialized a usable audio service. The conditional declaration above is a port-specific rule expressing that relationship.

### Application `NEEDS`

An application declares its dependencies with the `NEEDS` property. Each entry names a source group, a configuration macro expected in `btstack_config.h`, or a capability supplied by the port. Source groups express the BTstack implementation needed by the application; configuration macros and capabilities describe what must be available in the selected port.

`NEEDS` describes and validates compatibility; it never enables a BTstack feature. Enabling or disabling features remains the responsibility of `btstack_config.h` and the project's build settings.

### Resolving dependencies

Source groups, capabilities, and configuration macros are all dependencies. A resolver combines the requested dependencies and resolves their transitive requirements into the complete source-group set and the configuration macros that must be enabled.

Source groups can depend on other source groups. For example, requesting `gatt-server` also selects the ATT database, ATT dispatch, and LE support; a Classic profile can select RFCOMM, SDP, and lower protocol layers. This lets an application request only the groups it needs. When resolving applications for a port, the configured macros are checked as dependencies too. If `ENABLE_CLASSIC` is disabled in `btstack_config.h`, applications requiring Classic profiles are not selected, so those profiles do not end up in the source-file list.

To see what selecting a group implies, inspect it directly:

```sh
python3 tool/btstack_groups.py info gatt-server
```

The output shows its direct dependencies, the fully resolved group set, required configuration macros, source files, and public include directories required by applications. Source paths are relative to the BTstack root; public include paths are absolute. Some direct dependencies are internal configuration markers, so they are not selectable groups and do not appear in the resolved source closure.

`btstack_resolve_groups` returns the complete group set and identifies the `ENABLE_*` configuration macros it requires, such as `ENABLE_BLE` or `ENABLE_CLASSIC`.

### Port baseline

A port defines a baseline: the groups and board-local files required by every application it builds. Typical baseline groups provide the run loop, HCI transport, persistent storage, chipset support, and common cryptography. Board-local files — such as `main.c`, UART adapter, controller firmware, and SDK sources — remain explicit because they belong to the board or project rather than the BTstack catalog.

The baseline must also cover dependencies used by the port itself, regardless of an application's `NEEDS`. For example, a backend initialized unconditionally by `main.c` belongs in the baseline. When `main.c` initializes PortAudio only under a capability-provided `HAVE_PORTAUDIO` definition, the capability instead supplies the backend for the applications that request it.

#### Typical port structure

A port's `CMakeLists.txt` mainly follows this order:

1. Set up the project, toolchain, and board SDK, then include `tool/cmake/BTstack.cmake`.
2. Declare the port with `btstack_declare_port`.
3. Define its baseline groups and board-local sources.
4. Register the effective `btstack_config.h` and optional capabilities.
5. Include application declarations, filter them for the port, and create the selected application libraries and executables.
6. Optionally add diagnostic targets for inspecting the resolved port.

The SDK setup, board sources, and final executable policy remain port-specific. This section introduces the baseline and configuration in steps 2–4. The [Port authoring](#port-authoring) section covers application declarations and building in step 5, while [Inspect a configured port](#inspect-a-configured-port) covers the optional diagnostics in step 6.

#### Registering port configuration

A port does not need to repeat its configuration macros manually in CMake: `btstack_configure_port` preprocesses `btstack_config.h` and registers its effective macro set. To keep CMake selection aligned with compilation, source groups must match the effective configuration. Call `btstack_configure_port` after declaring the port so CMake can reject or omit groups and applications whose required features are disabled.

A minimal port setup looks like this:

```cmake
btstack_declare_port(my-port)
btstack_configure_port(
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h"
    DEFINITIONS ${MY_PORT_COMPILE_DEFINITIONS})
```

Pass to `DEFINITIONS` the same preprocessor definitions used when compiling the port, especially when `btstack_config.h` enables features conditionally. `btstack_declare_port` makes that port current for helpers called in the same directory and its children. If you declare more than one port, the last declared port is current; use `PORT <name>` to select another one explicitly. Helpers report an error if no current port exists.

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

A port can declare the applications it supports before creating targets for them. BTstack's examples are declared in `example/requirements.cmake`, but an external project can use the same declaration format. A declaration describes an application's sources, optional GATT database, and [`NEEDS`](#application-needs); it does not create a target or enable features:

```cmake
btstack_declare_application(gatt_counter
    SOURCES gatt_counter.c
    GATT_DB gatt_counter.gatt
    NEEDS gatt-server ENABLE_LE_PERIPHERAL)
```

Relative paths in `SOURCES` and `GATT_DB` are interpreted relative to the declaration file; absolute paths are unchanged. Query the resolved values with `btstack_get_application_property(output application SOURCES)` or `btstack_get_application_property(output application GATT_DB)`.

### Build applications declared by a port

For each supported application, ports normally create a small BTstack static library. `btstack_add_application_library` combines the application's requirements with the port baseline and compiles the resulting closure. Group objects are shared between applications with the same capability definitions; different definition sets get separate group objects. Other port compilation settings must remain compatible across applications.

The following diagram follows `a2dp_sink_demo` through the libusb port when PortAudio is available:

```text
 Application declaration                  libusb port
 ┌──────────────────────────────┐         ┌───────────────────────────────┐
 │ a2dp_sink_demo.c             │         │ btstack_config.h + build      │
 │ NEEDS: Classic audio groups, │         │ definitions → effective       │
 │        AVRCP groups, audio…  │         │ configuration macros          │
 └──────────────┬───────────────┘         │                               │
                │                         │ PortAudio found → audio       │
                │                         │ capability:                   │
                │                         │   groups: PortAudio, WAV      │
                │                         │   defines: HAVE_PORTAUDIO     │
                │                         │                               │
                │                         │ baseline groups + main.c      │
                │                         └───────────────┬───────────────┘
                └─────────────────┬───────────────────────┘
                                  ▼
                    Filter application for the port
                    Check required configuration macros
                    and availability of capabilities
                                  │
                      unsupported ├──────→ Exclude application
                                  │ supported
                                  ▼
                    Resolve groups and their dependencies
                    Add baseline and audio provider groups
                                  │
                                  ▼
                    Compile resolved group objects and
                    main.c with HAVE_PORTAUDIO
                                  │
                                  ▼
                    Application-specific BTstack library
                                  │
                                  ▼
                    Compile a2dp_sink_demo.c and link it
                    with the library and port dependencies
                                  │
                                  ▼
                         a2dp_sink_demo executable
```

The port-wide configuration is checked before capability definitions are applied. If PortAudio is unavailable, the libusb port does not declare `audio`, and the filter excludes this application. An application such as `gatt_counter` does not request `audio`, so its resolved groups and definitions do not include the PortAudio provider.

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

`port-info` shows the effective BTstack configuration macros and declared capabilities. `list-applications` shows candidates for the current port, and `application-info` explains whether a selected application is included. For excluded applications, it reports missing macros or capabilities; for included applications, it prints capability definitions and the source and include-directory lists.

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


