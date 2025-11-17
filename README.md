# ESP-IDF Improv Server

Implements some parts of [Improv Wifi](https://www.improv-wifi.com/). Uses NimBLE, 
so you need to build with it (`CONFIG_BT_ENABLED=y`, `CONFIG_BT_NIMBLE_ENABLED=y`).
Does not try to co-exist with anything, so mainly for applications that use WiFi
primarily. Also the library is designed to keep accepting new WiFi provisioning.

## Usage

```cpp
#include "esp_improv.h"

static esp_err_t start_wifi(const char *ssid, const char *password)
{
    ESP_LOGI("MyApp", "SSID: %s Password: %s", ssid, password);
    return ESP_OK;
}

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    improvserver::ImprovServer server("BtName", "Manufacturer", "Model");
    ESP_ERROR_CHECK(server.Initialize(&start_wifi));
    ESP_ERROR_CHECK(server.StartAdvertising());
}
```