
// SPDX-License-Identifier: MIT

#include "esptoolsquared.h"

#include "chips.h"

#include <esp_app_format.h>
#include <esp_log.h>
#include <string.h>

static char const TAG[] = "et2";

#define ET2_TIMEOUT pdMS_TO_TICKS(1000)

#define RETURN_ON_ERR(x, ...)                                                                                          \
    do {                                                                                                               \
        esp_err_t err = (x);                                                                                           \
        if (err) {                                                                                                     \
            __VA_ARGS__;                                                                                               \
            return err;                                                                                                \
        }                                                                                                              \
    } while (0)

#define LEN_CHECK_MIN(resp, resp_len, exp_len, ...)                                                                    \
    do {                                                                                                               \
        if ((resp_len) < (exp_len)) {                                                                                  \
            ESP_LOGE(TAG, "Invalid response length; expected %zu, got %zu", (size_t)(exp_len), (size_t)(resp_len));    \
            free((resp));                                                                                              \
            __VA_ARGS__;                                                                                               \
            return ESP_ERR_INVALID_RESPONSE;                                                                           \
        }                                                                                                              \
    } while (0)

#define LEN_CHECK(resp, resp_len, exp_len, ...)                                                                        \
    do {                                                                                                               \
        if ((resp_len) != (exp_len)) {                                                                                 \
            ESP_LOGE(TAG, "Invalid response length; expected %zu, got %zu", (size_t)(exp_len), (size_t)(resp_len));    \
            free((resp));                                                                                              \
            __VA_ARGS__;                                                                                               \
            return ESP_ERR_INVALID_RESPONSE;                                                                           \
        }                                                                                                              \
    } while (0)



// Send raw bytes.
static esp_err_t et2_raw_tx(void const *data, size_t len);
// Receive raw bytes.
static esp_err_t et2_raw_rx(void *data, size_t len);
// Change interface speed.
static esp_err_t et2_setfreq(long freq);
// Send SLIP packet start/end.
static esp_err_t et2_slip_send_startstop();
// Send SLIP packet data.
static esp_err_t et2_slip_send_data(void const *data, size_t len);
// Receive and decode a SLIP packet.
static esp_err_t et2_slip_recv(void **resp, size_t *resp_len);
// Send a command.
static esp_err_t
    et2_send_cmd(et2_cmd_t cmd, void const *param, size_t param_len, void **resp, size_t *resp_len, uint32_t *val);

// Command header.
typedef struct {
    uint8_t  resp;
    uint8_t  cmd;
    uint16_t len;
    uint32_t chk;
} et2_hdr_t;
// Security info data.
typedef struct {
    uint32_t flags;
    uint8_t  key_count;
    uint8_t  key_purpose[7];
    uint32_t chip_id;
} et2_sec_info_t;

// Current UART.
static uart_port_t       cur_uart;
// Current chip ID value.
static uint32_t          chip_id;
// Current chip attributes.
static et2_chip_t const *chip_attr;



void et2_test() {
    uint32_t chip_id;
    ESP_ERROR_CHECK(et2_detect(&chip_id));
    ESP_LOGI("et2", "ESP32 detected; chip id 0x%08" PRIx32, chip_id);
    ESP_ERROR_CHECK(et2_run_stub());
}

// Set interface used to a UART.
esp_err_t et2_setif_uart(uart_port_t uart) {
    cur_uart = uart;
    return ESP_OK;
}

// Wait for the ROM "waiting for download" message.
static esp_err_t et2_wait_dl() {
    char const msg[] = "waiting for download\r\n";
    size_t     i     = 0;
    TickType_t lim   = xTaskGetTickCount() + ET2_TIMEOUT * 5;
    while (xTaskGetTickCount() < lim) {
        char rxd = 0;
        if (et2_raw_rx(&rxd, 1) == ESP_OK) {
            if (rxd != msg[i]) {
                ESP_LOGV(TAG, "NE %zu", i);
                i = 0;
            }
            if (rxd == msg[i]) {
                ESP_LOGV(TAG, "EQ %zu", i);
                i++;
                if (i >= strlen(msg)) {
                    ESP_LOGI(TAG, "Download boot detected");
                    return ESP_OK;
                }
            }
        }
    }
    ESP_LOGI(TAG, "Download boot timeout");
    return ESP_ERR_TIMEOUT;
}

// Try to connect to and synchronize with the ESP32.
esp_err_t et2_sync() {
    RETURN_ON_ERR(et2_wait_dl());
    // clang-format off
    uint8_t const sync_rom[] = {
        0x07, 0x07, 0x12, 0x20,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    };
    // clang-format on

    for (int i = 0; i < 5; i++) {
        void    *data;
        size_t   len;
        uint32_t val;
        if (et2_send_cmd(ET2_CMD_SYNC, sync_rom, sizeof(sync_rom), &data, &len, &val) == ESP_OK) {
            free(data);
            if (val != 0) {
                ESP_LOGI(TAG, "Sync received");
                return ESP_OK;
            }
        }
    }

    ESP_LOGI(TAG, "Sync timeout");
    return ESP_ERR_TIMEOUT;
}

// Set attributes according to chip ID.
void check_chip_id() {
    switch (chip_id & 0xffff) {
#ifdef CONFIG_ET2_SUPPORT_ESP32C3
        case ESP_CHIP_ID_ESP32C3: chip_attr = &et2_chip_esp32c3; break;
#else
        case ESP_CHIP_ID_ESP32C3: ESP_LOGW(TAG, "ESP32-C3 not supported!"); break;
#endif
#ifdef CONFIG_ET2_SUPPORT_ESP32C2
        case ESP_CHIP_ID_ESP32C2: chip_attr = &et2_chip_esp32c2; break;
#else
        case ESP_CHIP_ID_ESP32C2: ESP_LOGW(TAG, "ESP32-C2 not supported!"); break;
#endif
#ifdef CONFIG_ET2_SUPPORT_ESP32C6
        case ESP_CHIP_ID_ESP32C6: chip_attr = &et2_chip_esp32c6; break;
#else
        case ESP_CHIP_ID_ESP32C6: ESP_LOGW(TAG, "ESP32-C6 not supported!"); break;
#endif
#ifdef CONFIG_ET2_SUPPORT_ESP32P4
        case ESP_CHIP_ID_ESP32P4: chip_attr = &et2_chip_esp32p4; break;
#else
        case ESP_CHIP_ID_ESP32P4: ESP_LOGW(TAG, "ESP32-P4 not supported!"); break;
#endif
#ifdef CONFIG_ET2_SUPPORT_ESP32S2
        case ESP_CHIP_ID_ESP32S2: chip_attr = &et2_chip_esp32s2; break;
#else
        case ESP_CHIP_ID_ESP32S2: ESP_LOGW(TAG, "ESP32-S2 not supported!"); break;
#endif
#ifdef CONFIG_ET2_SUPPORT_ESP32S3
        case ESP_CHIP_ID_ESP32S3: chip_attr = &et2_chip_esp32s3; break;
#else
        case ESP_CHIP_ID_ESP32S3: ESP_LOGW(TAG, "ESP32-S3 not supported!"); break;
#endif
        default: ESP_LOGW(TAG, "Unknown chip ID 0x%04" PRIX32, chip_id & 0xffff); break;
    }
}

// Detect an ESP32 and, if present, read its chip ID.
// If a pointer is NULL, the property is not read.
esp_err_t et2_detect(uint32_t *chip_id_out) {
    void  *resp;
    size_t resp_len;
    RETURN_ON_ERR(et2_send_cmd(ET2_CMD_SEC_INFO, NULL, 0, &resp, &resp_len, NULL));
    LEN_CHECK_MIN(resp, resp_len, sizeof(et2_sec_info_t));
    chip_id = ((et2_sec_info_t *)resp)->chip_id;
    check_chip_id();
    *chip_id_out = chip_id;
    free(resp);
    return ESP_OK;
}

// Upload and start a flasher stub.
esp_err_t et2_run_stub() {
    uint32_t chip_id;
    RETURN_ON_ERR(et2_detect(&chip_id));
    if (!chip_attr) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    // Upload the stub.
    ESP_LOGI(TAG, "Uploading flasher stub...");
    RETURN_ON_ERR(
        et2_mem_write(chip_attr->stub->text_start, chip_attr->stub->text, chip_attr->stub->text_len),
        ESP_LOGE(TAG, "Failed to upload stub")
    );
    RETURN_ON_ERR(
        et2_mem_write(chip_attr->stub->data_start, chip_attr->stub->data, chip_attr->stub->data_len),
        ESP_LOGE(TAG, "Failed to upload stub")
    );

    // Start the stub.
    ESP_LOGI(TAG, "Starting flasher stub...");
    RETURN_ON_ERR(et2_cmd_mem_end(chip_attr->stub->entry), ESP_LOGE(TAG, "Failed to start stub"));

    // Verify that the stub has successfully started.
    void  *resp;
    size_t resp_len;
    RETURN_ON_ERR(et2_slip_recv(&resp, &resp_len), ESP_LOGE(TAG, "Stub did not respond"));
    if (resp_len != 4 || memcmp(resp, "OHAI", 4)) {
        ESP_LOGE(TAG, "Unexpected response from stub");
        free(resp);
        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}



// Send raw bytes.
static esp_err_t et2_raw_tx(void const *data, size_t len) {
    ESP_LOGV(TAG, "TX %zd", len);
    int res = uart_write_bytes(cur_uart, data, len);
    if (res < 0) {
        ESP_LOGE(TAG, "UART send failed");
        return ESP_FAIL;
    } else if (res != len) {
        ESP_LOGE(TAG, "Incorrect send count; expected %zd, got %d", len, res);
        return ESP_FAIL;
    } else {
        ESP_LOGV(TAG, "TX success");
        return ESP_OK;
    }
}

// Receive raw bytes.
static esp_err_t et2_raw_rx(void *data, size_t len) {
    ESP_LOGV(TAG, "RX %zd", len);
    int res = uart_read_bytes(cur_uart, data, len, ET2_TIMEOUT);
    if (res < 0) {
        ESP_LOGE(TAG, "UART recv failed");
        return ESP_FAIL;
    } else if (res == 0) {
        return ESP_ERR_TIMEOUT;
    } else if (res != len) {
        ESP_LOGE(TAG, "Incorrect recv count; expected %zd, got %d", len, res);
        return ESP_FAIL;
    } else {
        ESP_LOGV(TAG, "RX success");
        if (esp_log_level_get(TAG) >= ESP_LOG_DEBUG) {
            for (size_t i = 0; i < len; i++) {
                ESP_LOGV(TAG, "RXD %02X %c", ((uint8_t *)data)[i], ((uint8_t *)data)[i]);
            }
        }
        return ESP_OK;
    }
}

// Change interface speed.
static esp_err_t et2_setfreq(long freq) {
    return uart_set_baudrate(cur_uart, freq);
}

// Send SLIP packet start/end.
static esp_err_t et2_slip_send_startstop() {
    return et2_raw_tx((char[]){0xC0}, 1);
}

// Send SLIP packet data.
static esp_err_t et2_slip_send_data(void const *_data, size_t len) {
    if (!len)
        return ESP_OK;

    char const *data     = _data;
    char const  esc_c0[] = {0xDB, 0xDC};
    char const  esc_db[] = {0xDB, 0xDD};
    esp_err_t   res      = 0;

    while (len) {
        char const *db  = memchr(data, 0xDB, len);
        char const *c0  = memchr(data, 0xC0, len);
        char const *esc = NULL;
        char const *min = data + len - 1;

        // Escape characters.
        if (db && (db < c0 || !c0)) {
            esc = esc_db;
            min = db;
        } else if (c0 && (c0 < db || !db)) {
            esc = esc_c0;
            min = c0;
        } else {
            min = data + len - 1;
        }

        // Send data.
        res = et2_raw_tx(data, min - data);
        if (res)
            return res;
        len  -= min - data + 1;
        data  = min + 1;
        // Send escape characters.
        if (esc) {
            res = et2_raw_tx(esc, 2);
            if (res)
                return res;
        }
    }

    return ESP_OK;
}

// Received a SLIP packet in its entirety.
static esp_err_t et2_slip_recv(void **resp, size_t *resp_len) {
    // Wait for start of packet.
    while (true) {
        uint8_t rxd = 0;
        RETURN_ON_ERR(et2_raw_rx(&rxd, 1));
        if (rxd == 0xC0)
            break;
    }

    size_t   len = 0;
    size_t   cap = 32;
    uint8_t *buf = malloc(cap);
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }

    while (true) {
        uint8_t rxd = 0;
        RETURN_ON_ERR(et2_raw_rx(&rxd, 1));

        if (rxd == 0xC0) {
            // End of message.
            break;

        } else if (rxd == 0xDB) {
            // Handle escape sequences.
            RETURN_ON_ERR(et2_raw_rx(&rxd, 1));
            if (rxd == 0xDC) {
                rxd = 0xC0;
            } else if (rxd == 0xDD) {
                rxd = 0xDB;
            } else {
                ESP_LOGE(TAG, "Invalid escape sequence 0xDB 0x%02" PRIX8, rxd);
                return ESP_ERR_INVALID_RESPONSE;
            }
        }

        // Append character to output.
        if (len >= cap) {
            cap       *= 2;
            void *mem  = realloc(buf, cap);
            if (!mem) {
                free(buf);
                return ESP_ERR_NO_MEM;
            }
            buf = mem;
        }
        buf[len++] = rxd;
    }

    *resp     = buf;
    *resp_len = len;
    return ESP_OK;
}

// Send a command.
static esp_err_t
    et2_send_cmd(et2_cmd_t cmd, void const *param, size_t param_len, void **resp, size_t *resp_len, uint32_t *val) {
    void  *resp_dummy;
    size_t resp_len_dummy;
    bool   ignore_resp = false;
    if (!resp && !resp_len) {
        ignore_resp = true;
        resp        = &resp_dummy;
        resp_len    = &resp_len_dummy;
    } else if (!resp || !resp_len) {
        return ESP_ERR_INVALID_ARG;
    }

    // Send command.
    ESP_LOGD(TAG, "Send cmd 0x%02X param %zd byte%c", cmd, param_len, param_len != 1 ? 's' : 0);
    RETURN_ON_ERR(et2_slip_send_startstop());
    et2_hdr_t header = {0, cmd, param_len, 0};
    RETURN_ON_ERR(et2_slip_send_data(&header, sizeof(header)));
    RETURN_ON_ERR(et2_slip_send_data(param, param_len));
    RETURN_ON_ERR(et2_slip_send_startstop());

    // Wait for max 100 tries for a response.
    for (int try = 0;; try++) {
        ESP_LOGD(TAG, "Recv try %d", try);
        RETURN_ON_ERR(et2_slip_recv(resp, resp_len));
        if (*resp_len < sizeof(et2_hdr_t) || ((et2_hdr_t *)*resp)->resp != 1) {
            continue;
        } else if (((et2_hdr_t *)*resp)->cmd == cmd) {
            break;
        } else if (try >= 100) {
            return ESP_ERR_TIMEOUT;
        }
    }

    // Trim the header off of the response.
    if (val) {
        *val = ((et2_hdr_t *)*resp)->len;
    }
    if (ignore_resp) {
        free(*resp);
    } else if (*resp_len == sizeof(et2_hdr_t)) {
        free(*resp);
        *resp     = NULL;
        *resp_len = 0;
    } else {
        *resp_len -= sizeof(et2_hdr_t);
        memmove(*resp, *resp + sizeof(et2_hdr_t), *resp_len);
    }

    ESP_LOGD(TAG, "Recv OK");
    return ESP_OK;
}



// Write to a range of memory.
esp_err_t et2_mem_write(uint32_t addr, void const *_wdata, uint32_t len) {
    uint8_t const *wdata = _wdata;

    // Compute number of blocks.
    uint32_t blocks = (len + chip_attr->ram_block - 1) / chip_attr->ram_block;

    // Initiate write sequence.
    RETURN_ON_ERR(et2_cmd_mem_begin(len, blocks, chip_attr->ram_block, addr));

    // Send write data in blocks.
    for (uint32_t i = 0; i < blocks; i++) {
        uint32_t chunk_size = len - i * chip_attr->ram_block;
        if (chunk_size > chip_attr->ram_block) {
            chunk_size = chip_attr->ram_block;
        }
        RETURN_ON_ERR(et2_cmd_mem_data(wdata + i * chip_attr->ram_block, chunk_size, i));
    }

    return ESP_OK;
}

// Write to a range of FLASH.
esp_err_t et2_flash_write(uint32_t flash_off, void const *wdata, uint32_t len) {
    return ESP_ERR_NOT_SUPPORTED;
}



// Send MEM_BEGIN command to initiate memory writes.
esp_err_t et2_cmd_mem_begin(uint32_t size, uint32_t blocks, uint32_t blocksize, uint32_t offset) {
    uint32_t payload[] = {size, blocks, blocksize, offset};
    return et2_send_cmd(ET2_CMD_MEM_BEGIN, payload, sizeof(payload), NULL, NULL, NULL);
}

// Send MEM_DATA command to send memory write payload.
esp_err_t et2_cmd_mem_data(void const *data, uint32_t data_len, uint32_t seq) {
    uint32_t header[] = {data_len, seq, 0, 0};
    uint8_t *payload  = malloc(sizeof(header) + data_len);
    if (!payload) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(payload, header, sizeof(header));
    memcpy(payload + sizeof(header), data, data_len);
    esp_err_t res = et2_send_cmd(ET2_CMD_MEM_DATA, payload, sizeof(header) + data_len, NULL, NULL, NULL);
    free(payload);
    return res;
}

// Send MEM_END command to restart into application.
esp_err_t et2_cmd_mem_end(uint32_t entrypoint) {
    uint32_t payload[] = {!entrypoint, entrypoint};
    return et2_send_cmd(ET2_CMD_MEM_END, payload, sizeof(payload), NULL, NULL, NULL);
}

// Send FLASH_BEGIN command to initiate memory writes.
esp_err_t et2_cmd_flash_begin(uint32_t size, uint32_t offset) {
    return ESP_ERR_NOT_SUPPORTED;
}

// Send FLASH_BLOCK command to send memory write payload.
esp_err_t et2_cmd_flash_block(void const *data, uint32_t data_len, uint32_t seq) {
    return ESP_ERR_NOT_SUPPORTED;
}

// Send FLASH_FINISH command to restart into application.
esp_err_t et2_cmd_flash_finish(bool reboot) {
    return ESP_ERR_NOT_SUPPORTED;
}
