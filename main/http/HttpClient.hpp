#pragma once

#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "json.hpp"
#include "esp_http_client.h"
#include "ui.h"
#include "esp_err.h"
#include "esp_log.h"



namespace app
{


    class AppRestClient
    {
    private:
        bool m_toggle_btn;
        TaskHandle_t m_publish_task;

        static esp_err_t handleHttpEvents(esp_http_client_event_t *evt);
        static void publishSensorState(void *arg);

    public:
        void init(void)
        {
            xTaskCreate(publishSensorState, "publish", 4096, this, 5, &m_publish_task);
            vTaskSuspend(m_publish_task);
        }

        void start(void)
        {
            vTaskResume(m_publish_task);
        }

        void pause(void)
        {
            vTaskSuspend(m_publish_task);
        }
    };

    void AppRestClient::publishSensorState(void *arg)
    {
        AppRestClient *obj = reinterpret_cast<AppRestClient *>(arg);
        std::string host{std::string("http://" CONFIG_REST_SERVER_IP ":") + std::string(CONFIG_REST_SERVER_PORT)};
        std::string health_url{host + "/health"};

        static char buffer[1024];

        while (true)
        {
            vTaskDelay(pdMS_TO_TICKS(3000));

            esp_http_client_config_t client_config = {
                .url = health_url.c_str(),
                .method = HTTP_METHOD_GET,
                .event_handler = handleHttpEvents,
                .transport_type = HTTP_TRANSPORT_OVER_TCP,
                .user_data = buffer};

            esp_http_client_handle_t client = esp_http_client_init(&client_config);
            esp_http_client_perform(client);
            ESP_LOGI("HTTP_CLIENT", "Response: %s", buffer);


            // sSensorConfig sensor_config = nlohmann::json::parse(buffer);
            
            // esp_http_client_set_url(client, data_url.c_str());
            // esp_http_client_set_method(client, HTTP_METHOD_PUT);
            // esp_http_client_set_header(client, "Content-Type", "application/json");

    
            // nlohmann::json data_json = data;
            // std::string data_str = data_json.dump(); // data what? define it
            // esp_http_client_set_post_field(client, data_str.c_str(), data_str.length());
            // esp_http_client_perform(client);
            

            esp_http_client_cleanup(client);
        }
    }

    esp_err_t AppRestClient::handleHttpEvents(esp_http_client_event_t *evt)
    {
        switch (evt->event_id)
        {
        case HTTP_EVENT_ON_DATA:
            memcpy(evt->user_data, evt->data, evt->data_len);
        default:
            break;
        }
        return ESP_OK;
    }


}