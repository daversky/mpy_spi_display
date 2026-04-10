#include "display_dma.h"

// Stub implementations for platforms without DMA
bool display_dma_init(void *spi_obj, dma_handle_t *handle) {
    (void)spi_obj;
    (void)handle;
    return false;
}

void display_dma_deinit(dma_handle_t handle) {
    (void)handle;
}

size_t display_dma_send(dma_handle_t handle, const uint8_t *data, size_t len) {
    (void)handle;
    (void)data;
    (void)len;
    return 0;
}

bool display_dma_available(void *spi_obj) {
    (void)spi_obj;
    return false;
}

size_t display_dma_chunk_size(dma_handle_t handle) {
    (void)handle;
    return 0;
}