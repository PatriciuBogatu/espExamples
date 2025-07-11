#include <stdio.h>
#include "esp_system.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
// SquareLine–exported header:
#include "ui.h"
#include "sensor/SensorApp.hpp"
#include "AppWifi.hpp"
#include "agent/AppAgent.hpp"
#include "esp_heap_caps.h"
// #include "AppMqtt.hpp"
// #include "http/HttpServer.hpp"
// #include "http/HttpClient.hpp"
// #include "http/AppOTA.hpp"
// #include "http/rainmaker/ota/AppNode.hpp"
// #include "http/rainmaker/ota/AppDriver.hpp"
// Audio configuration

namespace
{
    app::AppWifi app_wifi;
    // app::AppMqtt app_mqtt{&app_sensor};
    // app::AppRestServer app_http_server{&app_sensor};
    // app::AppRestClient app_rest_client;
    // app::AppOtaClient app_ota_client;

    // app::AppDriver app_driver;
    // app::AppNode app_node;
    app::AppAgent app_agent;
    // app::AppSensor app_sensor{true, &app_node};
}

extern "C" void app_main(void)
{
// Initialize microphone codec
#if CONFIG_SPIRAM_SUPPORT
ESP_LOGI("MEM", "INTO MAIN");
    ESP_LOGI("MEM", "SPIRAM detected, free: %zu", 
            heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
#else
    ESP_LOGE("MEM", "SPIRAM NOT CONFIGURED!");
#endif
bsp_spiffs_mount();



    // bsp_i2c_init();

    /* Initialize display and LVGL */
    // bsp_display_start();

    /* Set default display brightness */
    // bsp_display_brightness_set(50);

    // 4. Create/load your SquareLine GUI within a lock
    // lvgl_port_lock(0);
    // ui_init(); // This is from your SquareLine export
    // lvgl_port_unlock();

    // 5. Done! The esp_lvgl_port takes care of the LVGL task loop in the background.

    // If you have other tasks or logic, they can live here or in separate freertos tasks.
    // Just remember: whenever you directly call any LVGL function (like creating new widgets),
    // wrap it with lvgl_port_lock() / lvgl_port_unlock().

    // auto wifi_connected = [](esp_ip4_addr_t *ip)
    // {
    //     ESP_LOGI(TAG, "wifi connected!");
    //     // app_mqtt.start();
    //     // app_rest_client.start();
    //     app_ota_client.start();
    // };

    // auto wifi_disconnected = []()
    // {
    //     ESP_LOGW(TAG, "wifi disconnected");
    //     // app_rest_client.pause();
    //     if (!app_ota_client.isOtaDone())
    //     {
    //         app_ota_client.pause();
    //     }

    //     // if the ota is done then there is nothing to pause because the underlying FreeRtos task is deleted
    // };

    // auto mqtt_cb = [](app::MqttEventData_t event)
    // {
    //     switch (event.id)
    //     {
    //     case MQTT_EVENT_ERROR:
    //         ESP_LOGW(TAG, "mqtt error");
    //         break;
    //     case MQTT_EVENT_CONNECTED:
    //         ESP_LOGI(TAG, "mqtt connected");
    //         break;
    //     case MQTT_EVENT_DISCONNECTED:
    //         ESP_LOGW(TAG, "mqtt disconnected");
    //         break;
    //     default:
    //         break;
    //     }
    // };

    // vTaskDelay(pdMS_TO_TICKS(2000));

    // app_sensor.init();

    // app_mqtt.init(mqtt_cb);
    // app_rest_client.init();
    // app_ota_client.init();
    // vTaskDelay(pdMS_TO_TICKS(7000));
    
    // reset init
    // halt
    // flash erase_address 0x0 0x1000000
    // resume
    // exit
    app_wifi.init(); // wifi_connected, wifi_disconnected
    app_wifi.connect();
    vTaskDelay(pdMS_TO_TICKS(10000));
    ESP_LOGI("MAIN", "Launching Agent");
    app_agent.launchApp();
    
    
    
    
    
    // app_http_server.start(); // no need to init - this is esp's own resource

    // Initializez the underlying flash/NVS access and Wifi
    // app_driver.init();
    // // Configures Rainmaker
    // app_node.init();

    // Will allow Rainmaker task to monitor Wifi events and react to them
    // app_node.start();
    // // Starting Wifi
    // app_driver.start();

    // while (1)
    // {

    //     vTaskDelay(pdMS_TO_TICKS(2000));

    //     app::SensorReading reading = app_sensor.read();
    //     ESP_LOGI("[SENSOR READING]", "%f %f %f", reading.temperature, reading.humidity, reading.pressure);
    // if(app_ota_client.isOtaDone()){
    //     // reboot, teh bootloader will find the new firmware and activate it
    //     esp_restart();
    // }
    // }
    //     while(true) {
    //         app::SensorReading reading = sensor.read();
    //         ESP_LOGI("[SENSOR READING]", "%f %f %f", reading.temperature, reading.humidity, reading.pressure);
    //         vTaskDelay(pdMS_TO_TICKS(2000));
    //     }
    // }
}