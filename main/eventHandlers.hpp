// #include "ui.h"
// #include <vector>
// #include <cstdio>
// #include "sensor/SensorApp.hpp"

// app::AppSensor sensor{};

// void avgClickedHandler(lv_event_t *e) {
//     // lv_obj_t *btn = lv_event_get_target(e);

//     // Example: Change button text on click
//     // lv_label_set_text(lv_obj_get_child(btn, 0), "Clicked!");
//     auto temperatures = sensor.temperatures;
//     double avg = 0;
//     int numbers = 0;
//     for (int num : temperatures) {
//         avg += num;
//         numbers++;
//     }

//     avg /= numbers;


//     char buffer[20]; // Allocate enough space for the number
//     sprintf(buffer, "%f", avg);  // Convert number to string format

//     lv_label_set_text(ui_sensorLabel, "buffer");
// }