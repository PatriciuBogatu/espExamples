#pragma once

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_spiffs.h"
#include "esp_http_client.h"


// Audio Configuration
#define NUM_CHANNELS    1
#define RECORD_SECONDS  5
#define BUFFER_SIZE     2048
#define BITS_PER_SAMPLE   16
#define SAMPLE_RATE       16000  // Logs show 16kHz sampling
#define NUM_CHANNELS      2     // Stereo (MIC1 + MIC2)
#define CHANNEL_MASK      0x03  // Binary 0b11 (enable both channels)


#pragma pack(push, 1)
typedef struct {
    char     RIFF[4];        // "RIFF"
    uint32_t ChunkSize;      // File size - 8
    char     WAVE[4];        // "WAVE"
    char     fmt[4];         // "fmt "
    uint32_t Subchunk1Size;  // 16 for PCM
    uint16_t AudioFormat;    // 1 = PCM
    uint16_t NumChannels;    // 1 or 2
    uint32_t SampleRate;     // e.g., 16000
    uint32_t ByteRate;       // SampleRate * NumChannels * BitsPerSample/8
    uint16_t BlockAlign;     // NumChannels * BitsPerSample/8
    uint16_t BitsPerSample;  // 8, 16, etc
    char     Data[4];        // "data"
    uint32_t Subchunk2Size;  // Data size
} wav_header_t;
#pragma pack(pop)


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

        ESP_LOGI("MEM", "SPIRAM - Free: %8zu | Min Free: %8zu | Largest Block: %8zu",
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),
             heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

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

      size_t free_spiram = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

      ESP_LOGI("AppRecorder", "Checking SPIRAM %d", free_spiram);
    // if (file_size > free_spiram) {
    //     ESP_LOGE(TAG, "Insufficient SPIRAM: Needed %zu, available %zu", file_size, free_spiram);
    //     return;
    // }
      uint8_t *file_data = static_cast<uint8_t *>(heap_caps_malloc(file_size, MALLOC_CAP_SPIRAM));
      ESP_LOGI(TAG, "Upload buffer %p", file_data);
      if (!file_data)
      {
        ESP_LOGE(TAG, "Failed to allocate upload buffer %p", file_data);
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

      ESP_LOGI("AppRecorder", "Configuring rest client");
      // 2. Configure HTTP client
      ESP_LOGI("AppRecoreder", "UPLOAD URL IS %s", UPLOAD_URL);
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

      ESP_LOGI("Recorder", "REST Client inited");

      // 3. Set headers and post data
      esp_http_client_set_header(client, "Content-Type", "audio/wav");
      esp_http_client_set_post_field(client, reinterpret_cast<char *>(file_data), file_size);

      // 4. Execute upload
      
      ESP_LOGI("AppRecorder", "Firing request");
      esp_err_t err = esp_http_client_perform(client);
      ESP_LOGI("AppRecorder", "Firing request DONE");
      if (err == ESP_OK)
      {
        ESP_LOGI("AppRecorder", "Rest client call with no errors");
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
      size_t total, used;
    esp_spiffs_info(NULL, &total, &used);
    if (used > (total * 0.9)) {
        ESP_LOGE(TAG, "SPIFFS full: %zu/%zu bytes", used, total);
        return;
    }

    // 2. Open file with error details
    FILE *file = fopen(filename, "wb");
    if (!file) {
        ESP_LOGE(TAG, "fopen failed: %s", strerror(errno));
        return;
    }

    // 3. Write with verification
    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    size_t written = fwrite(audio_buffer, 1, total_bytes, file);
    xSemaphoreGive(buffer_mutex);

    if (written != total_bytes) {
        ESP_LOGE(TAG, "Partial write: %zu/%zu (%s)", 
                written, total_bytes, strerror(errno));
    }

    // 4. Force flush
    fflush(file);
    fsync(fileno(file));
    fclose(file);

    // 5. Verify file size
    struct stat st;
    stat(filename, &st);
    ESP_LOGI(TAG, "File verification: %li bytes", st.st_size);
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

    static void record_task_wrapper(void *arg)
    {
      static_cast<AppRecorder *>(arg)->record_task();
    }

    void record_task()
    {
      ESP_LOGI("MAIN", "MIC INIT");
esp_codec_dev_handle_t mic_dev = bsp_audio_codec_microphone_init();
if (mic_dev == NULL) {
    ESP_LOGE(TAG, "Failed to initialize microphone codec");
    return;
}

ESP_LOGI("MAIN", "MIC INIT DONE");

// Open codec device with audio format (CORRECTED)
esp_codec_dev_sample_info_t fs = {
    .bits_per_sample = BITS_PER_SAMPLE,  // Must come first per struct definition
    .channel = NUM_CHANNELS,            // Now in correct position
    .channel_mask = 0x3,               // Enable first channel
    .sample_rate = SAMPLE_RATE,
    .mclk_multiple = 256                // Default MCLK multiplier
};

if (esp_codec_dev_open(mic_dev, &fs) != ESP_CODEC_DEV_OK) {
    ESP_LOGE(TAG, "Failed to open codec device with specified format");
    return;
}

ESP_LOGI("MAIN", "CODEC OPEN DONE");

// Create WAV file
FILE *f = fopen("/spiffs/recording.wav", "wb");
if (f == NULL) {
    ESP_LOGE("MAIN", "Failed to create file");
    return;
}

// Prepare WAV header with correct initialization order (FIXED)
wav_header_t wav_header = {
    .RIFF = {'R', 'I', 'F', 'F'},
    .ChunkSize = 0, // Placeholder
    .WAVE = {'W', 'A', 'V', 'E'},
    .fmt = {'f', 'm', 't', ' '},
    .Subchunk1Size = 16,
    .AudioFormat = 1,
    .NumChannels = NUM_CHANNELS,
    .SampleRate = SAMPLE_RATE,
    .ByteRate = SAMPLE_RATE * NUM_CHANNELS * BITS_PER_SAMPLE / 8,
    .BlockAlign = NUM_CHANNELS * BITS_PER_SAMPLE / 8,
    .BitsPerSample = BITS_PER_SAMPLE,
    .Data = {'d', 'a', 't', 'a'},
    .Subchunk2Size = 0 // Placeholder
};
 
     // Write initial header
     fwrite(&wav_header, sizeof(wav_header), 1, f);
 
     // Record audio
     ESP_LOGI(TAG, "Recording...");
     size_t buffer_bytes = BUFFER_SIZE * NUM_CHANNELS * sizeof(int16_t);
int16_t *audio_buffer = (int16_t*)heap_caps_malloc(buffer_bytes, MALLOC_CAP_SPIRAM);
if (!audio_buffer) {
    ESP_LOGE(TAG, "Failed to allocate audio buffer");
    return;
}
     size_t total_bytes = 0;
     int16_t duration_ms = RECORD_SECONDS * 1000;
     int32_t start = esp_log_timestamp();
 
     while (esp_log_timestamp() - start < duration_ms) {
         esp_codec_dev_read(mic_dev, audio_buffer, BUFFER_SIZE);
        fwrite(audio_buffer, 1, BUFFER_SIZE * NUM_CHANNELS * sizeof(int16_t), f);
         
     }
 
     free(audio_buffer);
     ESP_LOGI(TAG, "Recording finished");
 
     // Update WAV header with actual sizes
     wav_header.ChunkSize = total_bytes + sizeof(wav_header) - 8;
     wav_header.Subchunk2Size = total_bytes;
 
     fseek(f, 0, SEEK_SET);
     fwrite(&wav_header, sizeof(wav_header), 1, f);
     fclose(f);
 
     // Clean up
     esp_codec_dev_close(mic_dev);

     upload_file_task();
     vTaskDelete(NULL);
    }

    void upload_recording()
    {
      xTaskCreate(upload_task_wrapper, "upload_task", 4096, this, 4, NULL);
    }

  };

  bool AppRecorder::isRecording = false;
  bool AppRecorder::recordingFinished = false;
  bool AppRecorder::i2s_enabled = false;
} // namespace app