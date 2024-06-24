#include <stdint.h>
#include <inttypes.h>
#include "rvswd.h"

rvswd_result_t rvswd_init(rvswd_handle_t* handle) {
    gpio_config_t swio_cfg = {
        .pin_bit_mask = BIT64(handle->swdio),
        .mode         = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en   = false,
        .pull_down_en = false,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t res = gpio_config(&swio_cfg);
    if (res != ESP_OK) {
        return RVSWD_FAIL;
    }

    gpio_config_t swck_cfg = {
        .pin_bit_mask = BIT64(handle->swclk),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = false,
        .pull_down_en = false,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    res = gpio_config(&swck_cfg);
    if (res != ESP_OK) {
        return RVSWD_FAIL;
    }

    return RVSWD_OK;
}

rvswd_result_t rvswd_start(rvswd_handle_t* handle) {
    // Start with both lines high
    gpio_set_level(handle->swdio, true);
    gpio_set_level(handle->swclk, true);
    vTaskDelay(pdMS_TO_TICKS(50));

    // Pull data low
    gpio_set_level(handle->swdio, false);
    gpio_set_level(handle->swclk, true);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Pull clock low
    gpio_set_level(handle->swdio, false);
    gpio_set_level(handle->swclk, false);
    vTaskDelay(pdMS_TO_TICKS(10));
    return RVSWD_OK;
}

rvswd_result_t rvswd_stop(rvswd_handle_t* handle) {
    // Pull data low
    gpio_set_level(handle->swdio, false);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(handle->swclk, true);
    vTaskDelay(pdMS_TO_TICKS(50));
    // Let data float high
    gpio_set_level(handle->swdio, true);
    vTaskDelay(pdMS_TO_TICKS(10));
    return RVSWD_OK;
}

rvswd_result_t rvswd_reset(rvswd_handle_t* handle) {
    gpio_set_level(handle->swdio, true);
    vTaskDelay(pdMS_TO_TICKS(10));
    for (uint8_t i = 0; i < 100; i++) {
        gpio_set_level(handle->swclk, false);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(handle->swclk, true);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return rvswd_stop(handle);
}

void rvswd_write_bit(rvswd_handle_t* handle, bool value) {
    printf("write %u\n", value);
    gpio_set_level(handle->swdio, value);
    gpio_set_level(handle->swclk, false);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(handle->swclk, true); // Data is sampled on rising edge of clock
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(handle->swclk, false);
}

bool rvswd_read_bit(rvswd_handle_t* handle) {
    gpio_set_level(handle->swdio, true);
    gpio_set_level(handle->swclk, false);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(handle->swclk, true); // Data is output on rising edge of clock
    vTaskDelay(pdMS_TO_TICKS(10));
    bool res = gpio_get_level(handle->swdio);
    gpio_set_level(handle->swclk, false);
    return res;
}

rvswd_result_t rvswd_write(rvswd_handle_t* handle, uint8_t reg, uint32_t value) {
    bool parity;

    printf("start\n");

    rvswd_start(handle);

    printf("addr host\n");

    // ADDR HOST
    parity = false; // This time it's odd parity?
    for (uint8_t position = 0; position < 7; position++) {
        bool bit = (reg >> (6 - position)) & 1;
        rvswd_write_bit(handle, bit);
        if (bit) parity = !parity;
    }

    printf("operation write\n");

    //Operation: write
    rvswd_write_bit(handle, true);
    parity = !parity;

    printf("parity %u\n", parity);

    // Parity bit (even)
    rvswd_write_bit(handle, parity);

    printf("trailing bytes\n");

    rvswd_write_bit(handle, 1);
    rvswd_write_bit(handle, 0);
    rvswd_write_bit(handle, 1);
    rvswd_write_bit(handle, 0);
    rvswd_write_bit(handle, 1);

    printf("data %08" PRIx32 "\n", value);

    // Data
    parity = false; // This time it's even parity?
    for (uint8_t position = 0; position < 32; position++) {
        bool bit = (value >> (31 - position)) & 1;
        rvswd_write_bit(handle, bit);
        if (bit) parity = !parity;
    }

    printf("parity %u\n", parity);

    // Parity bit
    rvswd_write_bit(handle, parity);

    printf("trailing bytes\n");

    rvswd_write_bit(handle, 1);
    rvswd_write_bit(handle, 0);
    rvswd_write_bit(handle, 1);
    rvswd_write_bit(handle, 1);
    rvswd_write_bit(handle, 1);

    printf("stop\n");

    rvswd_stop(handle);

    return RVSWD_OK;
}

rvswd_result_t rvswd_read(rvswd_handle_t* handle, uint8_t reg, uint32_t* value) {
    bool parity;

    printf("start\n");

    rvswd_start(handle);

    printf("addr host\n");

    // ADDR HOST
    parity = false; // This time it's odd parity?
    for (uint8_t position = 0; position < 7; position++) {
        bool bit = (reg >> (6 - position)) & 1;
        rvswd_write_bit(handle, bit);
        if (bit) parity = !parity;
    }

    printf("operation read\n");

    //Operation: read
    rvswd_write_bit(handle, false);

    printf("parity %u\n", parity);

    // Parity bit (even)
    rvswd_write_bit(handle, parity);

    printf("trailing bytes\n");

    rvswd_write_bit(handle, 1);
    rvswd_write_bit(handle, 0);
    rvswd_write_bit(handle, 1);
    rvswd_write_bit(handle, 0);
    rvswd_write_bit(handle, 1);

    *value = 0;

    // Data
    parity = false; // This time it's even parity?
    for (uint8_t position = 0; position < 32; position++) {
        bool bit = rvswd_read_bit(handle);
        if (bit) {
            *value |= 1 << (31 - position);
        }
        if (bit) parity = !parity;
    }

    // Parity bit
    bool parity_read = rvswd_read_bit(handle);

    printf("calculated parity %u, read parity %u\n", parity, parity_read);

    printf("trailing bytes\n");

    rvswd_write_bit(handle, 1);
    rvswd_write_bit(handle, 0);
    rvswd_write_bit(handle, 1);
    rvswd_write_bit(handle, 1);
    rvswd_write_bit(handle, 1);

    printf("stop\n");

    rvswd_stop(handle);

    return (parity == parity_read) ? RVSWD_OK : RVSWD_FAIL;
}
