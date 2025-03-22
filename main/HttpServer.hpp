#pragma once

#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "json.hpp"
#include "esp_http_server.h"
#include "sensor/SensorApp.hpp"

namespace app
{
    class AppRestServer
    {
    private:
 
        AppSensor *m_sensor;
        httpd_handle_t m_server = nullptr;

        static esp_err_t handleDataGet(httpd_req_t *req);

    public:
        
        AppRestServer(AppSensor *sensor): m_sensor(sensor){
        }

        void start(void)
        {
            httpd_config_t config = HTTPD_DEFAULT_CONFIG();
            config.stack_size = 6000;
        
            // Ensure the server starts successfully
            if (httpd_start(&m_server, &config) == ESP_OK)
            {
                httpd_uri_t data_ep = {
                    .uri = "/data",
                    .method = HTTP_GET,
                    .handler = handleDataGet,
                    .user_ctx = this};
        
                if (httpd_register_uri_handler(m_server, &data_ep) != ESP_OK)
                {
                    ESP_LOGE("AppRestServer", "Failed to register URI handler for /data");
                }
                else
                {
                    ESP_LOGI("AppRestServer", "Successfully registered URI handler for /data");
                }
            }
            else
            {
                ESP_LOGE("AppRestServer", "Failed to start the HTTP server");
            }
        }

        void stop(void)
        {
            httpd_stop(m_server);
            m_server = nullptr;
        }
    };

    esp_err_t AppRestServer::handleDataGet(httpd_req_t *req)
    {
        AppRestServer *obj = (AppRestServer *)(req->user_ctx);
 
        std::string data_str = obj->m_sensor->getReadingsMQTT();
        httpd_resp_send(req, data_str.c_str(), data_str.length());
        
        return ESP_OK;
    }

}