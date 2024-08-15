#include "sid.h"

#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "libcsid.h"

/*#define NUM_SAMPLES 300
unsigned short samples_data[2 * NUM_SAMPLES];

static void render_audio() {
    libcsid_render(samples_data, NUM_SAMPLES);

    int pos = 0;
    int left = 2 * NUM_SAMPLES;
    unsigned char *ptr = (unsigned char *)samples_data;

    while (left > 0) {
        size_t written = 0;
        i2s_write(I2S_NUM_0, (const char *)ptr, left, &written, 100 / portTICK_PERIOD_MS);
        pos += written;
        ptr += written;
        left -= written;
    }
}*/

#define NUM_SAMPLES 800
static unsigned short    mono_samples_data[2 * NUM_SAMPLES];
static unsigned short    samples_data[2 * 2 * NUM_SAMPLES];
static TaskHandle_t      player_handle     = NULL;
static i2s_chan_handle_t player_i2s_handle = NULL;

static void render_audio() {
    libcsid_render(mono_samples_data, NUM_SAMPLES);

    // Duplicate mono samples to create stereo buffer
    for (unsigned int i = 0; i < NUM_SAMPLES; i++) {
        unsigned int sample_val = mono_samples_data[i];
        samples_data[i * 2 + 0] = (unsigned short)(((short)sample_val) * 1.0);
        samples_data[i * 2 + 1] = (unsigned short)(((short)sample_val) * 0.6);
    }

    int            pos  = 0;
    int            left = 2 * 2 * NUM_SAMPLES;
    unsigned char *ptr  = (unsigned char *)samples_data;

    while (left > 0) {
        size_t written = 0;
        i2s_channel_write(player_i2s_handle, (char const *)ptr, left, &written, 100 / portTICK_PERIOD_MS);
        pos  += written;
        ptr  += written;
        left -= written;
    }
}

#include "commando.inc"

static void player_task(void *pvParameters) {
    while (1) {
        render_audio();
        vTaskDelay(1);
    }
}


esp_err_t sid_init(i2s_chan_handle_t i2s_handle) {
    player_i2s_handle = i2s_handle;
    libcsid_init(16000, SIDMODEL_6581);
    libcsid_load((unsigned char *)&commando_sid, commando_sid_len, 0);

    printf("SID Title: %s\n", libcsid_gettitle());
    printf("SID Author: %s\n", libcsid_getauthor());
    printf("SID Info: %s\n", libcsid_getinfo());

    xTaskCreate(player_task, "Audio player task", 2048, NULL, tskIDLE_PRIORITY + 2, &player_handle);

    return ESP_OK;
}
