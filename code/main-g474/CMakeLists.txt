cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME main-g474)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ../third_party/microrl/src/microrl.c
    ../common/console/console.c
    usb/usb_descriptors.c
    usb/usb_app.c
    # TinyUSB device stack with MIDI on the STM32 fsdev port
    ../third_party/tinyusb/src/tusb.c
    ../third_party/tinyusb/src/common/tusb_fifo.c
    ../third_party/tinyusb/src/device/usbd.c
    ../third_party/tinyusb/src/device/usbd_control.c
    ../third_party/tinyusb/src/class/midi/midi_device.c
    ../third_party/tinyusb/src/portable/st/stm32_fsdev/fsdev_common.c
    ../third_party/tinyusb/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c
)

# Force include our config.h for microrl.c (overrides third-party config.h)
set_source_files_properties(
    ../third_party/microrl/src/microrl.c
    PROPERTIES
    COMPILE_FLAGS "-include${CMAKE_CURRENT_SOURCE_DIR}/../common/console/config.h"
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ../third_party/microrl/src
    ../common/console
    ../common/spi_link
    usb
    ../third_party/tinyusb/src
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
    CFG_TUSB_MCU=OPT_MCU_STM32G4
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
