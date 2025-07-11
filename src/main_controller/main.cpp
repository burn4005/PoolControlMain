#include "pool_controller.h"

static const char *TAG = "POOL_CONTROLLER";

// Global variables
system_config_t g_config;
system_state_t g_state;


extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Pool Controller Starting...");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize system configuration
    ESP_ERROR_CHECK(system_config_init());
    
    // Initialize pool controller
    pool_controller_init();
    
    // Create main pool controller task
    xTaskCreate(pool_controller_task, "pool_controller", 8192, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Pool Controller Started Successfully");
}
