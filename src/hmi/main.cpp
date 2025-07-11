#include "hmi_main.h"

static const char *TAG = "HMI_MAIN";

// Global variables
system_data_t g_system_data;
lv_disp_t *g_disp = NULL;
bool g_data_updated = false;

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Pool Controller HMI Starting...");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize HMI
    hmi_init();
    
    // Create main HMI task
    xTaskCreate(hmi_task, "hmi_task", 8192, NULL, 5, NULL);
    
    // Create communication task
    xTaskCreate(communication_task, "comm_task", 4096, NULL, 4, NULL);
    
    ESP_LOGI(TAG, "Pool Controller HMI Started Successfully");
}
