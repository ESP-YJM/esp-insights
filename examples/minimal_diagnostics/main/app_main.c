/* Minimal Diagnostics Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "string.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_insights.h"
#include "esp_diagnostics_system_metrics.h"
#include "esp_rmaker_utils.h"
#include "app_wifi.h"

/* The examples uses configuration that you can set via project configuration menu

    If you'd rather not, just change the below entries to strings with
    the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"

    Default values:
        CONFIG_ESP_WIFI_SSID               : "myssid"
        CONFIG_ESP_WIFI_PASSWORD           : "mypassword"
        CONFIG_CLOUD_REPORTING_PERIOD_HR   : one hour
*/
#define EXAMPLE_ESP_WIFI_SSID               CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS               CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_CLOUD_REPORTING_PERIOD      (CONFIG_CLOUD_REPORTING_PERIOD_HR * 3600)

#if CONFIG_DIAG_ENABLE_LOG_TYPE_ALL
#define EXAMPLE_DIAG_LOG_TYPE               ESP_DIAG_LOG_TYPE_ERROR \
                                            | ESP_DIAG_LOG_TYPE_WARNING \
                                            | ESP_DIAG_LOG_TYPE_EVENT
#else
#define EXAMPLE_DIAG_LOG_TYPE               0
#endif /* CONFIG_DIAG_ENABLE_LOG_TYPE_ALL */

static const char *TAG = "minimal_diag";

void app_main(void)
{
    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = app_wifi_sta_init(EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi, err:0x%x", ret);
    }
    ESP_ERROR_CHECK(ret);

    /* This initializes SNTP for time synchronization.
     * ESP Insights uses relative time since bootup if time is not synchronized and
     * epoch since 1970 if time is synsynchronized.
     */
    esp_rmaker_time_sync_init(NULL);

    esp_insights_config_t config = {
        .log_type = EXAMPLE_DIAG_LOG_TYPE,
        .cloud_reporting_period = EXAMPLE_CLOUD_REPORTING_PERIOD,
    };
    ret = esp_insights_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ESP Insights, err:0x%x", ret);
    }
    ESP_ERROR_CHECK(ret);

    /* Following code generates an example error and logs it */
    nvs_handle_t handle;
    ret = nvs_open("unknown", NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Test error: API nvs_open() failed, error:0x%x", ret);
    }

#if CONFIG_DIAG_ENABLE_HEAP_METRICS
    /* Dump heap metrics */
    esp_diag_heap_metrics_dump();
#endif /* CONFIG_DIAG_ENABLE_HEAP_METRICS */
}
