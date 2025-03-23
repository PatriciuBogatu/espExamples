#pragma once

#include <cinttypes>
#include <map>
#include <string>
#include <esp_log.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_ota.h>
#include <esp_rmaker_common_events.h>
#include <esp_rmaker_utils.h>
// useful for seeing what is going with the node after is was deployed
// so we can track the functionality
#include <app_insights.h>

namespace app
{
    class AppNode
    {
    public:
        bool m_connected{false};
    private:
        // node - container from Rainmaker platform that can contain one or mode devices
        // a node definition can include multiple device definitions, e.g a node can integrate a light sensor and a bulb on the same hardware
        esp_rmaker_node_t *m_rmaker_node;
        // a sensor for example
        // a device definition can include multiple parameter definitions, e.g a multisensor device showing temperature, humidity, pressure
        // for a light sensor, you would have a single node with a single device with a single param
        esp_rmaker_device_t *m_device;

        esp_rmaker_param_t *m_temp_param;

        // is the node connected to the rainmaker mqtt service
        // flag whether we push readings to an MQTT topic or not

        static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    public:
        void init()
        {
            esp_event_handler_register(RMAKER_EVENT, ESP_EVENT_ANY_ID, &event_handler, this);
            esp_event_handler_register(RMAKER_COMMON_EVENT, ESP_EVENT_ANY_ID, &event_handler, this);
            esp_event_handler_register(RMAKER_OTA_EVENT, ESP_EVENT_ANY_ID, &event_handler, this);

            // we want to associate readings with timestamps, first enable a timezone
            esp_rmaker_time_set_timezone(CONFIG_ESP_RMAKER_DEF_TIMEZONE);

            // enable sntp in esp32
            esp_rmaker_config_t rainmaker_cfg = {
                .enable_time_sync = true,
            };
            m_rmaker_node = esp_rmaker_node_init(&rainmaker_cfg, "A temp-sensor node", "TempSensorNode");

            /*RainMaker has a predefined list of devices but, basically, you
can define anything as a device type*/
            m_device = esp_rmaker_device_create("TempSensorDevice", ESP_RMAKER_DEVICE_TEMP_SENSOR, nullptr);
            // the name from gui that can be changed
            m_temp_param = esp_rmaker_param_create("TempParam", "app.tempparam", esp_rmaker_int(0), PROP_FLAG_READ | PROP_FLAG_TIME_SERIES);
            esp_rmaker_param_add_ui_type(m_temp_param, ESP_RMAKER_UI_TEXT);
            esp_rmaker_device_add_param(m_device, m_temp_param);
            esp_rmaker_device_assign_primary_param(m_device, m_temp_param);
            // linking the device and the node
            esp_rmaker_node_add_device(m_rmaker_node, m_device);

            // the node will report its status to the backend
            app_insights_enable();

            /*The configuration includes the certificate of the OTA server running on the RainMaker platform. This
certificate comes with the RainMaker library and is embedded into the flash image at compile
time*/
            esp_rmaker_ota_config_t ota_config = {
                .server_cert = ESP_RMAKER_OTA_DEFAULT_SERVER_CERT,
            };
            esp_rmaker_ota_enable(&ota_config, OTA_USING_PARAMS);
        }

        void start()
        {
            esp_rmaker_start();
        }


        void update(uint32_t tempLevel){

            if(m_connected){
                esp_rmaker_param_update_and_report(m_temp_param, esp_rmaker_int(tempLevel));
            }
        }
    };

    void AppNode::event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        AppNode *obj = reinterpret_cast<AppNode *>(arg);

        if (event_base == RMAKER_EVENT)
        {
            switch (event_id)
            {
            case RMAKER_EVENT_INIT_DONE:
                ESP_LOGI(__func__, "RainMaker Initialised.");
                break;
            case RMAKER_EVENT_CLAIM_STARTED:
                ESP_LOGI(__func__, "RainMaker Claim Started.");
                break;
            case RMAKER_EVENT_CLAIM_SUCCESSFUL:
                ESP_LOGI(__func__, "RainMaker Claim Successful.");
                break;
            case RMAKER_EVENT_CLAIM_FAILED:
                ESP_LOGI(__func__, "RainMaker Claim Failed.");
                break;
            default:
                ESP_LOGW(__func__, "Unhandled RainMaker Event: %" PRIi32, event_id);
            }
        }
        else if(event_base == RMAKER_COMMON_EVENT){
            switch(event_id){
                case RMAKER_MQTT_EVENT_CONNECTED:
                    obj->m_connected = true;
                    break;
                case RMAKER_MQTT_EVENT_DISCONNECTED:
                    obj->m_connected = false;
                    break;
            }
        }

        else if (event_base == RMAKER_OTA_EVENT)
        {
            switch (event_id)
            {
            case RMAKER_OTA_EVENT_STARTING:
                ESP_LOGI(__func__, "Starting OTA.");
                break;
            case RMAKER_OTA_EVENT_IN_PROGRESS:
                ESP_LOGI(__func__, "OTA is in progress.");
                break;
            case RMAKER_OTA_EVENT_SUCCESSFUL:
                ESP_LOGI(__func__, "OTA successful.");
                break;
            case RMAKER_OTA_EVENT_FAILED:
                ESP_LOGI(__func__, "OTA Failed.");
                break;
            case RMAKER_OTA_EVENT_REJECTED:
                ESP_LOGI(__func__, "OTA Rejected.");
                break;
            case RMAKER_OTA_EVENT_DELAYED:
                ESP_LOGI(__func__, "OTA Delayed.");
                break;
            case RMAKER_OTA_EVENT_REQ_FOR_REBOOT:
                ESP_LOGI(__func__, "Firmware image downloaded. Please reboot your device to apply the upgrade.");
                break;
            default:
                ESP_LOGW(__func__, "Unhandled OTA Event: %" PRIi32, event_id);
                break;
            }
        }
    }
}