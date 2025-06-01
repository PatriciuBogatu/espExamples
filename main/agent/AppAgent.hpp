#pragma once

#include "ml/AppWake.hpp"
#include "voice/AppRecorder.hpp"

namespace app
{

  class AppAgent
  {
  public:
    static AppRecorder *appRecorder;
    static AppWake *appWake;

    static void launch_agent(void *arg)
    {
      appRecorder = new AppRecorder();
      appWake = new AppWake();

      
      appWake->init();

      while (true)
      {
        if (appRecorder->checkRecordingFinished() && !appWake->isActive())
        {
          AppRecorder::setRecordingFinishedToFalse();
          appWake->init();
          vTaskDelay(pdMS_TO_TICKS(50));
        }

        else if (!appWake->isActive() && !AppRecorder::checkIsRecording())
        {
          ESP_LOGI("AppAgent", "Starting recording...");
          appRecorder->start_recording();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
      }

      vTaskSuspend(NULL);
    }

    void launchApp()
    {
      xTaskCreate(launch_agent, "launch_agent", 4 * 1024, NULL, 5, NULL);
    }
  };

  // Static members initialization
  AppRecorder *AppAgent::appRecorder = nullptr;
  AppWake *AppAgent::appWake = nullptr;

} // namespace app