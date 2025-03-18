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
#include "AppMqtt.hpp"



namespace
{
    app::AppWifi app_wifi;
    app::AppSensor app_sensor{true};
    app::AppMqtt app_mqtt{&app_sensor};
}


extern "C" void app_main(void)
{
    // bsp_spiffs_mount();

    /* Initialize I2C (for touch and audio) */
    bsp_i2c_init();
        
    /* Initialize display and LVGL */
    bsp_display_start();

    /* Set default display brightness */
    bsp_display_brightness_set(50);


    // 4. Create/load your SquareLine GUI within a lock
    lvgl_port_lock(0);
    ui_init();         // This is from your SquareLine export
    lvgl_port_unlock();

    // lvgl_port_lock(0);
    // lv_obj_t * label = lv_label_create(lv_scr_act());
    // lv_label_set_text(label, "Hello from LVGL!");
    // lv_obj_center(label);
    // lvgl_port_unlock();

    // 5. Done! The esp_lvgl_port takes care of the LVGL task loop in the background.

    // If you have other tasks or logic, they can live here or in separate freertos tasks.
    // Just remember: whenever you directly call any LVGL function (like creating new widgets),
    // wrap it with lvgl_port_lock() / lvgl_port_unlock().




    auto wifi_connected = [](esp_ip4_addr_t *ip)
    {
        ESP_LOGI(TAG, "wifi connected");
        app_mqtt.start();
    };

    auto wifi_disconnected = []()
    {
        ESP_LOGW(TAG, "wifi disconnected");
    };

    auto mqtt_cb = [](app::MqttEventData_t event)
    {
        switch (event.id)
        {
        case MQTT_EVENT_ERROR:
            ESP_LOGW(TAG, "mqtt error");
            break;
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "mqtt connected");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "mqtt disconnected");
            break;
        default:
            break;
        }
    };


    vTaskDelay(pdMS_TO_TICKS(2000));
    
    app_sensor.init();

    app_mqtt.init(mqtt_cb);
    app_wifi.init(wifi_connected, wifi_disconnected);
    app_wifi.connect();

//     while(true) {
//         app::SensorReading reading = sensor.read();
//         ESP_LOGI("[SENSOR READING]", "%f %f %f", reading.temperature, reading.humidity, reading.pressure);
//         vTaskDelay(pdMS_TO_TICKS(2000));
//     }
// }
}