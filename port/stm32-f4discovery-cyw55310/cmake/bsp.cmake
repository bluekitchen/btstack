if(DEFINED BSP)
    set( BSP_SOURCE_DIR ${STM32F4CUBE_PATH}/Drivers/BSP )
    include_directories(${BSP_SOURCE_DIR}/${BSP})

    # Locate sources
    file(GLOB BSP_SOURCES ${BSP_SOURCE_DIR}/${BSP}/*.c)

    # Add components
    # BSP_COMPONENTS should be a list of components to be included
    foreach(COMPONENT ${BSP_COMPONENTS})
        include_directories(${BSP_SOURCE_DIR}/Components/${COMPONENT}/)
        file(GLOB_RECURSE COMPONENT_SOURCES ${BSP_SOURCE_DIR}/Components/${COMPONENT}/*.c)
        set(BSP_COMPONENT_SOURCES ${BSP_COMPONENT_SOURCES} ${COMPONENT_SOURCES})
    endforeach(COMPONENT)

    # Create bsp library
    add_library(bsp ${BSP_SOURCES} ${BSP_COMPONENT_SOURCES})

    # stm32f4_discovery_audio.c implements BTstack's audio callback hooks.
    # The BSP is compiled as its own target, so it must explicitly receive
    # the embedded platform headers rather than relying on an example target.
    target_include_directories(bsp PRIVATE
        ${CMAKE_SOURCE_DIR}/../../src
        ${CMAKE_SOURCE_DIR}/../../platform/embedded
    )

    # Ignore unused parameters in the STM32CubeF4 BSP sources.
    target_compile_options(bsp PRIVATE -Wno-unused-parameter)

    target_link_libraries(bsp stm32cubemx)

    # Add library to the build
    set(LIBS ${LIBS} bsp)

endif(DEFINED BSP)

