idf_component_register(
    SRCS
      "audio.cpp"
      "eventHandlers.hpp"
      "sensor/SensorApp.hpp"
      "AppWifi.hpp"
      "AppMqtt.hpp"
      "json.hpp"
      "HttpServer.hpp"
      "HttpClient.hpp"
      "ui/ui.c"
      "ui/ui_helpers.c"
      "ui/ui_events.c"
      "ui/components/ui_comp_hook.c"
      "ui/screens/ui_Screen1.c"
      "../components/esp32-i2c-new-driver-bme280/bme280_defs.h"
      "../components/esp32-i2c-new-driver-bme280/bme280-new-i2c.hpp"
      "../components/esp32-i2c-new-driver-bme280/bme280.c"
      "../components/esp32-i2c-new-driver-bme280/bme280.h"
      "../components/esp32-i2c-new-driver-bme280/esp32-i2c-new-driver-bme280.cpp"
    REQUIRES esp_wifi wifi_provisioning nvs_flash mqtt esp_http_server esp_http_client
    PRIV_REQUIRES spi_flash
    INCLUDE_DIRS
       "." "ui" "../components/esp32-i2c-new-driver-bme280"
)