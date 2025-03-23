#pragma once

#include <esp_log.h>
#include <nvs_flash.h>
#include <app_wifi.h>
// the idf wifi lib is esp_wifi.h

namespace app
{
    class AppDriver
    {
    public:
        void init()
        {
            esp_err_t err = nvs_flash_init();
            if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
            {
                nvs_flash_erase();
                nvs_flash_init();
            }

            // rainmaker wifi lib to initialize wifi stack
            app_wifi_init();
        }

        void start()
        {
            // also from rainmaker wifi lib(with provisioning and qr logic)
            app_wifi_start(POP_TYPE_RANDOM);
        }
    };
}