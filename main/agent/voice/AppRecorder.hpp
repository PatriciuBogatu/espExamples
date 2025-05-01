#pragma once

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "driver/i2s_pdm.h"
#include "esp_private/gdma.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_spiffs.h"
#include "esp_http_client.h"

// Hardware Configuration
#define I2S_NUM I2S_NUM_0
#define PDM_CLK_GPIO GPIO_NUM_1
#define PDM_DIN_GPIO GPIO_NUM_2

// Audio Configuration
#define SAMPLE_RATE 16000 // 16 kHz
#define BIT_DEPTH I2S_DATA_BIT_WIDTH_16BIT
#define RECORD_DURATION_MS 5000 // 5 seconds
#define WAV_HEADER_SIZE 44

namespace app
{

  class AppRecorder
  {
  private:
    static constexpr const char *TAG = "AudioRecorder";
    static bool isRecording; // Static state flag
    static bool recordingFinished;
    static bool i2s_enabled;
    uint8_t *audio_buffer = nullptr;
    bool bufferFull = false;
    size_t total_bytes = 0;
    i2s_chan_handle_t rx_chan;
    SemaphoreHandle_t buffer_mutex;
    bool data_ready = false;
    static constexpr const char *UPLOAD_URL = "http://" CONFIG_REST_SERVER_IP ":" CONFIG_REST_SERVER_PORT "/upload";

  public:
    AppRecorder()
    {
      buffer_mutex = xSemaphoreCreateMutex();
    }

    ~AppRecorder()
    {
      if (audio_buffer)
        heap_caps_free(audio_buffer);
      vSemaphoreDelete(buffer_mutex);
      i2s_del_channel(rx_chan);
    }

    void upload_file_task()
    {
      ESP_LOGI(TAG, "Starting file upload");

      // 1. Read the saved file
      FILE *file = fopen("/spiffs/recording.wav", "rb");
      if (!file)
      {
        ESP_LOGE(TAG, "Failed to open file for upload");
        return;
      }

      fseek(file, 0, SEEK_END);
      size_t file_size = ftell(file);
      fseek(file, 0, SEEK_SET);

      uint8_t *file_data = static_cast<uint8_t *>(heap_caps_malloc(file_size, MALLOC_CAP_SPIRAM));
      if (!file_data)
      {
        ESP_LOGE(TAG, "Failed to allocate upload buffer");
        fclose(file);
        return;
      }

      if (fread(file_data, 1, file_size, file) != file_size)
      {
        ESP_LOGE(TAG, "File read error");
        fclose(file);
        free(file_data);
        return;
      }
      fclose(file);

      // 2. Configure HTTP client
      esp_http_client_config_t config = {
          .url = UPLOAD_URL,
          .method = HTTP_METHOD_POST,
          .event_handler = [](esp_http_client_event_t *evt)
          {
            switch (evt->event_id)
            {
            case HTTP_EVENT_ON_DATA:
              ESP_LOGI("UPLOAD", "Received response: %.*s", evt->data_len, (char *)evt->data);
              break;
            default:
              break;
            }
            return ESP_OK;
          },
          .transport_type = HTTP_TRANSPORT_OVER_TCP,
      };

      esp_http_client_handle_t client = esp_http_client_init(&config);

      // 3. Set headers and post data
      esp_http_client_set_header(client, "Content-Type", "audio/wav");
      esp_http_client_set_post_field(client, reinterpret_cast<char *>(file_data), file_size);

      // 4. Execute upload
      esp_err_t err = esp_http_client_perform(client);
      if (err == ESP_OK)
      {
      }
      else
      {
        ESP_LOGE(TAG, "Upload failed: %s", esp_err_to_name(err));
      }

      // 5. Cleanup
      free(file_data);
      esp_http_client_cleanup(client);

      isRecording = false;
      recordingFinished = true;
    }

    static void upload_task_wrapper(void *arg)
    {
      static_cast<AppRecorder *>(arg)->upload_file_task();
      vTaskDelete(NULL);
    }
    void save_to_file(const char *filename)
    {
      FILE *file = fopen(filename, "wb");
      if (!file)
      {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
      }

      xSemaphoreTake(buffer_mutex, portMAX_DELAY);
      ESP_LOGI(TAG, "Writing %zu total_bytes", total_bytes);
      size_t bytes_written = fwrite(audio_buffer, 1, total_bytes, file);
      xSemaphoreGive(buffer_mutex);

      ESP_LOGI(TAG, "Wrote %zu bytes to %s", bytes_written, filename);
      fclose(file); // No need to unregister SPIFFS here
    }

    void init_i2s()
    {
      // Calculate required buffer size
      size_t data_size = SAMPLE_RATE * 2 * RECORD_DURATION_MS / 1000; // 16kHz * 16-bit * 5s
      size_t buffer_size = WAV_HEADER_SIZE + data_size;

      // Allocate buffer if not already allocated
      if (!audio_buffer || i2s_enabled)
      {
        ESP_LOGI(TAG, "Allocating buffer");
        audio_buffer = (uint8_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
        if (!audio_buffer)
        {
          ESP_LOGE(TAG, "Failed to allocate audio buffer!");
          return;
        }
      }

      if (i2s_enabled)
        return;

      // 2. Configure I2S channel with DMA settings
      i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

      ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));

      // 4. Configure PDM RX
      i2s_pdm_rx_config_t pdm_rx_cfg = {
          .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
          .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(BIT_DEPTH, I2S_SLOT_MODE_MONO),
          .gpio_cfg = {
              .clk = PDM_CLK_GPIO,
              .din = PDM_DIN_GPIO,
              .invert_flags = {.clk_inv = false},
          },
      };
      ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_chan, &pdm_rx_cfg));
      ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
      i2s_enabled = true;
    }

    void start_recording()
    { // Remove static modifier
      isRecording = true;
      recordingFinished = false;
      xSemaphoreTake(buffer_mutex, portMAX_DELAY);
      total_bytes = 0;
      data_ready = false;
      xSemaphoreGive(buffer_mutex);

      xTaskCreate(&record_task_wrapper, "rec_task", 4096, this, 5, NULL);
    }

    bool is_data_ready() { return data_ready; }

    static bool checkIsRecording() { return isRecording; }

    static bool checkRecordingFinished() { return recordingFinished; }

    static void setRecordingFinishedToFalse() { recordingFinished = false; }

    void clear_buffer()
    {
      xSemaphoreTake(buffer_mutex, portMAX_DELAY);
      total_bytes = 0;
      data_ready = false;
      xSemaphoreGive(buffer_mutex);
    }

  private:
    void create_wav_header(uint8_t *header, uint32_t data_size)
    {
      memcpy(header, "RIFF", 4);
      *(uint32_t *)(header + 4) = data_size + 36;
      memcpy(header + 8, "WAVEfmt ", 8);
      *(uint32_t *)(header + 16) = 16;
      *(uint16_t *)(header + 20) = 1;
      *(uint16_t *)(header + 22) = 1;
      *(uint32_t *)(header + 24) = SAMPLE_RATE;
      *(uint32_t *)(header + 28) = SAMPLE_RATE * 2;
      *(uint16_t *)(header + 32) = 2;
      *(uint16_t *)(header + 34) = 16;
      memcpy(header + 36, "data", 4);
      *(uint32_t *)(header + 40) = data_size;
    }

    static void record_task_wrapper(void *arg)
    {
      static_cast<AppRecorder *>(arg)->record_task();
    }

    void record_task()
    {
      size_t bytes_read;
      const uint32_t start_time = esp_log_timestamp();

      while ((esp_log_timestamp() - start_time) < RECORD_DURATION_MS)
      {
        xSemaphoreTake(buffer_mutex, portMAX_DELAY);

        if (total_bytes + 1600 > (SAMPLE_RATE * 2 * RECORD_DURATION_MS / 1000))
        {
          ESP_LOGW(TAG, "Buffer full!");
          bufferFull = true;
          break;
        }

        i2s_channel_read(rx_chan,
                         audio_buffer + WAV_HEADER_SIZE + total_bytes,
                         1600, &bytes_read, pdMS_TO_TICKS(100));

        total_bytes += bytes_read;
        xSemaphoreGive(buffer_mutex);

        vTaskDelay(pdMS_TO_TICKS(10));
      }

      ESP_LOGI(TAG, "Out of record_task loop");

      if (bufferFull)
      {
        xSemaphoreGive(buffer_mutex);
        bufferFull = false;
      }

      finalize_recording();

      data_ready = true;
      ESP_LOGI(TAG, "Recording finished");
      vTaskDelete(NULL);
    }

    void upload_recording()
    {
      xTaskCreate(upload_task_wrapper, "upload_task", 4096, this, 4, NULL);
    }

    void finalize_recording()
    {
      uint8_t header[WAV_HEADER_SIZE];
      create_wav_header(header, total_bytes);

      xSemaphoreTake(buffer_mutex, portMAX_DELAY);
      memmove(audio_buffer + WAV_HEADER_SIZE, audio_buffer, total_bytes);
      memcpy(audio_buffer, header, WAV_HEADER_SIZE);
      total_bytes += WAV_HEADER_SIZE;
      xSemaphoreGive(buffer_mutex);

      ESP_LOGI(TAG, "Recording ready: %zu bytes", total_bytes);
      save_to_file("/spiffs/recording.wav");
      upload_recording();
    }
  };

  bool AppRecorder::isRecording = false;
  bool AppRecorder::recordingFinished = false;
  bool AppRecorder::i2s_enabled = false;
} // namespace app