idf_component_register(
    SRCS
      "audio.cpp"
      "sensor/SensorApp.hpp"
      "sensor/bme280_defs.h"
      "sensor/bme280-new-i2c.hpp"
      "sensor/bme280.c"
      "sensor/bme280.h"
      "sensor/esp32-i2c-new-driver-bme280.cpp"
      #"AppWifi.hpp"
      #"AppMqtt.hpp"
      #"http/AppOTA.hpp"
      #"http/json.hpp"
      #"http/HttpServer.hpp"
      #"http/HttpClient.hpp"
      "http/rainmaker/ota/AppDriver.hpp"
      "http/rainmaker/ota/AppNode.hpp"
      "ui/ui.c"
      "ui/ui_helpers.c"
      "ui/ui_events.c"
      "ui/components/ui_comp_hook.c"
      "ui/screens/ui_Screen1.c"
    REQUIRES esp_wifi wifi_provisioning nvs_flash mqtt esp_http_server esp_http_client app_update esp_https_ota esp_rainmaker app_network
    PRIV_REQUIRES spi_flash
    INCLUDE_DIRS
       "." "ui"
    EMBED_TXTFILES "http/server/cert.pem"
)