#pragma once

#include <functional>
#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "sdkconfig.h"
#include "sensor/SensorApp.hpp"

namespace app
{
    typedef struct
    {
        esp_mqtt_event_id_t id;
        void *data;
    } MqttEventData_t;

    using MqttEventCb_f = std::function<void(MqttEventData_t)>;

    // revisit later in the mqtt section how the author made them configurable
    char* MQTT_USER = "";
    char* MQTT_PWD = "";

    // reminder, that you commented some lines because they are related to the button logic from the chapter

    class AppMqtt
    {
    private:
        AppSensor *m_sensor;
        esp_mqtt_client_handle_t m_client = nullptr;
        std::string m_sensor_topic;
        TaskHandle_t m_publish_handle = nullptr;
        MqttEventCb_f m_event_cb;

        void handleMqttData(esp_mqtt_event_handle_t event);

        static void mqttEventHandler(void *arg,
                                     esp_event_base_t base,
                                     int32_t event_id,
                                     void *event_data);

        static void publishSensorState(void *param);

    public:
        AppMqtt(AppSensor *sensor) : m_sensor(sensor), m_sensor_topic(sensor->getName() + "/readings")
        {
        }

        void init(MqttEventCb_f cb)
        {
            m_event_cb = cb;
            esp_mqtt_client_config_t mqtt_cfg = {};

            // Set broker address
            std::string broker_uri = "mqtt://" + std::string(CONFIG_MQTT_BROKER_IP) + ":" + std::to_string(CONFIG_MQTT_PORT);

            ESP_LOGI("AppMqtt", "Broker_uri: %s", broker_uri.c_str());
            ESP_LOGI("AppMqtt", "Broker username: %s", MQTT_USER);
            ESP_LOGI("AppMqtt", "Broker password: %s", MQTT_PWD);

            mqtt_cfg.broker.address.uri = broker_uri.c_str();
        
            // Set credentials
            mqtt_cfg.credentials.client_id = m_sensor->getName().c_str();
            mqtt_cfg.credentials.username = MQTT_USER;
            mqtt_cfg.credentials.authentication.password = MQTT_PWD;
            mqtt_cfg.network.timeout_ms = 10000;  // 10-second timeout
            mqtt_cfg.network.reconnect_timeout_ms = 5000;

            m_client = esp_mqtt_client_init(&mqtt_cfg);
            esp_mqtt_client_register_event(m_client, MQTT_EVENT_ANY, mqttEventHandler, this);
            xTaskCreate(publishSensorState, "publish", 4096, this, 5, &m_publish_handle);
            vTaskSuspend(m_publish_handle);   
        }

        void start(void)
        {
            esp_mqtt_client_start(m_client);
        }
    }; // end class

    void AppMqtt::mqttEventHandler(void *arg,
                                   esp_event_base_t base,
                                   int32_t event_id,
                                   void *event_data)
    {
        AppMqtt *obj = reinterpret_cast<AppMqtt *>(arg);

        switch (static_cast<esp_mqtt_event_id_t>(event_id))
        {
        case MQTT_EVENT_CONNECTED:
            esp_mqtt_client_subscribe(obj->m_client, obj->m_sensor_topic.c_str(), 1);
            vTaskResume(obj->m_publish_handle);
            break;
        case MQTT_EVENT_DATA:
            obj->handleMqttData(reinterpret_cast<esp_mqtt_event_handle_t>(event_data));
            break;
        case MQTT_EVENT_ERROR: {
            esp_mqtt_event_handle_t event = reinterpret_cast<esp_mqtt_event_handle_t>(event_data);
            ESP_LOGE("MQTT", "Error: type=%d, err=0x%x", 
                    event->error_handle->error_type,
                    event->error_handle->esp_transport_sock_errno);
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE("MQTT", "Last errno: %s", 
                        esp_err_to_name(event->error_handle->esp_transport_sock_errno));
            }
            break;
        }
        case MQTT_EVENT_DISCONNECTED:
            vTaskSuspend(obj->m_publish_handle);
            break;
        default:
            break;
        }
        obj->m_event_cb({static_cast<esp_mqtt_event_id_t>(event_id), event_data});
    }

    
    void AppMqtt::handleMqttData(esp_mqtt_event_handle_t event)
    {
        std::string data{event->data, (size_t)event->data_len};
        
        ESP_LOGI("APP_MQTT", "handleMqttData %s", data.c_str());
    }


    void AppMqtt::publishSensorState(void *param)
    {
        AppMqtt *obj = reinterpret_cast<AppMqtt *>(param);

        while (true)
        {
            vTaskDelay(pdMS_TO_TICKS(3000));
           
            std::string state_text = obj->m_sensor->getReadingsMQTT();
            esp_mqtt_client_publish(obj->m_client, obj->m_sensor_topic.c_str(), state_text.c_str(), state_text.length(), 1, 0);
            
        }
    }
}
