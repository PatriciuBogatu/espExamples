#pragma once

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_http_client.h"

// Hardware Configuration (ESP32-S3-Box-3 specific)
#define I2S_NUM I2S_NUM_1
#define PDM_CLK_GPIO GPIO_NUM_1
#define PDM_DIN_GPIO GPIO_NUM_2
#define BUTTON_PIN GPIO_NUM_0 // Built-in BOOT button

// Audio Configuration
#define SAMPLE_RATE 16000
#define BIT_DEPTH I2S_DATA_BIT_WIDTH_16BIT
#define BUFFER_SIZE_MS 3000 // 3 seconds
#define DMA_BUF_COUNT 4
#define DMA_BUF_LEN (SAMPLE_RATE * BIT_DEPTH / 8 / (1000 / BUFFER_SIZE_MS))

namespace app
{

  class BedrockClient
  {

  private:
    static const char *TAG = "VOICE2AWS";
    uint8_t *audio_buffer = NULL;
    size_t total_bytes = 0;

  public:
    // HTTP Upload with WAV Header
    void upload_audio()
    {
      uint8_t wav_header[44];
      create_wav_header(wav_header, total_bytes);

      esp_http_client_config_t config = {
          .url = "https://your-aws-bedrock-endpoint",
          .method = HTTP_METHOD_POST,
          .buffer_size = 4096,
      };

      esp_http_client_handle_t client = esp_http_client_init(&config);
      esp_http_client_set_header(client, "Content-Type", "audio/wav");

      // First send WAV header
      esp_http_client_set_post_field(client, (const char *)wav_header, 44);
      esp_http_client_perform(client);

      // Then send audio data
      esp_http_client_set_post_field(client, (const char *)audio_buffer, total_bytes);
      esp_http_client_perform(client);

      esp_http_client_cleanup(client);
    }

    void start()
    {

      upload_audio();
    }
  };
}
