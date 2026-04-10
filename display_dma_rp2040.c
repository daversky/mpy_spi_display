#include "display_dma.h"
#include "extmod/machine_spi.h"
#include "py/mphal.h"

#ifdef PICO_PLATFORM

#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

typedef struct {
    spi_inst_t *spi_inst;
    int dma_channel;
    bool dma_busy;
} rp2040_dma_handle_t;

bool display_dma_init(void *spi_obj, dma_handle_t *handle) {
    machine_spi_obj_t *spi = (machine_spi_obj_t *)spi_obj;
    if (spi->spi_inst == NULL) {
        return false;
    }

    rp2040_dma_handle_t *dma_handle = m_new_obj(rp2040_dma_handle_t);
    if (!dma_handle) {
        return false;
    }

    dma_handle->spi_inst = spi->spi_inst;
    dma_handle->dma_busy = false;

    // Claim a DMA channel
    dma_handle->dma_channel = dma_claim_unused_channel(true);
    if (dma_handle->dma_channel < 0) {
        m_del_obj(rp2040_dma_handle_t, dma_handle);
        return false;
    }

    *handle = dma_handle;
    return true;
}

void display_dma_deinit(dma_handle_t handle) {
    if (handle) {
        rp2040_dma_handle_t *dma_handle = (rp2040_dma_handle_t *)handle;
        dma_channel_unclaim(dma_handle->dma_channel);
        m_del_obj(rp2040_dma_handle_t, dma_handle);
    }
}

size_t display_dma_send(dma_handle_t handle, const uint8_t *data, size_t len) {
    if (!handle) {
        return 0;
    }

    rp2040_dma_handle_t *dma_handle = (rp2040_dma_handle_t *)handle;

    // Wait for any previous DMA to complete
    while (dma_handle->dma_busy) {
        if (dma_channel_is_busy(dma_handle->dma_channel)) {
            mp_hal_delay_us(10);
        } else {
            dma_handle->dma_busy = false;
            break;
        }
    }

    // Configure DMA channel
    dma_channel_config cfg = dma_channel_get_default_config(dma_handle->dma_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, spi_get_dreq(dma_handle->spi_inst, true));

    dma_channel_configure(
        dma_handle->dma_channel,
        &cfg,
        &spi_get_hw(dma_handle->spi_inst)->dr,  // Write address
        data,                                    // Read address
        len,                                     // Transfer count
        false                                    // Don't start yet
    );

    dma_handle->dma_busy = true;
    dma_channel_start(dma_handle->dma_channel);

    return len;
}

bool display_dma_available(void *spi_obj) {
    machine_spi_obj_t *spi = (machine_spi_obj_t *)spi_obj;
    // DMA available for both SPI0 and SPI1 on RP2040
    return (spi->spi_inst != NULL);
}

size_t display_dma_chunk_size(dma_handle_t handle) {
    (void)handle;
    // RP2040 DMA works best with 1024 byte chunks
    return 1024;
}

#else

// Stub implementations for non-RP2040
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

#endif