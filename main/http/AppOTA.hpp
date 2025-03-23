#pragma once

#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "sdkconfig.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_log.h"

#define FILE_SERVER_URL "https://" CONFIG_REST_SERVER_IP ":" CONFIG_REST_SERVER_PORT

// marks the start of the address in the memory of the tls certificate of the server
// we instructed the build system to store the tls cert in the firmware binary
extern const uint8_t server_cert_pem_start[] asm("_binary_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_cert_pem_end");

namespace app
{
    // this class encapsulates an etire secure OTA process
    class AppOtaClient
    {
    private:

        // job to poll the server for any new updates and install if required
        TaskHandle_t m_ota_task;
        // the variable will hold the rtos task
        static void otaTaskFunc(void *arg);

        // GET - returns firmware filename
        static constexpr char *INFO_ENDPOINT{FILE_SERVER_URL "/info"};
        // this will be used for concatenation to create the download URL, this + the filename returned by /info
        static constexpr char *FILE_SERVER_STATIC{FILE_SERVER_URL "/static/"};

        // queries the server to get the filename
        std::string getInfo(char *buffer, size_t buffer_len);

        // takes the filename and does the update if the server holds a newer firmware
        void doOtaUpdate(char *buffer, size_t buffer_len, const std::string &filename);
        static esp_err_t handleHttpEvents(esp_http_client_event_t *evt);

        bool m_ota_done;

    public:
        void init(void)
        {
            m_ota_done = false;
            // we pass the this pointer to xTaskCreate as a parameter, so that we can have access to the class instance at runtime when the task starts
            xTaskCreate(otaTaskFunc, "ota", 8192, this, 5, &m_ota_task);
            // we susped immediately because we need a wifi connection first
            vTaskSuspend(m_ota_task);
        }

        void start(void)
        {
            vTaskResume(m_ota_task);
        }

        void pause(void)
        {
            vTaskSuspend(m_ota_task);
        }
        
        // ota download is finished?
        bool isOtaDone(void) const
        {   
            return m_ota_done;
        }
    };


    /*
otaTaskFunc is a static function for the FreeRTOS task. However, we can access the AppOtaClient 
object, which is created at runtime, by casting back the arg argument to an object pointer.*/
    void AppOtaClient::otaTaskFunc(void *arg)
    {
        AppOtaClient *obj = reinterpret_cast<AppOtaClient *>(arg);

        // placeholder for the incoming data from the server
        static char buffer[8192];

        while (true)
        {
            vTaskDelay(pdMS_TO_TICKS(10000));

            std::string filename = obj->getInfo(buffer, sizeof(buffer));
            if (!filename.empty())
            {
                obj->doOtaUpdate(buffer, sizeof(buffer), filename);
            }
        }
    }

    std::string AppOtaClient::getInfo(char *buffer, size_t buffer_len)
    {   
        // first clear the buffer for storing incoming data from the server
        memset(buffer, 0, buffer_len);
        
        // config how we connect to the server securely
        esp_http_client_config_t client_config = {
            .url = INFO_ENDPOINT,
            .cert_pem = (const char *)server_cert_pem_start, // used to establish the secure connection
            .method = HTTP_METHOD_GET,
            .event_handler = handleHttpEvents,
            .transport_type = HTTP_TRANSPORT_OVER_SSL, // specifying the tls?
            .user_data = buffer, // here we just do the mapping for the buffer
            .skip_cert_common_name_check = true};

        esp_http_client_handle_t client = esp_http_client_init(&client_config);

        std::string filename{""};
        if (esp_http_client_perform(client) == ESP_OK)
        {
            filename = std::string(buffer);
        }
        esp_http_client_cleanup(client);

        return filename;
    }

    esp_err_t AppOtaClient::handleHttpEvents(esp_http_client_event_t *evt)
    {
        switch (evt->event_id)
        {
        case HTTP_EVENT_ON_DATA:
            memcpy(evt->user_data, evt->data, evt->data_len); // here the copying of the response body to the buffer is happening
        default:
            break;
        }
        return ESP_OK;
    }

    void AppOtaClient::doOtaUpdate(char *buffer, size_t buffer_len, const std::string &filename)
    {
        std::string file_url{std::string(FILE_SERVER_STATIC) + filename};

        esp_http_client_config_t client_config = {
            .url = file_url.c_str(),
            .cert_pem = (const char *)server_cert_pem_start,
            .method = HTTP_METHOD_GET,
            .event_handler = nullptr, // no need for event handler this time - the data will be habdled by the OTA update lib
            .transport_type = HTTP_TRANSPORT_OVER_SSL,
            .user_data = buffer,
            .skip_cert_common_name_check = true};
        
        // wrapper of the client config
        esp_https_ota_config_t ota_config = {
            .http_config = &client_config,
            .partial_http_download = true,
            .max_http_request_size = (int)buffer_len}; // the firmware is too big to download it in a single http get req
        
        // we have 2 partitions for firmware
        // one holds the current running hardware and the other is reserved for the OTA update
        const esp_partition_t *running = esp_ota_get_running_partition();
        esp_app_desc_t running_app_info; // will hold the description with current version of the app running
        esp_ota_get_partition_description(running, &running_app_info);
        ESP_LOGI("AppOTA", "Current version is %s", running_app_info.version);

        esp_https_ota_handle_t https_ota_handle{nullptr};
        // initialize
        esp_https_ota_begin(&ota_config, &https_ota_handle);

        /*Every ESP32 application has description data at the beginning of its firmware file. The esp_https_
 ota_get_img_desc function exploits this property of firmware files by only requesting the header(that part) 
part of the firmware from the remote server.*/
        esp_app_desc_t app_desc;
        // if the response is not ok or the versions are the same then abort the update
        if (esp_https_ota_get_img_desc(https_ota_handle, &app_desc) != ESP_OK ||
            memcmp(app_desc.version, running_app_info.version, sizeof(app_desc.version)) == 0)
        {
                // is the image header an actual http header? so the ota lib finds out the version from the header instead from the initial downloaded bin chunk?
                ESP_LOGI("AppOTA", "Target version is %s", app_desc.version);
            esp_https_ota_abort(https_ota_handle);
            return;
        }

        // if it is newer - we download it
        int img_size = esp_https_ota_get_image_size(https_ota_handle);

        // downloads the firmware from the server
        while (esp_https_ota_perform(https_ota_handle) == ESP_ERR_HTTPS_OTA_IN_PROGRESS)
        {
            int read_size = esp_https_ota_get_image_len_read(https_ota_handle);
            ESP_LOGI(__func__, "%%%0.1f (bytes %d/%d)", ((float)read_size) * 100 / img_size, read_size, img_size);
        }

        // validate the downloaded data
        if (esp_https_ota_is_complete_data_received(https_ota_handle) != true)
        {
            esp_https_ota_abort(https_ota_handle);
        }
        else
        {   
            // mark the finish
            if (esp_https_ota_finish(https_ota_handle) == ESP_OK)
            {
                m_ota_done = true;
                vTaskDelete(nullptr);
            }
            else
            {
                esp_https_ota_abort(https_ota_handle);
            }
        }
    }

}