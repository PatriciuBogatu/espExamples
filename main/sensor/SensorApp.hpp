#pragma once

#include "esp_log.h"
#include "bme280-new-i2c.hpp"
#include "bsp/esp-bsp.h"  // For BSP I2C handle
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include <vector>
#include <cstdio>
#include "ui.h"
#include "esp_err.h"
#include "esp_log.h"
#include <string>
#include "http/json.hpp"
#include "sdkconfig.h"
#include <stdio.h>
#include <stdlib.h>  // For rand() and srand()
#include <time.h>


static const char *TAG = "AppSensor";

std::vector<double> temperatures;

void avgClickedHandler(lv_event_t *e) {
    // lv_obj_t *btn = lv_event_get_target(e);

    // Example: Change button text on click
    // lv_label_set_text(lv_obj_get_child(btn, 0), "Clicked!");
    
    ESP_LOGI(TAG ,"Invoking avgClickedHandler");

    double avg = 0;
    int numbers = 0;
    for (double num : temperatures) {
        avg += num;
        numbers++;
    }

    avg /= numbers;


    char buffer[20]; // Allocate enough space for the number
    sprintf(buffer, "%f", avg);  // Convert number to string format

    lv_label_set_text(ui_avgSensor, buffer);
}


namespace app {


    struct SensorReading {
        float temperature;
        float pressure;
        float humidity;
    };


    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SensorReading, temperature, pressure, humidity);

    class AppSensor {
    private:
        // i2c_bus_handle_t i2c_bus_ = NULL;
        // bme280_handle_t  bme280 = NULL;
        // i2c_master_bus_handle_t i2c_handle;
        // bme280_handle_t bme280_handle = NULL;

        std::string m_sensor_name;
        nlohmann::json readings;
        bool m_enabled;

        bool isMock;

    public:

        AppSensor(bool isMock) : m_sensor_name(CONFIG_MQTT_CLIENT_IDENTIFIER)
        {
            readings["temparature"] = 0.0;
            readings["humidity"] = 0.0;
            readings["pressure"] = 0.0;
            this->isMock = isMock;
        }

        void init(void) {
            // const i2c_master_bus_config_t i2c_config = {
            //     .i2c_port = 0,
            //     .sda_io_num = GPIO_NUM_38,
            //     .scl_io_num = GPIO_NUM_39,
            //     .clk_source = I2C_CLK_SRC_DEFAULT,
            //     .glitch_ignore_cnt = 7,
            //     .intr_priority = 0,
            //     .trans_queue_depth = 0,
            //     .flags = 1
            // };

            // i2c_device_config_t bme280_conf = {
            //     .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            //     .device_address = 0x76,  // or 0x77
            //     .scl_speed_hz = 100000,
            // };

            // ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_config, &i2c_handle));

            // // Initialize BME280
            // bme280_default_init(bme280_handle);

            
            if(!isMock){
                BME280_i2c_master_init();
                initializeBME280();
            }
        }

        SensorReading read(void) { 
            SensorReading sensorReading;
            char buffer[50];

            if(isMock){
                srand(time(NULL));

                // Generate a random number
                int random_number = rand() % 30;
                sensorReading.humidity = random_number;
                sensorReading.temperature = random_number + 1;
            }else{
                struct bme280_data data = BME280_I2C_read_data();
                sensorReading.humidity = data.humidity;
                sensorReading.temperature = data.temperature;
                sensorReading.pressure = data.pressure;
            }
            
            sprintf(buffer, "Hum: %.3f Temp: %.3f", sensorReading.humidity, sensorReading.temperature); 
            lvgl_port_lock(0);
            lv_label_set_text(ui_sensorLabel, buffer);
            lvgl_port_unlock();
            temperatures.push_back(sensorReading.temperature);
            return sensorReading;
        }

        std::string getName(void) const { return m_sensor_name; }
        
        std::string getReadingsMQTT(void){ 
            
            SensorReading currentReading = read();
            readings["temparature"] = currentReading.temperature;
            readings["humidity"] = currentReading.humidity;
            readings["pressure"] = currentReading.pressure;

            return readings.dump(); 
        
        }


        // void setState(std::string new_state)
        // {
        //     nlohmann::json val = nlohmann::json::parse(new_state, nullptr, false);
        //     if (!val.is_discarded())
        //     {
        //         readings.merge_patch(val);
        //         m_enabled = m_state["enabled"].get<bool>();
        //         handleNewState();
        //     }
        // }

    };
}