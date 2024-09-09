
// SPDX-License-Identifier: MIT

#include "esptoolsquared.h"

#include <esp_log.h>
#include <string.h>

static char const TAG[] = "et2";

#define ET2_TIMEOUT pdMS_TO_TICKS(1000)

#define RETURN_ON_ERR(x)                                                                                               \
    do {                                                                                                               \
        esp_err_t tmp = (x);                                                                                           \
        if (tmp)                                                                                                       \
            return tmp;                                                                                                \
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
uart_port_t cur_uart;



void et2_test() {
    uint32_t chip_id;
    ESP_ERROR_CHECK(et2_detect(&chip_id));
    ESP_LOGI("et2", "ESP32 detected; chip id 0x%08" PRIx32, chip_id);
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

// Detect an ESP32 and, if present, read its chip ID.
// If a pointer is NULL, the property is not read.
esp_err_t et2_detect(uint32_t *chip_id) {
    void  *resp;
    size_t resp_len;
    RETURN_ON_ERR(et2_send_cmd(ET2_CMD_SEC_INFO, NULL, 0, &resp, &resp_len, NULL));
    LEN_CHECK_MIN(resp, resp_len, sizeof(et2_sec_info_t));
    *chip_id = ((et2_sec_info_t *)resp)->chip_id;
    free(resp);
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
    if (!resp || !resp_len) {
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
    if (*resp_len == sizeof(et2_hdr_t)) {
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
