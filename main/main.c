/* I2S Digital Microphone Recording Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "sdmmc_cmd.h"
#include "sdkconfig.h"
#include "driver/sdmmc_host.h"

static const char* TAG = "pdm_rec";
#define REC_TIME            15
#define BIT_SAMPLE          32
#define SAMPLE_RATE         48000
#define I2S_CH              0
#define NUM_CHANNELS        (1) // For mono recording only!
#define SD_MOUNT_POINT      "/sdcard"
#define SAMPLE_SIZE         (BIT_SAMPLE * 1024)
#define BYTE_RATE           (SAMPLE_RATE * (BIT_SAMPLE / 8)) * NUM_CHANNELS

// When testing SD and SPI modes, keep in mind that once the card has been
// initialized in SPI mode, it can not be reinitialized in SD mode without
// toggling power to the card.
sdmmc_host_t host = SDMMC_HOST_DEFAULT();
sdmmc_card_t* card;

static int16_t i2s_readraw_buff[SAMPLE_SIZE];
size_t bytes_read;
const int WAVE_HEADER_SIZE = 44;

void mount_sdcard(void)
{
    esp_err_t ret;
    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 8 * 1024
    };
    ESP_LOGI(TAG, "Initializing SD card");
    const char mount_point[] = SD_MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");
    ESP_LOGI(TAG, "Using SDMMC peripheral");
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;//hard coded single bit sdmmc, for esp32s3 eye v2.2
    slot_config.clk = 39;
    slot_config.cmd = 38;
    slot_config.d0 = 40;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");
    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
}

void generate_wav_header(char* wav_header, uint32_t wav_size, uint32_t sample_rate){

    // See this for reference: http://soundfile.sapp.org/doc/WaveFormat/
    uint32_t file_size = wav_size + WAVE_HEADER_SIZE - 8;
    uint32_t byte_rate = BYTE_RATE;

    const char set_wav_header[] = {
        'R','I','F','F', // ChunkID
        file_size, file_size >> 8, file_size >> 16, file_size >> 24, // ChunkSize
        'W','A','V','E', // Format
        'f','m','t',' ', // Subchunk1ID
        0x10, 0x00, 0x00, 0x00, // Subchunk1Size (16 for PCM)
        0x01, 0x00, // AudioFormat (1 for PCM)
        0x01, 0x00, // NumChannels (1 channel)
        sample_rate, sample_rate >> 8, sample_rate >> 16, sample_rate >> 24, // SampleRate
        byte_rate, byte_rate >> 8, byte_rate >> 16, byte_rate >> 24, // ByteRate
        0x04, 0x00, // BlockAlign --NumChannels * BitsPerSample/8
        0x20, 0x00, //32 bits
        'd','a','t','a', // Subchunk2ID
        wav_size, wav_size >> 8, wav_size >> 16, wav_size >> 24, // Subchunk2Size
    };

    memcpy(wav_header, set_wav_header, sizeof(set_wav_header));
}

void record_wav(uint32_t rec_time)
{
    // Use POSIX and C standard library functions to work with files.
    int flash_wr_size = 0;
    ESP_LOGI(TAG, "Opening file");

    char wav_header_fmt[WAVE_HEADER_SIZE];

    uint32_t flash_rec_time = BYTE_RATE * rec_time;
    generate_wav_header(wav_header_fmt, flash_rec_time, SAMPLE_RATE);

    // First check if file exists before creating a new file.
    struct stat st;
    if (stat(SD_MOUNT_POINT"/record.wav", &st) == 0) {
        // Delete it if it exists
        unlink(SD_MOUNT_POINT"/record.wav");
    }

    // Create new WAV file
    FILE* f = fopen(SD_MOUNT_POINT"/record.wav", "a");
    //FILE* txt = fopen(SD_MOUNT_POINT"/record.txt", "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    // Write the header to the WAV file
    fwrite(wav_header_fmt, 1, WAVE_HEADER_SIZE, f);
    //fwrite(wav_header_fmt, 1, WAVE_HEADER_SIZE, txt);

    // Start recording
    while (flash_wr_size < flash_rec_time) {
        // Read the RAW samples from the microphone
        i2s_read(I2S_CH, (char *)i2s_readraw_buff, SAMPLE_SIZE, &bytes_read, 100);
        //copy buffer to file
        if (bytes_read > 0) {
            ESP_LOGI(TAG, "Bytes read %zu", bytes_read);
            // Write to file
            fwrite(i2s_readraw_buff, 1, bytes_read, f);
            flash_wr_size += bytes_read;
        }
        
    }

    ESP_LOGI(TAG, "Recording done!");
    fclose(f);

    ESP_LOGI(TAG, "File written on SDCard");
    

}

void init_microphone(void)
{
    // Set the I2S configuration as //PDM and 16bits per sample
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = (i2s_bits_per_sample_t)32,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
        .dma_buf_count = 8,
        .dma_buf_len = 200,
        .use_apll = 0,
    };

    // Set the pinout configuration (set using menuconfig)
    i2s_pin_config_t pin_config = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = 41,//I2S_PIN_NO_CHANGE,
        .ws_io_num = 42,    //CONFIG_EXAMPLE_I2S_CLK_GPIO,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = 2,
    };

    // Call driver installation function before any I2S R/W operation.
    ESP_ERROR_CHECK( i2s_driver_install(I2S_CH, &i2s_config, 0, NULL) );
    ESP_ERROR_CHECK( i2s_set_pin(I2S_CH, &pin_config) );
    i2s_zero_dma_buffer(I2S_CH);
    //ESP_ERROR_CHECK( i2s_set_clk(I2S_CH, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO) );
    
}

void app_main(void)
{
    ESP_LOGI(TAG, "PDM microphone recording Example start");
    // Mount the SDCard for recording the audio file
    mount_sdcard();
    // Init the PDM digital microphone
    init_microphone();
    ESP_LOGI(TAG, "Starting recording for %d seconds!", REC_TIME);
    // Start Recording
    record_wav(REC_TIME);
    // Stop I2S driver and destroy
    ESP_ERROR_CHECK( i2s_driver_uninstall(I2S_CH) );
    //unmount sdcard
    esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, card);
    ESP_LOGI(TAG, "Card unmounted");
}
