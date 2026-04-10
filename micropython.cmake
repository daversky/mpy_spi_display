# Create a library for the SPI display module
set(MOD_NAME spi_displays)

# Define source files
set(MOD_SRC
    ${CMAKE_CURRENT_LIST_DIR}/modspidisplay.c
    ${CMAKE_CURRENT_LIST_DIR}/display.c
)

# Platform-specific DMA implementation
if(CMAKE_SYSTEM_PROCESSOR MATCHES "xtensa|esp32")
    list(APPEND MOD_SRC ${CMAKE_CURRENT_LIST_DIR}/display_dma_esp32.c)
    target_compile_definitions(${MICROPY_TARGET} PRIVATE
        DISPLAY_DMA_ESP32=1
    )
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "cortex-m0plus" OR PICO_PLATFORM)
    list(APPEND MOD_SRC ${CMAKE_CURRENT_LIST_DIR}/display_dma_rp2040.c)
    target_compile_definitions(${MICROPY_TARGET} PRIVATE
        DISPLAY_DMA_RP2040=1
    )
else()
    list(APPEND MOD_SRC ${CMAKE_CURRENT_LIST_DIR}/display_dma_stub.c)
endif()

# Register the module with MicroPython
# Note: This uses the MicroPython build system's variable
if(NOT DEFINED USER_C_MODULES)
    set(USER_C_MODULES "")
endif()

# Add our sources to the build
list(APPEND USER_C_MODULES ${MOD_SRC})

# Add include directories
set(MOD_INCLUDES
    ${CMAKE_CURRENT_LIST_DIR}
)

# Make variables available to parent scope
set(USER_C_MODULES ${USER_C_MODULES} PARENT_SCOPE)
set(USER_C_MODULES_INCLUDES ${MOD_INCLUDES} PARENT_SCOPE)