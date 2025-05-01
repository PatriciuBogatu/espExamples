#pragma once

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_board_init.h"
#include "model_path.h"
#include "ui.h"
#include "string.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "driver/gpio.h"

#define SAMPLE_RATE 16000  // 16 kHz
#define NUM_CHANNELS 4     // Raw mode = 4; processed mode = 3
#define BITS_PER_SAMPLE 16 // int16_t buffer
#define DURATION_SEC 5

namespace app
{

  class AppWake
  {

  private:
    static esp_afe_sr_iface_t *afe_handle;
    static esp_afe_sr_data_t *afe_data;
    static afe_config_t *afe_config;
    static volatile int task_flag;
    static int count;
    static srmodel_list_t *models;
    static bool active;

  public:
    static void deinit()
    {
      ESP_LOGI("AppWake", "Deiniting the board");
      // Signal tasks to exit
      task_flag = 0; // Signal tasks to exit

      vTaskDelay(pdMS_TO_TICKS(1000));

      if (afe_handle && afe_data)
      {
        afe_handle->destroy(afe_data);
        afe_data = nullptr;
        afe_handle = nullptr; // Reset handle
      }
      if (afe_config)
      {
        free(afe_config); // Free config memory
        afe_config = nullptr;
      }

      vTaskDelay(pdMS_TO_TICKS(200));
      ESP_LOGI("AppWake", "Deiniting hardware");
      esp_board_deinit(); // Deinit hardware last
      // esp_srmodel_deinit(models);

      vTaskDelay(pdMS_TO_TICKS(1000));

      active = false;
      vTaskDelete(NULL);
    }

    static void feed_Task(void *arg)
    {
      esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t *)arg;
      int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
      ESP_LOGI("AppWake", "Chunksize is %d", audio_chunksize);
      int nch = afe_handle->get_feed_channel_num(afe_data);
      int feed_channel = esp_get_feed_channel();
      ESP_LOGI("AppWake", "Feed channel is %d", feed_channel);
      assert(nch == feed_channel);
      int16_t *i2s_buff = (int16_t *)malloc(audio_chunksize * sizeof(int16_t) * feed_channel);
      assert(i2s_buff);
      int i = 0;
      while (task_flag)
      {
        if (i == 0)
        {
          ESP_LOGI("AppWake", "Buffer len is %d", audio_chunksize * sizeof(int16_t) * feed_channel);
          i++;
        }

        esp_get_feed_data(true, i2s_buff, audio_chunksize * sizeof(int16_t) * feed_channel);
        // esp_audio_play(i2s_buff, audio_chunksize * sizeof(int16_t) * feed_channel, 0);
        afe_handle->feed(afe_data, i2s_buff);
      }
      if (i2s_buff)
      {
        free(i2s_buff);
        i2s_buff = NULL;
      }

      ESP_LOGI("AppWake", "FeedTask done");
      vTaskDelete(NULL);
    }

    static void detect_Task(void *arg)
    {
      esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t *)arg;
      int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
      int16_t *buff = (int16_t *)malloc(afe_chunksize * sizeof(int16_t));
      assert(buff);
      ESP_LOGI("AppWake", "------------detect start------------\n");

      while (task_flag)
      {

        afe_fetch_result_t *res = afe_handle->fetch(afe_data);
        if (!res || res->ret_value == ESP_FAIL)
        {
          ESP_LOGI("AppWake", "fetch error!\n");
          break;
        }
        // printf("vad state: %d\n", res->vad_state);

        if (res->wakeup_state == WAKENET_DETECTED)
        {
          char buffer[50];
          ESP_LOGI("AppWake", "wakeword detected: len=%d\n", res->data_size);
          sprintf(buffer, "%d", count++);
          lvgl_port_lock(0);
          lv_label_set_text(ui_sensorLabel, buffer);
          lvgl_port_unlock();
          deinit();
        }
      }
      if (buff)
      {
        free(buff);
        buff = NULL;
      }
      ESP_LOGI("AppWake", "DetectTask done");
      vTaskDelete(NULL);
    }

    static bool isActive() { return active; }

    static void init()
    {

      ESP_LOGI("AppWake", "Initing the board");
      ESP_ERROR_CHECK(esp_board_init(16000, 1, 16));
      // ESP_ERROR_CHECK(esp_sdcard_init("/sdcard", 10));

      models = esp_srmodel_init("model");
      if (models)
      {

        ESP_LOGI("AppWake", "There are models %d\n", models->num);

        for (int i = 0; i < models->num; i++)
        {
          if (strstr(models->model_name[i], ESP_WN_PREFIX) != NULL)
          {
            ESP_LOGI("AppWake", "wakenet model in flash: %s\n", models->model_name[i]);
          }
        }
      }

      ESP_LOGI("AppWake", "Initing AFE");
      afe_config = afe_config_init(esp_get_input_format(), models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
      afe_handle = esp_afe_handle_from_config(afe_config);
      afe_data = afe_handle->create_from_config(afe_config);
      ESP_LOGI("AppWake", "Initing AFE done");

      task_flag = 1;
      xTaskCreatePinnedToCore(feed_Task, "feed", 8 * 1024, (void *)afe_data, 5, NULL, 0);
      xTaskCreatePinnedToCore(detect_Task, "detect", 4 * 1024, (void *)afe_data, 5, NULL, 1);
      ESP_LOGI("AppWake", "Wake word detection is active");
      active = true;
    }
  };
}
esp_afe_sr_iface_t *app::AppWake::afe_handle = nullptr;
esp_afe_sr_data_t *app::AppWake::afe_data = nullptr;
afe_config_t *app::AppWake::afe_config = nullptr;
srmodel_list_t *app::AppWake::models = nullptr;
volatile int app::AppWake::task_flag = 0;
int app::AppWake::count = 0;
bool app::AppWake::active = false;