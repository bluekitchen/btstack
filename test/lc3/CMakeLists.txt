cmake_minimum_required (VERSION 3.12)
project(BTstack-Test-LC3)
set (CMAKE_CXX_STANDARD 11)

# find pkgconfig
find_package(PkgConfig REQUIRED)

# portaudio
pkg_check_modules(PORTAUDIO portaudio-2.0)
if(PORTAUDIO_FOUND)
    include_directories(${PORTAUDIO_INCLUDE_DIRS})
    link_directories(${PORTAUDIO_LIBRARY_DIRS})
    link_libraries(${PORTAUDIO_LIBRARIES})
    add_compile_definitions(HAVE_PORTAUDIO)
endif()

# local dir for btstack_config.h after build dir to avoid using .h from Makefile
include_directories(.)

include_directories(../../3rd-party/lc3-google/include)
include_directories(../../3rd-party/tinydir)
include_directories(../../platform/posix)
include_directories(../../src)

file(GLOB SOURCES_POSIX     "../../platform/posix/*.c")
file(GLOB SOURCES_SRC       "../../src/*.c" "../../src/*.cpp")
file(GLOB SOURCES_LC3_GOOGLE "../../3rd-party/lc3-google/src/*.c")

# Enable ASAN
add_compile_options( -g -fsanitize=address)
add_link_options(       -fsanitize=address)

# create targets
file(GLOB EXAMPLES "lc3_*.c")
foreach(EXAMPLE_FILE ${EXAMPLES})
	get_filename_component(EXAMPLE ${EXAMPLE_FILE} NAME_WE)
	set (SOURCE_FILES ${SOURCES_POSIX} ${SOURCES_SRC} ${SOURCES_LC3_GOOGLE} ${EXAMPLE_FILE})
	message("Tool: ${EXAMPLE}")
	add_executable(${EXAMPLE} ${SOURCE_FILES} )
endforeach(EXAMPLE_FILE)
