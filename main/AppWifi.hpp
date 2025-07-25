#pragma once

#include <functional>
#include <string>
#include <cstring>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"
#include "nvs_flash.h"
#include "qrcode.h"
#include "http/json.hpp"

#define PROV_SERVICE_NAME "patriciu"
#define PROV_POP "bogatu"
#define PROV_TRANSPORT_SOFTAP "softap"

namespace app
{
    using OnWifiConnected_f = std::function<void(esp_ip4_addr_t *)>;
    using OnWifiDisconnected_f = std::function<void(void)>;

    class AppWifi
    {
    protected:
        OnWifiConnected_f m_connected;
        OnWifiDisconnected_f m_disconnected;

        static void handleWifiEvent(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data);

        static void printQrCode(const char *transport_method); // declarations here, implementations below

    public:
        void init() // OnWifiConnected_f conn, OnWifiDisconnected_f disc
        {
            // m_connected = conn;
            // m_disconnected = disc;

            ESP_LOGI("AppWifi", "Initing wifi!");
            esp_err_t err = nvs_flash_init();
            if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
            {
                nvs_flash_erase();
                nvs_flash_init();
            }

            esp_netif_init();
            esp_event_loop_create_default();
            esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, handleWifiEvent, this);
            esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, handleWifiEvent, this);
            esp_netif_create_default_wifi_sta();
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            esp_wifi_init(&cfg);
        }

        void connect(void)
        {
            ESP_LOGI("AppWifi", "Connecting to wifi");
            const char *ssid = "victory";
            const char *password = "or death";

            wifi_config_t wifi_config;
            memset(&wifi_config, 0, sizeof(wifi_config));
            memcpy(wifi_config.sta.ssid, ssid, strlen(ssid));
            memcpy(wifi_config.sta.password, password, strlen(password));
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

            esp_wifi_set_mode(WIFI_MODE_STA);
            esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            esp_wifi_start();
            ESP_LOGI("AppWifi", "WiFi started in station mode");
        }

        // void connect(void)
        // {
        //     ESP_LOGI("AppWifi", "Connecting wifi!");
        //     wifi_prov_mgr_config_t config = {
        //         .scheme = wifi_prov_scheme_softap,
        //         .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,
        //     };
        //     wifi_prov_mgr_init(config);

        //     bool provisioned = false;
        //     wifi_prov_mgr_is_provisioned(&provisioned);

        //     if (provisioned)
        //     {
        //         wifi_prov_mgr_deinit();
        //         esp_wifi_set_mode(WIFI_MODE_STA);
        //         esp_wifi_start();
        //     }
        //     else
        //     {
        //         esp_netif_create_default_wifi_ap();
        //         wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, PROV_POP, PROV_SERVICE_NAME, nullptr);
        //         printQrCode(PROV_TRANSPORT_SOFTAP);
        //         wifi_prov_mgr_wait();
        //         wifi_prov_mgr_deinit();
        //     }
        // }
    };

    void AppWifi::handleWifiEvent(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
    {
        AppWifi *obj = reinterpret_cast<AppWifi *>(arg);
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
        {
            esp_wifi_connect();
        }
        else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            // obj->m_disconnected();
            vTaskDelay(pdMS_TO_TICKS(3000));
            esp_wifi_connect();
        }
        else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
        {
            ip_event_got_ip_t *event = reinterpret_cast<ip_event_got_ip_t *>(event_data);
            // obj->m_connected(&event->ip_info.ip);
        }
    }

    void AppWifi::printQrCode(const char *transport_method)
    {
        nlohmann::json payload{{"ver", "v1"},
                               {"name", PROV_SERVICE_NAME},
                               {"pop", PROV_POP},
                               {"transport", transport_method}};
        esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
        esp_qrcode_generate(&cfg, payload.dump().c_str());
    }

}