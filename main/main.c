// SPDX-License-Identifier: MIT

#include <stdio.h>
#include "bsp.h"
#include "bsp/es8156.h"
#include "freertos/FreeRTOS.h"
#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/i2s_std.h"
#include "bsp/why2025_coproc.h"
#include "display_test.h"
#include "techdemo.h"
#include "sid.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "fonts/chakrapetchmedium.h"
#include "rvswd.h"
#include <string.h>
#include "pax_codecs.h"

#include "usb/usb_host.h"
#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"
#include "usb/hid_usage_mouse.h"

extern const uint8_t ch32_firmware_start[] asm("_binary_ch32_firmware_bin_start");
extern const uint8_t ch32_firmware_end[] asm("_binary_ch32_firmware_bin_end");

extern const uint8_t splashscreen_start[] asm("_binary_splashscreen_png_start");
extern const uint8_t splashscreen_end[] asm("_binary_splashscreen_png_end");

static char const *TAG = "main";

void display_version() {
    esp_app_desc_t const *app_description = esp_app_get_description();
    printf("BADGE.TEAM %s demo firmware v%s\n", app_description->project_name, app_description->version);
}

bool start_demo = false;

i2s_chan_handle_t i2s_handle = NULL;

uint8_t volume = 100;
uint8_t screen_brightness = 51;
uint8_t keyboard_brightness = 255;

typedef struct _audio_player_cfg {
    uint8_t* buffer;
    size_t   size;
    bool     free_buffer;
} audio_player_cfg_t;

void audio_player_task(void* arg) {
    audio_player_cfg_t* config        = (audio_player_cfg_t*) arg;
    size_t              sample_length = config->size;
    uint8_t*            sample_buffer = config->buffer;

    size_t count;
    size_t position = 0;

    while (1) {
        position = 0;
        while (position < sample_length) {
            size_t length = sample_length - position;
            if (length > 256) length = 256;
            uint8_t buffer[256];
            memcpy(buffer, &sample_buffer[position], length);
            for (size_t l = 0; l < length; l += 2) {
                int16_t* sample = (int16_t*) &buffer[l];
                *sample *= 0.55;
            }
            i2s_channel_write(i2s_handle, (char const *)buffer, length, &count, portMAX_DELAY);
            if (count != length) {
                printf("i2s_write_bytes: count (%d) != length (%d)\n", count, length);
                abort();
            }
            position += length;
        }
    }

    //i2s_zero_dma_buffer(0);  // Fill buffer with silence
    if (config->free_buffer) free(sample_buffer);
    vTaskDelete(NULL);  // Tell FreeRTOS that the task is done
}

extern const uint8_t boot_snd_start[] asm("_binary_boot_snd_start");
extern const uint8_t boot_snd_end[] asm("_binary_boot_snd_end");

audio_player_cfg_t bootsound;

void play_bootsound() {
    TaskHandle_t handle;

    bootsound.buffer      = (uint8_t*) (boot_snd_start);
    bootsound.size        = boot_snd_end - boot_snd_start;
    bootsound.free_buffer = false;

    xTaskCreate(&audio_player_task, "Audio player", 4096, (void*) &bootsound, 10, &handle);
}

esp_err_t i2s_test() {
    // I2S audio
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(0, I2S_ROLE_MASTER);

    esp_err_t res = i2s_new_channel(&chan_cfg, &i2s_handle, NULL);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Initializing I2S channel failed");
        return res;
    }

    i2s_std_config_t i2s_config = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg =
            {
                .mclk = GPIO_NUM_30,
                .bclk = GPIO_NUM_29,
                .ws   = GPIO_NUM_31,
                .dout = GPIO_NUM_28,
                .din  = I2S_GPIO_UNUSED,
                .invert_flags =
                    {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv   = false,
                    },
            },
    };

    res = i2s_channel_init_std_mode(i2s_handle, &i2s_config);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Configuring I2S channel failed");
        return res;
    }

    res = i2s_channel_enable(i2s_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Enabling I2S channel failed");
        return res;
    }

    res = es8156_init();
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Initializing codec failed");
        return res;
    }

    res = es8156_codec_set_voice_volume(volume);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Set volume on codec failed");
        return res;
    }

    res = sid_init(i2s_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Initializing SID emulator failed");
        return res;
    }

    //play_bootsound();

    res = bsp_amplifier_control(true);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Enabling amplifier failed");
        return res;
    }

    return ESP_OK;
}

esp_err_t sd_test(sd_pwr_ctrl_handle_t pwr_ctrl_handle) {
    esp_err_t res;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card;
    const char mount_point[] = "/sdcard";
    ESP_LOGI(TAG, "Initializing SD card");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.pwr_ctrl_handle = pwr_ctrl_handle;

    sdmmc_slot_config_t slot_config = {
        .clk = GPIO_NUM_43,
        .cmd = GPIO_NUM_44,
        .d0 = GPIO_NUM_39,
        .d1 = GPIO_NUM_40,
        .d2 = GPIO_NUM_41,
        .d3 = GPIO_NUM_42,
        .d4 = GPIO_NUM_NC,
        .d5 = GPIO_NUM_NC,
        .d6 = GPIO_NUM_NC,
        .d7 = GPIO_NUM_NC,
        .cd = SDMMC_SLOT_NO_CD,
        .wp = SDMMC_SLOT_NO_WP,
        .width = 4,
        .flags = 0,
    };

    ESP_LOGI(TAG, "Mounting filesystem");
    res = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (res != ESP_OK) {
        if (res == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount SD card filesystem.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the SD card (%s). ", esp_err_to_name(res));
        }
        return res;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    sdmmc_card_print_info(stdout, card);
    return ESP_OK;
}

static esp_err_t print_sdio_cis_information(sdmmc_card_t* card)
{
    const size_t cis_buffer_size = 256;
    uint8_t cis_buffer[cis_buffer_size];
    size_t cis_data_len = 1024; //specify maximum searching range to avoid infinite loop
    esp_err_t ret = ESP_OK;

    ret = sdmmc_io_get_cis_data(card, cis_buffer, cis_buffer_size, &cis_data_len);
    if (ret == ESP_ERR_INVALID_SIZE) {
        int temp_buf_size = cis_data_len;
        uint8_t* temp_buf = malloc(temp_buf_size);
        assert(temp_buf);

        ESP_LOGW(TAG, "CIS data longer than expected, temporary buffer allocated.");
        ret = sdmmc_io_get_cis_data(card, temp_buf, temp_buf_size, &cis_data_len);
        ESP_ERROR_CHECK(ret);

        sdmmc_io_print_cis_info(temp_buf, cis_data_len, NULL);

        free(temp_buf);
    } else if (ret == ESP_OK) {
        sdmmc_io_print_cis_info(cis_buffer, cis_data_len, NULL);
    } else {
        //including ESP_ERR_NOT_FOUND
        ESP_LOGE(TAG, "failed to get the entire CIS data.");
        abort();
    }
    return ESP_OK;
}

esp_err_t sdio_test() {
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_4BIT;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    host.flags |= SDMMC_HOST_FLAG_ALLOC_ALIGNED_BUF;

    sdmmc_slot_config_t slot_config = {
        .clk = GPIO_NUM_17,
        .cmd = GPIO_NUM_16,
        .d0 = GPIO_NUM_18,
        .d1 = GPIO_NUM_19,
        .d2 = GPIO_NUM_20,
        .d3 = GPIO_NUM_21,
        .d4 = GPIO_NUM_NC,
        .d5 = GPIO_NUM_NC,
        .d6 = GPIO_NUM_NC,
        .d7 = GPIO_NUM_NC,
        .cd = SDMMC_SLOT_NO_CD,
        .wp = SDMMC_SLOT_NO_WP,
        .width = 4,
        .flags = 0,
    };

    esp_err_t res = sdmmc_host_init();
    if (res != ESP_OK) return res;

    res = sdmmc_host_init_slot(SDMMC_HOST_SLOT_1, &slot_config);
    if (res != ESP_OK) return res;

    sdmmc_card_t *card = (sdmmc_card_t *)malloc(sizeof(sdmmc_card_t));

    for (;;) {
        if (sdmmc_card_init(&host, card) == ESP_OK) {
            break;
        }
        ESP_LOGW(TAG, "slave init failed, retry...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    sdmmc_card_print_info(stdout, card);

    print_sdio_cis_information(card);

    return res;
}

// USB stuff

QueueHandle_t app_event_queue = NULL;

/**
 * @brief APP event group
 *
 * Application logic can be different. There is a one among other ways to distingiush the
 * event by application event group.
 * In this example we have two event groups:
 * APP_EVENT            - General event, which is APP_QUIT_PIN press event (Generally, it is IO0).
 * APP_EVENT_HID_HOST   - HID Host Driver event, such as device connection/disconnection or input report.
 */
typedef enum {
    APP_EVENT = 0,
    APP_EVENT_HID_HOST
} app_event_group_t;

/**
 * @brief APP event queue
 *
 * This event is used for delivering the HID Host event from callback to a task.
 */
typedef struct {
    app_event_group_t event_group;
    /* HID Host - Device related info */
    struct {
        hid_host_device_handle_t handle;
        hid_host_driver_event_t event;
        void *arg;
    } hid_host_device;
} app_event_queue_t;

/**
 * @brief HID Protocol string names
 */
static const char *hid_proto_name_str[] = {
    "NONE",
    "KEYBOARD",
    "MOUSE"
};

/**
 * @brief Key event
 */
typedef struct {
    enum key_state {
        KEY_STATE_PRESSED = 0x00,
        KEY_STATE_RELEASED = 0x01
    } state;
    uint8_t modifier;
    uint8_t key_code;
} key_event_t;

/* Main char symbol for ENTER key */
#define KEYBOARD_ENTER_MAIN_CHAR    '\r'
/* When set to 1 pressing ENTER will be extending with LineFeed during serial debug output */
#define KEYBOARD_ENTER_LF_EXTEND    1

/**
 * @brief Scancode to ascii table
 */
const uint8_t keycode2ascii [57][2] = {
    {0, 0}, /* HID_KEY_NO_PRESS        */
    {0, 0}, /* HID_KEY_ROLLOVER        */
    {0, 0}, /* HID_KEY_POST_FAIL       */
    {0, 0}, /* HID_KEY_ERROR_UNDEFINED */
    {'a', 'A'}, /* HID_KEY_A               */
    {'b', 'B'}, /* HID_KEY_B               */
    {'c', 'C'}, /* HID_KEY_C               */
    {'d', 'D'}, /* HID_KEY_D               */
    {'e', 'E'}, /* HID_KEY_E               */
    {'f', 'F'}, /* HID_KEY_F               */
    {'g', 'G'}, /* HID_KEY_G               */
    {'h', 'H'}, /* HID_KEY_H               */
    {'i', 'I'}, /* HID_KEY_I               */
    {'j', 'J'}, /* HID_KEY_J               */
    {'k', 'K'}, /* HID_KEY_K               */
    {'l', 'L'}, /* HID_KEY_L               */
    {'m', 'M'}, /* HID_KEY_M               */
    {'n', 'N'}, /* HID_KEY_N               */
    {'o', 'O'}, /* HID_KEY_O               */
    {'p', 'P'}, /* HID_KEY_P               */
    {'q', 'Q'}, /* HID_KEY_Q               */
    {'r', 'R'}, /* HID_KEY_R               */
    {'s', 'S'}, /* HID_KEY_S               */
    {'t', 'T'}, /* HID_KEY_T               */
    {'u', 'U'}, /* HID_KEY_U               */
    {'v', 'V'}, /* HID_KEY_V               */
    {'w', 'W'}, /* HID_KEY_W               */
    {'x', 'X'}, /* HID_KEY_X               */
    {'y', 'Y'}, /* HID_KEY_Y               */
    {'z', 'Z'}, /* HID_KEY_Z               */
    {'1', '!'}, /* HID_KEY_1               */
    {'2', '@'}, /* HID_KEY_2               */
    {'3', '#'}, /* HID_KEY_3               */
    {'4', '$'}, /* HID_KEY_4               */
    {'5', '%'}, /* HID_KEY_5               */
    {'6', '^'}, /* HID_KEY_6               */
    {'7', '&'}, /* HID_KEY_7               */
    {'8', '*'}, /* HID_KEY_8               */
    {'9', '('}, /* HID_KEY_9               */
    {'0', ')'}, /* HID_KEY_0               */
    {KEYBOARD_ENTER_MAIN_CHAR, KEYBOARD_ENTER_MAIN_CHAR}, /* HID_KEY_ENTER           */
    {0, 0}, /* HID_KEY_ESC             */
    {'\b', 0}, /* HID_KEY_DEL             */
    {0, 0}, /* HID_KEY_TAB             */
    {' ', ' '}, /* HID_KEY_SPACE           */
    {'-', '_'}, /* HID_KEY_MINUS           */
    {'=', '+'}, /* HID_KEY_EQUAL           */
    {'[', '{'}, /* HID_KEY_OPEN_BRACKET    */
    {']', '}'}, /* HID_KEY_CLOSE_BRACKET   */
    {'\\', '|'}, /* HID_KEY_BACK_SLASH      */
    {'\\', '|'}, /* HID_KEY_SHARP           */  // HOTFIX: for NonUS Keyboards repeat HID_KEY_BACK_SLASH
    {';', ':'}, /* HID_KEY_COLON           */
    {'\'', '"'}, /* HID_KEY_QUOTE           */
    {'`', '~'}, /* HID_KEY_TILDE           */
    {',', '<'}, /* HID_KEY_LESS            */
    {'.', '>'}, /* HID_KEY_GREATER         */
    {'/', '?'} /* HID_KEY_SLASH           */
};

/**
 * @brief Makes new line depending on report output protocol type
 *
 * @param[in] proto Current protocol to output
 */
static void hid_print_new_device_report_header(hid_protocol_t proto)
{
    static hid_protocol_t prev_proto_output = -1;

    if (prev_proto_output != proto) {
        prev_proto_output = proto;
        printf("\r\n");
        if (proto == HID_PROTOCOL_MOUSE) {
            printf("Mouse\r\n");
        } else if (proto == HID_PROTOCOL_KEYBOARD) {
            printf("Keyboard\r\n");
        } else {
            printf("Generic\r\n");
        }
        fflush(stdout);
    }
}

/**
 * @brief HID Keyboard modifier verification for capitalization application (right or left shift)
 *
 * @param[in] modifier
 * @return true  Modifier was pressed (left or right shift)
 * @return false Modifier was not pressed (left or right shift)
 *
 */
static inline bool hid_keyboard_is_modifier_shift(uint8_t modifier)
{
    if (((modifier & HID_LEFT_SHIFT) == HID_LEFT_SHIFT) ||
            ((modifier & HID_RIGHT_SHIFT) == HID_RIGHT_SHIFT)) {
        return true;
    }
    return false;
}

/**
 * @brief HID Keyboard get char symbol from key code
 *
 * @param[in] modifier  Keyboard modifier data
 * @param[in] key_code  Keyboard key code
 * @param[in] key_char  Pointer to key char data
 *
 * @return true  Key scancode converted successfully
 * @return false Key scancode unknown
 */
static inline bool hid_keyboard_get_char(uint8_t modifier,
                                         uint8_t key_code,
                                         unsigned char *key_char)
{
    uint8_t mod = (hid_keyboard_is_modifier_shift(modifier)) ? 1 : 0;

    if ((key_code >= HID_KEY_A) && (key_code <= HID_KEY_SLASH)) {
        *key_char = keycode2ascii[key_code][mod];
    } else {
        // All other key pressed
        return false;
    }

    return true;
}

/**
 * @brief HID Keyboard print char symbol
 *
 * @param[in] key_char  Keyboard char to stdout
 */
static inline void hid_keyboard_print_char(unsigned int key_char)
{
    if (!!key_char) {
        putchar(key_char);
#if (KEYBOARD_ENTER_LF_EXTEND)
        if (KEYBOARD_ENTER_MAIN_CHAR == key_char) {
            putchar('\n');
        }
#endif // KEYBOARD_ENTER_LF_EXTEND
        fflush(stdout);
    }
}

/**
 * @brief Key Event. Key event with the key code, state and modifier.
 *
 * @param[in] key_event Pointer to Key Event structure
 *
 */
static void key_event_callback(key_event_t *key_event)
{
    unsigned char key_char;

    hid_print_new_device_report_header(HID_PROTOCOL_KEYBOARD);

    if (KEY_STATE_PRESSED == key_event->state) {
        if (hid_keyboard_get_char(key_event->modifier,
                                  key_event->key_code, &key_char)) {

            hid_keyboard_print_char(key_char);

        }
    }
}

/**
 * @brief Key buffer scan code search.
 *
 * @param[in] src       Pointer to source buffer where to search
 * @param[in] key       Key scancode to search
 * @param[in] length    Size of the source buffer
 */
static inline bool key_found(const uint8_t *const src,
                             uint8_t key,
                             unsigned int length)
{
    for (unsigned int i = 0; i < length; i++) {
        if (src[i] == key) {
            return true;
        }
    }
    return false;
}

/**
 * @brief USB HID Host Keyboard Interface report callback handler
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_keyboard_report_callback(const uint8_t *const data, const int length)
{
    hid_keyboard_input_report_boot_t *kb_report = (hid_keyboard_input_report_boot_t *)data;

    if (length < sizeof(hid_keyboard_input_report_boot_t)) {
        return;
    }

    static uint8_t prev_keys[HID_KEYBOARD_KEY_MAX] = { 0 };
    key_event_t key_event;

    for (int i = 0; i < HID_KEYBOARD_KEY_MAX; i++) {

        // key has been released verification
        if (prev_keys[i] > HID_KEY_ERROR_UNDEFINED &&
                !key_found(kb_report->key, prev_keys[i], HID_KEYBOARD_KEY_MAX)) {
            key_event.key_code = prev_keys[i];
            key_event.modifier = 0;
            key_event.state = KEY_STATE_RELEASED;
            key_event_callback(&key_event);
        }

        // key has been pressed verification
        if (kb_report->key[i] > HID_KEY_ERROR_UNDEFINED &&
                !key_found(prev_keys, kb_report->key[i], HID_KEYBOARD_KEY_MAX)) {
            key_event.key_code = kb_report->key[i];
            key_event.modifier = kb_report->modifier.val;
            key_event.state = KEY_STATE_PRESSED;
            key_event_callback(&key_event);
        }
    }

    memcpy(prev_keys, &kb_report->key, HID_KEYBOARD_KEY_MAX);
}

/**
 * @brief USB HID Host Mouse Interface report callback handler
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_mouse_report_callback(const uint8_t *const data, const int length)
{
    hid_mouse_input_report_boot_t *mouse_report = (hid_mouse_input_report_boot_t *)data;

    if (length < sizeof(hid_mouse_input_report_boot_t)) {
        return;
    }

    static int x_pos = 0;
    static int y_pos = 0;

    // Calculate absolute position from displacement
    x_pos += mouse_report->x_displacement;
    y_pos += mouse_report->y_displacement;

    hid_print_new_device_report_header(HID_PROTOCOL_MOUSE);

    printf("X: %06d\tY: %06d\t|%c|%c|\r",
           x_pos, y_pos,
           (mouse_report->buttons.button1 ? 'o' : ' '),
           (mouse_report->buttons.button2 ? 'o' : ' '));
    fflush(stdout);
}

/**
 * @brief USB HID Host Generic Interface report callback handler
 *
 * 'generic' means anything else than mouse or keyboard
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_generic_report_callback(const uint8_t *const data, const int length)
{
    hid_print_new_device_report_header(HID_PROTOCOL_NONE);
    for (int i = 0; i < length; i++) {
        printf("%02X", data[i]);
    }
    putchar('\r');
}

/**
 * @brief USB HID Host interface callback
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host interface event
 * @param[in] arg                Pointer to arguments, does not used
 */
void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                                 const hid_host_interface_event_t event,
                                 void *arg)
{
    uint8_t data[64] = { 0 };
    size_t data_length = 0;
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(hid_device_handle,
                                                                  data,
                                                                  64,
                                                                  &data_length));

        if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
            if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                hid_host_keyboard_report_callback(data, data_length);
            } else if (HID_PROTOCOL_MOUSE == dev_params.proto) {
                hid_host_mouse_report_callback(data, data_length);
            }
        } else {
            hid_host_generic_report_callback(data, data_length);
        }

        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HID Device, protocol '%s' DISCONNECTED",
                 hid_proto_name_str[dev_params.proto]);
        ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        ESP_LOGI(TAG, "HID Device, protocol '%s' TRANSFER_ERROR",
                 hid_proto_name_str[dev_params.proto]);
        break;
    default:
        ESP_LOGE(TAG, "HID Device, protocol '%s' Unhandled event",
                 hid_proto_name_str[dev_params.proto]);
        break;
    }
}

/**
 * @brief USB HID Host Device event
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host Device event
 * @param[in] arg                Pointer to arguments, does not used
 */
void hid_host_device_event(hid_host_device_handle_t hid_device_handle,
                           const hid_host_driver_event_t event,
                           void *arg)
{
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
    case HID_HOST_DRIVER_EVENT_CONNECTED:
        ESP_LOGI(TAG, "HID Device, protocol '%s' CONNECTED",
                 hid_proto_name_str[dev_params.proto]);

        const hid_host_device_config_t dev_config = {
            .callback = hid_host_interface_callback,
            .callback_arg = NULL
        };

        ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
        if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
            ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT));
            if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
            }
        }
        ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
        break;
    default:
        break;
    }
}

/**
 * @brief Start USB Host install and handle common USB host library events while app pin not low
 *
 * @param[in] arg  Not used
 */
static void usb_lib_task(void *arg)
{
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    ESP_ERROR_CHECK(usb_host_install(&host_config));
    xTaskNotifyGive(arg);

    while (true) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        // In this example, there is only one client registered
        // So, once we deregister the client, this call must succeed with ESP_OK
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
            break;
        }
    }

    ESP_LOGI(TAG, "USB shutdown");
    // Clean up USB Host
    vTaskDelay(10); // Short delay to allow clients clean-up
    ESP_ERROR_CHECK(usb_host_uninstall());
    vTaskDelete(NULL);
}

/**
 * @brief BOOT button pressed callback
 *
 * Signal application to exit the HID Host task
 *
 * @param[in] arg Unused
 */
static void gpio_isr_cb(void *arg)
{
    BaseType_t xTaskWoken = pdFALSE;
    const app_event_queue_t evt_queue = {
        .event_group = APP_EVENT,
    };

    if (app_event_queue) {
        xQueueSendFromISR(app_event_queue, &evt_queue, &xTaskWoken);
    }

    if (xTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief HID Host Device callback
 *
 * Puts new HID Device event to the queue
 *
 * @param[in] hid_device_handle HID Device handle
 * @param[in] event             HID Device event
 * @param[in] arg               Not used
 */
void hid_host_device_callback(hid_host_device_handle_t hid_device_handle,
                              const hid_host_driver_event_t event,
                              void *arg)
{
    const app_event_queue_t evt_queue = {
        .event_group = APP_EVENT_HID_HOST,
        // HID Host Device related info
        .hid_host_device.handle = hid_device_handle,
        .hid_host_device.event = event,
        .hid_host_device.arg = arg
    };

    if (app_event_queue) {
        xQueueSend(app_event_queue, &evt_queue, 0);
    }
}

esp_err_t usb_host_test() {
    app_event_queue_t evt_queue;

    BaseType_t task_created = xTaskCreatePinnedToCore(usb_lib_task,
                                           "usb_events",
                                           4096,
                                           xTaskGetCurrentTaskHandle(),
                                           2, NULL, 0);
    
    if (task_created != pdTRUE) {
        return ESP_FAIL;
    }

    ulTaskNotifyTake(false, 1000);

    const hid_host_driver_config_t hid_host_driver_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_host_device_callback,
        .callback_arg = NULL
    };

    ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));

    // Create queue
    app_event_queue = xQueueCreate(10, sizeof(app_event_queue_t));

    ESP_LOGI(TAG, "Waiting for HID Device to be connected");

    while (1) {
        // Wait queue
        if (xQueueReceive(app_event_queue, &evt_queue, portMAX_DELAY)) {
            if (APP_EVENT == evt_queue.event_group) {
                // User pressed button
                usb_host_lib_info_t lib_info;
                ESP_ERROR_CHECK(usb_host_lib_info(&lib_info));
                if (lib_info.num_devices == 0) {
                    // End while cycle
                    break;
                } else {
                    ESP_LOGW(TAG, "To shutdown example, remove all USB devices and press button again.");
                    // Keep polling
                }
            }

            if (APP_EVENT_HID_HOST ==  evt_queue.event_group) {
                hid_host_device_event(evt_queue.hid_host_device.handle,
                                      evt_queue.hid_host_device.event,
                                      evt_queue.hid_host_device.arg);
            }
        }
    }

    ESP_LOGI(TAG, "HID Driver uninstall");
    ESP_ERROR_CHECK(hid_host_uninstall());
    xQueueReset(app_event_queue);
    vQueueDelete(app_event_queue);

    return ESP_OK;
}

// Colors
pax_col_t color_background = 0xFFEEEAEE;
pax_col_t color_primary    = 0xFF01BC99;
pax_col_t color_secondary  = 0xFFFFCF53;
pax_col_t color_tertiary   = 0xFFFF017F;
pax_col_t color_quaternary = 0xFF340132;

void draw_test() {
    pax_buf_t* fb = display_test_get_buf();

    pax_background(fb, color_background);

    // Footer test
    int footer_height      = 32;
    int footer_vmarign     = 7;
    int footer_hmarign     = 20;
    int footer_text_height = 16;
    int footer_text_hmarign = 20;
    pax_font_t* footer_text_font = &chakrapetchmedium;

    int footer_box_height = footer_height + (footer_vmarign * 2);
    pax_draw_line(fb, color_quaternary, footer_hmarign, pax_buf_get_height(fb) - footer_box_height, pax_buf_get_width(fb) - footer_hmarign, pax_buf_get_height(fb) - footer_box_height);
    pax_right_text(fb, color_quaternary, footer_text_font, footer_text_height, pax_buf_get_width(fb) - footer_hmarign - footer_text_hmarign, pax_buf_get_height(fb) - footer_box_height + (footer_box_height - footer_text_height) / 2, "↑ / ↓ / ← / → Navigate   🅰 Start");


    // Font test
    /*pax_draw_text(fb, color_quaternary, pax_font_sky, footer_text_height, footer_hmarign + footer_text_hmarign,  (footer_vmarign + footer_text_height) * 1, "Test test test 🅰🅱🅷🅼🆂🅴↑↓←→⤓");
    pax_draw_text(fb, color_quaternary, pax_font_sky_mono, footer_text_height, footer_hmarign + footer_text_hmarign,  (footer_vmarign + footer_text_height) * 2, "Test test test 🅰🅱🅷🅼🆂🅴↑↓←→⤓");
    pax_draw_text(fb, color_quaternary, pax_font_marker, footer_text_height, footer_hmarign + footer_text_hmarign,  (footer_vmarign + footer_text_height) * 3, "Test test test 🅰🅱🅷🅼🆂🅴↑↓←→⤓");
    pax_draw_text(fb, color_quaternary, pax_font_saira_condensed, footer_text_height, footer_hmarign + footer_text_hmarign,  (footer_vmarign + footer_text_height) * 4, "Test test test 🅰🅱🅷🅼🆂🅴↑↓←→⤓");
    pax_draw_text(fb, color_quaternary, pax_font_saira_regular, footer_text_height, footer_hmarign + footer_text_hmarign,  (footer_vmarign + footer_text_height) * 5, "Test test test 🅰🅱🅷🅼🆂🅴↑↓←→⤓");
    pax_draw_text(fb, color_quaternary, &chakrapetchmedium, footer_text_height, footer_hmarign + footer_text_hmarign,  (footer_vmarign + footer_text_height) * 6, "Test test test 🅰🅱🅷🅼🆂🅴↑↓←→⤓");*/

    int circle_count = 6;
    int circle_spacing = 68;
    int circle_offset = (pax_buf_get_width(fb) - circle_spacing * (circle_count - 1)) / 2;

    for (int i = 0; i < circle_count; i++) {
        pax_draw_circle(fb, 0xFFFFFFFF, circle_offset + circle_spacing * i, 375, 28);
        pax_outline_circle(fb, 0xFFE4E4E4, circle_offset + circle_spacing * i, 375, 28);
    }

    int square_count = 5;
    int square_voffset = 116;
    int square_hoffset = 62;
    int square_size = 170;
    int square_selected = 0;
    int square_spacing = 1;
    int square_marign = 2;

    pax_col_t square_colors[] = {
        0xFFFFFFFF,
        color_secondary,
        color_tertiary,
        color_quaternary,
        color_primary,
    };

    for (int i = 0; i < square_count; i++) {
        if (i == square_selected) {
            pax_draw_rect(fb, color_primary, square_hoffset + (square_size + square_spacing) * i, square_voffset, square_size, square_size);
        }
        pax_draw_rect(fb, square_colors[i], square_hoffset + (square_size + square_spacing) * i + square_marign, square_voffset + square_marign, square_size - square_marign * 2, square_size - square_marign * 2);
    }

    int title_text_voffset = square_voffset - 32;
    int title_text_hoffset = square_hoffset - 2;
    int title_text_height = 20;
    pax_font_t* title_text_font = &chakrapetchmedium;

    pax_draw_text(fb, color_primary, title_text_font, title_text_height, title_text_hoffset, title_text_voffset, "Example Application Title");

    pax_right_text(fb, color_quaternary, title_text_font, 20, pax_buf_get_width(fb) - 40, 32, "23:37 :D");

    pax_push_2d(fb);
    pax_apply_2d(fb, matrix_2d_translate(pax_buf_get_width(fb) / 2, pax_buf_get_height(fb) / 2));
    pax_apply_2d(fb, matrix_2d_rotate(180 * 0.01745329252));
    pax_center_text(fb, 0xFF000000, pax_font_marker, 100, 0, -150, "Renze");
    pax_pop_2d(fb);

    pax_buf_t splash;
    pax_decode_png_buf(&splash, splashscreen_start, splashscreen_end - splashscreen_start, PAX_BUF_16_565RGB, 0);

    pax_draw_image(fb, &splash, 0, 0);

    display_test_flush();
}

void draw_key(bsp_input_t input) {
    pax_buf_t* fb = display_test_get_buf();
    pax_background(fb, color_background);
    pax_push_2d(fb);
    pax_apply_2d(fb, matrix_2d_translate(pax_buf_get_width(fb) / 2, pax_buf_get_height(fb) / 2));

    char text[128];
    sprintf(text, "Pressed key %04x", input);

    pax_center_text(fb, 0xFF000000, &chakrapetchmedium, 50, 0, -25, text);
    pax_pop_2d(fb);
    display_test_flush();
}

void draw_text(char* text) {
    pax_buf_t* fb = display_test_get_buf();
    pax_background(fb, color_background);
    pax_push_2d(fb);
    pax_apply_2d(fb, matrix_2d_translate(pax_buf_get_width(fb) / 2, pax_buf_get_height(fb) / 2));
    pax_center_text(fb, 0xFF000000, &chakrapetchmedium, 50, 0, -25, text);
    pax_pop_2d(fb);
    display_test_flush();
}

int current_key = -1;
static SemaphoreHandle_t demo_semaphore;

void blink_keyboard() {
    for (uint8_t i = 0; i < 3; i++) {
        ch32_set_keyboard_backlight(255);
        vTaskDelay(pdMS_TO_TICKS(100));
        ch32_set_keyboard_backlight(0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

#define CH32_REG_DEBUG_DATA0        0x04  // Data register 0, can be used for temporary storage of data
#define CH32_REG_DEBUG_DATA1        0x05  // Data register 1, can be used for temporary storage of data
#define CH32_REG_DEBUG_DMCONTROL    0x10  // Debug module control register
#define CH32_REG_DEBUG_DMSTATUS     0x11  // Debug module status register
#define CH32_REG_DEBUG_HARTINFO     0x12  // Microprocessor status register
#define CH32_REG_DEBUG_ABSTRACTCS   0x16  // Abstract command status register
#define CH32_REG_DEBUG_COMMAND      0x17  // Astract command register
#define CH32_REG_DEBUG_ABSTRACTAUTO 0x18  // Abstract command auto-executtion
#define CH32_REG_DEBUG_PROGBUF0     0x20  // Instruction cache register 0
#define CH32_REG_DEBUG_PROGBUF1     0x21  // Instruction cache register 1
#define CH32_REG_DEBUG_PROGBUF2     0x22  // Instruction cache register 2
#define CH32_REG_DEBUG_PROGBUF3     0x23  // Instruction cache register 3
#define CH32_REG_DEBUG_PROGBUF4     0x24  // Instruction cache register 4
#define CH32_REG_DEBUG_PROGBUF5     0x25  // Instruction cache register 5
#define CH32_REG_DEBUG_PROGBUF6     0x26  // Instruction cache register 6
#define CH32_REG_DEBUG_PROGBUF7     0x27  // Instruction cache register 7
#define CH32_REG_DEBUG_HALTSUM0     0x40  // Halt status register
#define CH32_REG_DEBUG_CPBR         0x7C  // Capability register
#define CH32_REG_DEBUG_CFGR         0x7D  // Configuration register
#define CH32_REG_DEBUG_SHDWCFGR     0x7E  // Shadow configuration register

#define CH32_REGS_CSR 0x0000  // Offsets for accessing CSRs.
#define CH32_REGS_GPR 0x1000  // Offsets for accessing general-purpose (x)registers.

#define CH32_CFGR_KEY   0x5aa50000
#define CH32_CFGR_OUTEN (1 << 10)

// The start of CH32 CODE FLASH region.
#define CH32_CODE_BEGIN 0x08000000
// the end of the CH32 CODE FLASH region.
#define CH32_CODE_END   0x08004000

// FLASH status register.
#define CH32_FLASH_STATR 0x4002200C
// FLASH configuration register.
#define CH32_FLASH_CTLR  0x40022010
// FLASH address register.
#define CH32_FLASH_ADDR  0x40022014

// FLASH is busy writing or erasing.
#define CH32_FLASH_STATR_BUSY (1 << 0)
// FLASH is busy writing
#define CH32_FLASH_STATR_WRBUSY (1 << 1)
// FLASH is finished with the operation.
#define CH32_FLASH_STATR_EOP  (1 << 5)

// Perform standard programming operation.
#define CH32_FLASH_CTLR_PG      (1 << 0)
// Perform 1K sector erase.
#define CH32_FLASH_CTLR_PER     (1 << 1)
// Perform full FLASH erase.
#define CH32_FLASH_CTLR_MER     (1 << 2)
// Perform user-selected word program.
#define CH32_FLASH_CTLR_OBG     (1 << 4)
// Perform user-selected word erasure.
#define CH32_FLASH_CTLR_OBER    (1 << 5)
// Start an erase operation.
#define CH32_FLASH_CTLR_STRT    (1 << 6)
// Lock the FLASH.
#define CH32_FLASH_CTLR_LOCK    (1 << 7)
// Start a fast page programming operation (256 bytes).
#define CH32_FLASH_CTLR_FTPG    (1 << 16)
// Start a fast page erase operation (256 bytes).
#define CH32_FLASH_CTLR_FTER    (1 << 17)
// Start a page programming operation (256 bytes).
#define CH32_FLASH_CTLR_PGSTRT  (1 << 21)

const uint8_t ch32_readmem[] = {
  0x88, 0x41, 0x02, 0x90
};

const uint8_t ch32_writemem[] = {
  0x88, 0xc1, 0x02, 0x90
};

rvswd_result_t ch32_halt_microprocessor(rvswd_handle_t* handle) {
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Make the debug module work properly
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Initiate a halt request

    // Get the debug module status information, check rdata[9:8], if the value is 0b11,
    // it means the processor enters the halt state normally. Otherwise try again.
    uint8_t timeout = 5;
    while (1) {
        uint32_t value;
        rvswd_read(handle, CH32_REG_DEBUG_DMSTATUS, &value);
        if (((value >> 8) & 0b11) == 0b11) {  // Check that processor has entered halted state
            break;
        }
        if (timeout == 0) {
            ESP_LOGE(TAG, "Failed to halt microprocessor, DMSTATUS=%" PRIx32, value);
            return false;
        }
        timeout--;
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000001);  // Clear the halt request
    ESP_LOGI(TAG, "Microprocessor halted");
    return RVSWD_OK;
}

rvswd_result_t ch32_resume_microprocessor(rvswd_handle_t* handle) {
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Make the debug module work properly
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Initiate a halt request
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000001);  // Clear the halt request
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x40000001);  // Initiate a resume request

    // Get the debug module status information, check rdata[17:16],
    // if the value is 0b11, it means the processor has recovered.
    uint8_t timeout = 5;
    while (1) {
        uint32_t value;
        rvswd_read(handle, CH32_REG_DEBUG_DMSTATUS, &value);
        if ((((value >> 10) & 0b11) == 0b11)) {
            break;
        }
        if (timeout == 0) {
            ESP_LOGE(TAG, "Failed to resume microprocessor, DMSTATUS=%" PRIx32, value);
            return RVSWD_FAIL;
        }
        timeout--;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return RVSWD_OK;
}

rvswd_result_t ch32_reset_microprocessor_and_run(rvswd_handle_t* handle) {
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Make the debug module work properly
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Initiate a halt request
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000001);  // Clear the halt request
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000003);  // Initiate a core reset request

    uint8_t timeout = 5;
    while (1) {
        uint32_t value;
        rvswd_read(handle, CH32_REG_DEBUG_DMSTATUS, &value);
        if (((value >> 18) & 0b11) == 0b11) {  // Check that processor has been reset
            break;
        }
        if (timeout == 0) {
            ESP_LOGE(TAG, "Failed to reset microprocessor");
            return RVSWD_FAIL;
        }
        timeout--;
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000001);  // Clear the core reset request
    vTaskDelay(pdMS_TO_TICKS(10));
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x10000001);  // Clear the core reset status signal
    vTaskDelay(pdMS_TO_TICKS(10));
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000001);  // Clear the core reset status signal clear request
    vTaskDelay(pdMS_TO_TICKS(10));

    return RVSWD_OK;
}

bool ch32_write_cpu_reg(rvswd_handle_t* handle, uint16_t regno, uint32_t value) {
    uint32_t command = regno         // Register to access.
                       | (1 << 16)   // Write access.
                       | (1 << 17)   // Perform transfer.
                       | (2 << 20)   // 32-bit register access.
                       | (0 << 24);  // Access register command.

    rvswd_write(handle, CH32_REG_DEBUG_DATA0, value);
    rvswd_write(handle, CH32_REG_DEBUG_COMMAND, command);
    return true;
}

bool ch32_read_cpu_reg(rvswd_handle_t* handle, uint16_t regno, uint32_t* value_out) {
    uint32_t command = regno         // Register to access.
                       | (0 << 16)   // Read access.
                       | (1 << 17)   // Perform transfer.
                       | (2 << 20)   // 32-bit register access.
                       | (0 << 24);  // Access register command.

    rvswd_write(handle, CH32_REG_DEBUG_COMMAND, command);
    rvswd_read(handle, CH32_REG_DEBUG_DATA0, value_out);
    return true;
}

bool ch32_run_debug_code(rvswd_handle_t* handle, const void* code, size_t code_size) {
    if (code_size > 8 * 4) {
        ESP_LOGE(TAG, "Debug program is too long (%zd/%zd)", code_size, (size_t)8 * 4);
        return false;
    } else if (code_size & 1) {
        ESP_LOGE(TAG, "Debug program size must be a multiple of 2 (%zd)", code_size);
        return false;
    }

    // Copy into program buffer.
    uint32_t tmp[8] = {0};
    memcpy(tmp, code, code_size);
    for (size_t i = 0; i < 8; i++) {
        rvswd_write(handle, CH32_REG_DEBUG_PROGBUF0 + i, tmp[i]);
    }

    // Run program buffer.
    uint32_t command = (0 << 17)     // Do not perform transfer.
                       | (1 << 18)   // Run program buffer afterwards.
                       | (2 << 20)   // 32-bit register access.
                       | (0 << 24);  // Access register command.
    rvswd_write(handle, CH32_REG_DEBUG_COMMAND, command);

    return true;
}

bool ch32_read_memory_word(rvswd_handle_t* handle, uint32_t address, uint32_t* value_out) {
    ch32_write_cpu_reg(handle, CH32_REGS_GPR + 11, address);
    ch32_run_debug_code(handle, ch32_readmem, sizeof(ch32_readmem));
    ch32_read_cpu_reg(handle, CH32_REGS_GPR + 10, value_out);
    return true;
}

bool ch32_write_memory_word(rvswd_handle_t* handle, uint32_t address, uint32_t value) {
    ch32_write_cpu_reg(handle, CH32_REGS_GPR + 10, value);
    ch32_write_cpu_reg(handle, CH32_REGS_GPR + 11, address);
    ch32_run_debug_code(handle, ch32_writemem, sizeof(ch32_writemem));
    return true;
}

// Wait for the FLASH chip to finish its current operation.
static void ch32_wait_flash(rvswd_handle_t* handle) {
    uint32_t value = 0;
    ch32_read_memory_word(handle, CH32_FLASH_STATR, &value);

    while (value & CH32_FLASH_STATR_BUSY) {
        vTaskDelay(1);
        ch32_read_memory_word(handle, CH32_FLASH_STATR, &value);
    }
}

static void ch32_wait_flash_write(rvswd_handle_t* handle) {
    uint32_t value = 0;
    ch32_read_memory_word(handle, CH32_FLASH_STATR, &value);
    while (value & CH32_FLASH_STATR_WRBUSY) {
        ch32_read_memory_word(handle, CH32_FLASH_STATR, &value);
    }
}

// Unlock the FLASH if not already unlocked.
bool ch32_unlock_flash(rvswd_handle_t* handle) {
    uint32_t ctlr;
    ch32_read_memory_word(handle, CH32_FLASH_CTLR, &ctlr);

    printf("CTLR before unlock: %08" PRIx32 "\r\n", ctlr);

    /*if (!(ctlr & 0x8080)) {
        // FLASH already unlocked.
        printf("Already unlocked\r\n");
        return true;
    }*/

    // Enter the unlock keys.
    ch32_write_memory_word(handle, 0x40022004, 0x45670123);
    ch32_write_memory_word(handle, 0x40022004, 0xCDEF89AB);
    ch32_write_memory_word(handle, 0x40022008, 0x45670123);
    ch32_write_memory_word(handle, 0x40022008, 0xCDEF89AB);
    ch32_write_memory_word(handle, 0x40022024, 0x45670123);
    ch32_write_memory_word(handle, 0x40022024, 0xCDEF89AB);

    // Check again if FLASH is unlocked.
    ch32_read_memory_word(handle, CH32_FLASH_CTLR, &ctlr);

    printf("CTLR after unlock: %08" PRIx32 "\r\n", ctlr);

    return !(ctlr & 0x8080);
}

// If unlocked: Erase a 256-byte block of FLASH.
bool ch32_erase_flash_block(rvswd_handle_t* handle, uint32_t addr) {
    if (addr % 256)
        return false;
    ch32_wait_flash(handle);
    ch32_write_memory_word(handle, CH32_FLASH_CTLR, CH32_FLASH_CTLR_FTER);
    ch32_write_memory_word(handle, CH32_FLASH_ADDR, addr);
    ch32_write_memory_word(handle, CH32_FLASH_CTLR, CH32_FLASH_CTLR_FTER | CH32_FLASH_CTLR_STRT);
    ch32_wait_flash(handle);
    ch32_write_memory_word(handle, CH32_FLASH_CTLR, 0);
    return true;
}

// If unlocked: Write a 256-byte block of FLASH.
bool ch32_write_flash_block(rvswd_handle_t* handle, uint32_t addr, const void* data) {
    if (addr % 256)
        return false;

    ch32_wait_flash(handle);
    ch32_write_memory_word(handle, CH32_FLASH_CTLR, CH32_FLASH_CTLR_FTPG);

    ch32_write_memory_word(handle, CH32_FLASH_ADDR, addr);

    uint32_t wdata[64];
    memcpy(wdata, data, sizeof(wdata));
    for (size_t i = 0; i < 64; i++) {
        ch32_write_memory_word(handle, addr + i * 4, wdata[i]);
        ch32_wait_flash_write(handle);
    }

    ch32_write_memory_word(handle, CH32_FLASH_CTLR, CH32_FLASH_CTLR_FTPG | CH32_FLASH_CTLR_PGSTRT);
    ch32_wait_flash(handle);
    ch32_write_memory_word(handle, CH32_FLASH_CTLR, 0);
    vTaskDelay(1);

    uint32_t rdata[64];
    for (size_t i = 0; i < 64; i++) {
        vTaskDelay(0);
        ch32_read_memory_word(handle, addr + i * 4, &rdata[i]);
    }
    if (memcmp(wdata, rdata, sizeof(wdata))) {
        ESP_LOGE(TAG, "Write block mismatch at %08" PRIx32, addr);
        ESP_LOGE(TAG, "Write:");
        for (size_t i = 0; i < 64; i++) {
            ESP_LOGE(TAG, "%zx: %08" PRIx32, i, wdata[i]);
        }
        ESP_LOGE(TAG, "Read:");
        for (size_t i = 0; i < 64; i++) {
            ESP_LOGE(TAG, "%zx: %08" PRIx32, i, rdata[i]);
        }
        return false;
    }

    return true;
}

// If unlocked: Erase and write a range of FLASH memory.
bool ch32_write_flash(rvswd_handle_t* handle, uint32_t addr, const void* _data, size_t data_len) {
    if (addr % 64) {
        return false;
    }

    const uint8_t* data = _data;

    char buffer[256];

    for (size_t i = 0; i < data_len; i += 256) {
        vTaskDelay(0);
        sprintf(buffer, "Erasing 0x%08" PRIx32"...\r\n", addr + i);
        printf("%s", buffer);
        draw_text(buffer);
        if (!ch32_erase_flash_block(handle, addr + i)) {
            ESP_LOGE(TAG, "Error: Failed to erase FLASH at %08" PRIx32, addr + i);
            return false;
        }

        sprintf(buffer, "Writing 0x%08" PRIx32"...\r\n", addr + i);
        printf("%s", buffer);
        draw_text(buffer);
        if (!ch32_write_flash_block(handle, addr + i, data + i)) {
            ESP_LOGE(TAG, "Error: Failed to write FLASH at %08" PRIx32, addr + i);
            return false;
        }
    }

    return true;
}

void rvswd_test(void) {
    rvswd_result_t res;

    rvswd_handle_t handle = {
        .swdio = 22,
        .swclk = 23,
    };

    res = rvswd_init(&handle);

    draw_text("Initializing RVSWD...");

    if (res != RVSWD_OK) {
        draw_text("Failed to initialize");
        ESP_LOGE(TAG, "Init error %u!", res);
        return;
    }

    draw_text("Reset RVSWD...");
    res = rvswd_reset(&handle);

    if (res != RVSWD_OK) {
        draw_text("Failed to reset");
        ESP_LOGE(TAG, "Reset error %u!", res);
        return;
    }

    draw_text("Halt coprocessor...");
    res = ch32_halt_microprocessor(&handle);
    if (res != RVSWD_OK) {
        draw_text("Failed to halt");
        ESP_LOGE(TAG, "Failed to halt");
        return;
    }

    bool bool_res = ch32_unlock_flash(&handle);

    printf("Unlock: %s\r\n", bool_res ? "yes" : "no");

    if (!bool_res) {
        draw_text("Failed to unlock");
        ESP_LOGE(TAG, "Failed to unlock");
        return;
    }

    draw_text("Flashing coprocessor...");
    bool_res = ch32_write_flash(&handle, 0x08000000, ch32_firmware_start, ch32_firmware_end - ch32_firmware_start);
    if (!bool_res) {
        draw_text("Failed to flash");
        ESP_LOGE(TAG, "Failed to write flash");
        return;
    }

    draw_text("Booting coprocessor...");
    res = ch32_reset_microprocessor_and_run(&handle);
    if (res != RVSWD_OK) {
        draw_text("Failed to boot");
        ESP_LOGE(TAG, "Failed to reset and run");
        return;
    }

    draw_text("Coprocessor ready!");

    ESP_LOGI(TAG, "Okay!");

    /*while (1) {

        // Halt
        rvswd_write(&handle, 0x10, 0x80000001);
        rvswd_write(&handle, 0x10, 0x80000001);

        // Read status
        uint32_t value = 0;
        res = rvswd_read(&handle, 0x11, &value);
        if (res == RVSWD_OK) {
            printf("Status after halt: %08" PRIx32 "\n", value);
        } else {
            ESP_LOGE(TAG, "Status error %u!", res);
        }

        // Dump some memory

        for (uint32_t address = 0x0; address < 0x10; address++) {
            uint32_t value = 0;
            ch32_read_memory_word(&handle, address, &value);
            printf("0x%08" PRIx32 "  0x%08" PRIx32 "\r\n", address, value);
        }
        printf("\r\n");

        // Resume
        rvswd_write(&handle, 0x10, 0x40000001);
        rvswd_write(&handle, 0x10, 0x40000001);

        // Read status
        value = 0;
        res = rvswd_read(&handle, 0x11, &value);
        if (res == RVSWD_OK) {
            printf("Status after resume: %08" PRIx32 "\n", value);
        } else {
            ESP_LOGE(TAG, "Status error %u!", res);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }*/

}

void app_main(void) {

    demo_semaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(demo_semaphore);

    display_version();
    bsp_init();
    display_test();

    rvswd_test();

    vTaskDelay(pdMS_TO_TICKS(200));

    uint16_t version = 0;
    esp_err_t ch32fwres = ch32_get_firmware_version(&version);

    if (ch32fwres != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ch32 firmware version %d", ch32fwres);
    }

    printf("CH32V203 firmware version %"PRIu16"\r\n", version);

    if (version != 4) {
        rvswd_test();
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    ch32_set_display_backlight(screen_brightness*5);
    ch32_set_keyboard_backlight(keyboard_brightness*5);
    bsp_c6_control(true, true);
    //bsp_c6_control(false, true);

    pax_buf_t clipbuffer;
    pax_buf_init(&clipbuffer, NULL, 800, 480, PAX_BUF_1_PAL);
    //pax_buf_set_orientation(&clipbuffer, PAX_O_ROT_CW);

    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = 4,
    };

    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;
    esp_err_t res = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create a new on-chip LDO power control driver");
        return;
    }

    res = sd_test(pwr_ctrl_handle);
    if (res != ESP_OK) {
        char text[128];
        sprintf(text, "SD card failed:\n%s", esp_err_to_name(res));
        draw_text(text);
        vTaskDelay(pdMS_TO_TICKS(1000));
    } else {
        draw_text("SD card OK");
    }

    /*res = sdio_test();
    if (res != ESP_OK) {
        char text[128];
        sprintf(text, "SDIO failed:\n%s", esp_err_to_name(res));
        draw_text(text);
        vTaskDelay(pdMS_TO_TICKS(1000));
    } else {
        draw_text("SDIO OK");
    }*/

    //usb_host_test();

    /*
    // Test pins of USB port on expansion header
    gpio_config_t dp_config = {
        .pin_bit_mask = BIT64(26),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = true,
        .pull_down_en = false,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    res = gpio_config(&dp_config);
    if (res != ESP_OK) {
        return;
    }

    gpio_config_t dn_config = {
        .pin_bit_mask = BIT64(27),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = true,
        .pull_down_en = false,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    res = gpio_config(&dn_config);
    if (res != ESP_OK) {
        return;
    }

    gpio_set_level(26, 1);
    gpio_set_level(27, 1);*/

    // CH32 stuff

    /*gpio_config_t swio_config = {
        .pin_bit_mask = BIT64(22),
        .mode         = GPIO_MODE_OUTPUT_OD,
        .pull_up_en   = true,
        .pull_down_en = false,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    res = gpio_config(&swio_config);
    if (res != ESP_OK) {
        return;
    }

    gpio_config_t swck_config = {
        .pin_bit_mask = BIT64(23),
        .mode         = GPIO_MODE_OUTPUT_OD,
        .pull_up_en   = true,
        .pull_down_en = false,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    res = gpio_config(&swck_config);
    if (res != ESP_OK) {
        return;
    }

    gpio_set_level(22, 1);
    gpio_set_level(23, 1);*/

    /*ESP_LOGW(TAG, "TEST PINS");
    while (1) {
        gpio_set_level(22, 1);
        gpio_set_level(23, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(22, 0);
        gpio_set_level(23, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
    }*/

    i2s_test();

    while (1) {
        draw_test();

        start_demo = false;
        while (!start_demo) {
            xSemaphoreTake(demo_semaphore, portMAX_DELAY);
            ESP_LOGI(TAG, "Redraw!");
            if (current_key < 0) {
                draw_test();
            } else {
                draw_key(current_key);
            }
        }
        pax_techdemo_init(display_test_get_buf(), &clipbuffer);
        bool finished = false;
        size_t start = esp_timer_get_time() / 1000;
        while (!finished) {
            size_t now = esp_timer_get_time() / 1000;
            finished = pax_techdemo_draw(now - start);
            size_t after = esp_timer_get_time() / 1000;
            display_test_flush();
            int delay = 33 - (after - now);
            if (delay < 0) delay = 0;
            //ESP_LOGI("MAIN", "Delay %d, took %d", delay, after - now);
            vTaskDelay(pdMS_TO_TICKS(delay));
        }
    }
}

void demo_call(bsp_input_t input, bool pressed) {
    if (input == 42 && pressed) {
        start_demo = true;
    } else if (input == 0x47 && pressed) {
        start_demo = true;
    } else if (input == 1 && pressed) {
        if (volume > 0x00) {
            volume--;
        }
        es8156_codec_set_voice_volume(volume);
    } else if (input == 2 && pressed) {
        if (volume < 0xFF) {
            volume++;
        }
        es8156_codec_set_voice_volume(volume);
    } else if (input == 5 && pressed) {
        if (screen_brightness > 0) {
            screen_brightness--;
        }
    ch32_set_display_backlight(screen_brightness*5);
    } else if (input == 6 && pressed) {
        if (screen_brightness < 51) {
            screen_brightness++;
        }
        ch32_set_display_backlight(screen_brightness*5);
    } else if (input == 7 && pressed) {
        if (keyboard_brightness > 0) {
            keyboard_brightness--;
        }
        ch32_set_keyboard_backlight(keyboard_brightness*5);
    } else if (input == 0x18 && pressed) {
        if (keyboard_brightness < 51) {
            keyboard_brightness++;
        }
        ch32_set_keyboard_backlight(keyboard_brightness*5);
    } else if (pressed) {
        current_key = input;
    } else {
        current_key = -1;
    }
    xSemaphoreGive(demo_semaphore);
}
