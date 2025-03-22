/*
 * File:    esp32-i2c-new-driver-bme280.c
 * Author:  Lukas Etschbacher
 * Date:    12.12.2024
 * Version: 1.0
 * Brief:   ESP32-based environmental monitoring system using the BME280 sensor.
 * Email:   etsch.work@sbg.at
 *
 * Description:
 * This file contains the implementation of an ESP32 application that interfaces 
 * with a BME280 sensor via I2C. It collects temperature, pressure, and humidity 
 * data and outputs them to the console. This work serves as an example of using 
 * the ESP-IDF framework for environmental sensing.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "bme280.h"
#include "bme280-new-i2c.hpp"


#define I2C_MASTER_SCL_IO 12       // GPIO for I2C SCL
#define I2C_MASTER_SDA_IO 11       // GPIO for I2C SDA
#define BME280_SENSOR_ADDR 0x76    // BME280 I2C address

// Global variable declarations
struct bme280_dev bme280;
struct bme280_data comp_data;
struct bme280_settings bme280_settings;

i2c_master_bus_config_t i2c_mst_config_1 = {
    .i2c_port = I2C_NUM_0,                   // I2C port (0 in this example)
    .sda_io_num = static_cast<gpio_num_t>(I2C_MASTER_SDA_IO),          // SDA pin
    .scl_io_num = static_cast<gpio_num_t>(I2C_MASTER_SCL_IO),           // SCL pin
    .clk_source = I2C_CLK_SRC_DEFAULT,         // Clock source
    // .glitch_ignore_cnt = 7,                    // Glitch filter count
    // .intr_priority = 0,                        // Use default interrupt priority (0 means driver selects default)
    // .trans_queue_depth = 10,                   // Set the transfer queue depth (adjust as needed)
    .flags = {
         .enable_internal_pullup = true,       // Enable internal pull-ups
         .allow_pd = false,                    // Set to false unless you need power down support
    },
};

i2c_master_bus_handle_t bus_handle;

i2c_device_config_t dev_cfg = {
    I2C_ADDR_BIT_LEN_7,
     BME280_SENSOR_ADDR,
    100000
};

i2c_master_dev_handle_t dev_handle;

/**
 * @brief Initializes the I2C master and configures the BME280 device.
 */
void BME280_i2c_master_init(void) {
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config_1, &bus_handle));
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_ERROR_CHECK(i2c_master_probe(bus_handle, BME280_SENSOR_ADDR, -1));
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));
    vTaskDelay(pdMS_TO_TICKS(2000));
}

/**
 * @brief Delay function for the BME280 library, adapted for FreeRTOS.
 * @param msek Time to delay in milliseconds.
 */
void BME280_delay_msek(uint32_t msek, void *intf_ptr) {
    vTaskDelay(msek / portTICK_PERIOD_MS);
}

/**
 * @brief I2C read function for the BME280 sensor.
 * @param reg_addr Register address to read from.
 * @param reg_data Pointer to the buffer to store data.
 * @param len Number of bytes to read.
 * @param intf_ptr Interface pointer (not used in this implementation).
 * @return int8_t Status code (0 for success).
 */
int8_t BME280_I2C_bus_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr) {
    int8_t result;
    uint8_t reg_addr_buf = reg_addr;

    result = i2c_master_transmit(dev_handle, &reg_addr_buf, 1, -1);
    if (result != 0) {
        return result;
    }
    return i2c_master_receive(dev_handle, reg_data, len, -1);
}

/**
 * @brief I2C write function for the BME280 sensor.
 * @param reg_addr Register address to write to.
 * @param reg_data Pointer to the data to write.
 * @param len Number of bytes to write.
 * @param intf_ptr Interface pointer (not used in this implementation).
 * @return int8_t Status code (0 for success).
 */
int8_t BME280_I2C_bus_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr) {
    uint8_t buffer[len + 1];
    buffer[0] = reg_addr;
    memcpy(&buffer[1], reg_data, len);

    return i2c_master_transmit(dev_handle, buffer, len + 1, -1);
}

/**
 * @brief Reads data from the BME280 sensor and prints it to the console.
 */


void initializeBME280(void){
    int8_t rslt;
    uint8_t settings_sel;

    bme280.intf = static_cast<bme280_intf>(1);
    bme280.read = BME280_I2C_bus_read;
    bme280.write = BME280_I2C_bus_write;
    bme280.delay_us = BME280_delay_msek;

    rslt = bme280_init(&bme280);
    if (rslt != BME280_OK) {
        ESP_LOGE("[BME INITIALIZATION]", "BME280 initialization failed. Error code: %d\n", rslt);
        return;
    }

    bme280_settings.osr_h = BME280_OVERSAMPLING_1X;
    bme280_settings.osr_p = BME280_OVERSAMPLING_16X;
    bme280_settings.osr_t = BME280_OVERSAMPLING_2X;
    bme280_settings.filter = BME280_FILTER_COEFF_16;
    bme280_settings.standby_time = BME280_STANDBY_TIME_0_5_MS;

    settings_sel = BME280_SEL_OSR_PRESS | BME280_SEL_OSR_TEMP |
                   BME280_SEL_OSR_HUM | BME280_SEL_FILTER | BME280_SEL_STANDBY;

    rslt = bme280_set_sensor_settings(settings_sel, &bme280_settings, &bme280);
    rslt = bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &bme280);

    vTaskDelay(1000 / portTICK_PERIOD_MS);
}


struct bme280_data BME280_I2C_read_data(void) {
    int8_t rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, &bme280);

    struct bme280_data reading;
    if (rslt == BME280_OK) {
        reading.pressure = comp_data.pressure / 100;
        reading.temperature = comp_data.temperature;
        reading.humidity = comp_data.humidity;
    }

    return reading;
}

/**
 * @brief Main application entry point.
 */
// void app_main(void) {
//     while (1) {
//         BME280_i2c_master_init();
//         BME280_I2C_read_data();
//         ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));
//         vTaskDelay(1000 / portTICK_PERIOD_MS);
//     }
// }