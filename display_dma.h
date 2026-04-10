#ifndef DISPLAY_DMA_H
#define DISPLAY_DMA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Opaque DMA handle type
typedef void* dma_handle_t;

// Initialize DMA for SPI transmission
// Returns true if DMA is supported and initialized
bool display_dma_init(void *spi_obj, dma_handle_t *handle);

// Deinitialize DMA
void display_dma_deinit(dma_handle_t handle);

// Send data via DMA
// Returns number of bytes sent, or 0 on error
size_t display_dma_send(dma_handle_t handle, const uint8_t *data, size_t len);

// Check if DMA is available for this platform/SPI
bool display_dma_available(void *spi_obj);

// Get optimal chunk size for DMA transfers
size_t display_dma_chunk_size(dma_handle_t handle);

#endif // DISPLAY_DMA_H