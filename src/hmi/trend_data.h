#ifndef TREND_DATA_H
#define TREND_DATA_H

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum {
    TREND_METRIC_PH = 0,
    TREND_METRIC_ORP,
    TREND_METRIC_TEMP,
    TREND_METRIC_COUNT
} trend_metric_t;

typedef enum {
    TREND_RANGE_24H = 0,
    TREND_RANGE_7D
} trend_range_t;

esp_err_t trend_data_init(void);
void trend_data_add_sample(float ph, float orp, float temperature, uint64_t timestamp_ms);
size_t trend_data_copy_series(trend_metric_t metric, trend_range_t range, float *out_values, size_t out_capacity);

#endif
