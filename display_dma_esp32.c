#include "display_dma.h"
#include "extmod/machine_spi.h"
#include "py/mphal.h"

#if defined(ESP32) || defined(ESP32C3) || defined(ESP32S3)

#include "driver/spi_master.h"
#include "esp_log.h"

bool display_dma_init(void *spi_obj, dma_handle_t *handle) {
    machine_spi_obj_t *spi = (machine_spi_obj_t *)spi_obj;
    if (spi->host == NULL) {
        return false;
    }

    // ESP32 SPI driver already supports DMA internally
    // We just store the SPI host handle
    *handle = spi->host;

    // Configure for maximum throughput
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = spi->baudrate,
        .mode = spi->phase ? 1 : 0,  // SPI mode
        .queue_size = 7,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .pre_cb = NULL,
        .post_cb = NULL,
    };

    spi_device_handle_t dev_handle;
    esp_err_t ret = spi_bus_add_device(spi->host, &devcfg, &dev_handle);
    if (ret != ESP_OK) {
        return false;
    }

    *handle = dev_handle;
    return true;
}

void display_dma_deinit(dma_handle_t handle) {
    if (handle) {
        spi_bus_remove_device((spi_device_handle_t)handle);
    }
}

size_t display_dma_send(dma_handle_t handle, const uint8_t *data, size_t len) {
    if (!handle) {
        return 0;
    }

    spi_device_handle_t dev = (spi_device_handle_t)handle;

    // Use SPI transaction with DMA
    spi_transaction_t trans = {
        .length = len * 8,
        .tx_buffer = data,
        .rx_buffer = NULL,
    };

    esp_err_t ret = spi_device_transmit(dev, &trans);
    if (ret != ESP_OK) {
        return 0;
    }

    return len;
}

bool display_dma_available(void *spi_obj) {
    // DMA always available on ESP32 SPI
    return true;
}

size_t display_dma_chunk_size(dma_handle_t handle) {
    // ESP32 DMA can handle large transfers efficiently
    // Use 4096 bytes as chunk size
    return 4096;
}

#else

// Stub implementations for non-ESP32
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