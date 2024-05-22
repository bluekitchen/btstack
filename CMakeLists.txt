# Parent/Meta CMake project created for the WinUSB port to work arround a limitation of Cmake in Visual Studio 2022.
# Open this project instead of ..\port\windows-winusb\CMakeLists.txt
# to enable a fully working CMake experience in Visual Studio 2022.
# This enables VS2022 to show all directories and files in the solution explorer and a couple of further features
# otherwise not working.

cmake_minimum_required(VERSION 3.12)

project(BTstack-Parent)

# Add subdirectories
add_subdirectory(${CMAKE_SOURCE_DIR}/port/windows-winusb-intel)
