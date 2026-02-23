#include "trend_data.h"

#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "TREND_DATA";

static const size_t TREND_24H_POINTS = 1440;
static const size_t TREND_7D_POINTS = 672;

typedef struct {
    float *values;
    size_t capacity;
    size_t head;
    bool full;
} ring_t;

static ring_t s_24h[TREND_METRIC_COUNT];
static ring_t s_7d[TREND_METRIC_COUNT];

static float s_agg_sum[TREND_METRIC_COUNT];
static uint8_t s_agg_count;
static SemaphoreHandle_t s_trend_mutex;

static void ring_push(ring_t *ring, float value)
{
    ring->values[ring->head] = value;
    ring->head = (ring->head + 1U) % ring->capacity;
    if (ring->head == 0U) {
        ring->full = true;
    }
}

static size_t ring_copy_ordered(const ring_t *ring, float *out_values, size_t out_capacity)
{
    size_t count = ring->full ? ring->capacity : ring->head;
    if (count == 0 || out_capacity == 0 || out_values == NULL) {
        return 0;
    }

    if (count > out_capacity) {
        count = out_capacity;
    }

    size_t start = ring->full ? ring->head : 0;
    for (size_t i = 0; i < count; i++) {
        size_t idx = (start + i) % ring->capacity;
        out_values[i] = ring->values[idx];
    }

    return count;
}

esp_err_t trend_data_init(void)
{
    memset(s_24h, 0, sizeof(s_24h));
    memset(s_7d, 0, sizeof(s_7d));
    memset(s_agg_sum, 0, sizeof(s_agg_sum));
    s_agg_count = 0;
    if (s_trend_mutex == NULL) {
        s_trend_mutex = xSemaphoreCreateMutex();
        if (s_trend_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    for (int i = 0; i < TREND_METRIC_COUNT; i++) {
        s_24h[i].capacity = TREND_24H_POINTS;
        s_24h[i].values = static_cast<float *>(heap_caps_calloc(TREND_24H_POINTS, sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        s_7d[i].capacity = TREND_7D_POINTS;
        s_7d[i].values = static_cast<float *>(heap_caps_calloc(TREND_7D_POINTS, sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

        if (s_24h[i].values == NULL || s_7d[i].values == NULL) {
            ESP_LOGE(TAG, "Failed to allocate trend buffers in PSRAM");
            return ESP_ERR_NO_MEM;
        }
    }

    ESP_LOGI(TAG, "Trend storage ready: 24h=%u points, 7d=%u points", (unsigned)TREND_24H_POINTS, (unsigned)TREND_7D_POINTS);
    return ESP_OK;
}

void trend_data_add_sample(float ph, float orp, float temperature, uint64_t timestamp_ms)
{
    (void)timestamp_ms;
    if (s_trend_mutex == NULL) {
        return;
    }
    if (xSemaphoreTake(s_trend_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return;
    }

    const float values[TREND_METRIC_COUNT] = {ph, orp, temperature};

    for (int i = 0; i < TREND_METRIC_COUNT; i++) {
        ring_push(&s_24h[i], values[i]);
        s_agg_sum[i] += values[i];
    }

    s_agg_count++;
    if (s_agg_count >= 15) {
        for (int i = 0; i < TREND_METRIC_COUNT; i++) {
            ring_push(&s_7d[i], s_agg_sum[i] / 15.0f);
            s_agg_sum[i] = 0.0f;
        }
        s_agg_count = 0;
    }
    xSemaphoreGive(s_trend_mutex);
}

size_t trend_data_copy_series(trend_metric_t metric, trend_range_t range, float *out_values, size_t out_capacity)
{
    if (metric < 0 || metric >= TREND_METRIC_COUNT) {
        return 0;
    }
    if (s_trend_mutex == NULL) {
        return 0;
    }
    if (xSemaphoreTake(s_trend_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return 0;
    }

    if (range == TREND_RANGE_7D) {
        size_t copied = ring_copy_ordered(&s_7d[metric], out_values, out_capacity);
        xSemaphoreGive(s_trend_mutex);
        return copied;
    }
    size_t copied = ring_copy_ordered(&s_24h[metric], out_values, out_capacity);
    xSemaphoreGive(s_trend_mutex);
    return copied;
}
