#include "hmi_main.h"

#include "theme_pool.h"
#include "trend_data.h"
#include "ui_components.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_chart;
static lv_obj_t *s_value;
static trend_metric_t s_metric = TREND_METRIC_PH;
static trend_range_t s_range = TREND_RANGE_24H;

static void refresh_chart(void)
{
    ui_chart_load_metric(s_chart, s_metric, s_range);
}

static void metric_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        s_metric = (trend_metric_t)(intptr_t)lv_event_get_user_data(e);
        refresh_chart();
    }
}

static void range_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        s_range = (trend_range_t)(intptr_t)lv_event_get_user_data(e);
        refresh_chart();
    }
}

void screen_data_create(void)
{
    if (s_screen != NULL) {
        return;
    }

    s_screen = lv_obj_create(NULL);
    theme_style_screen(s_screen);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Data Trends");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 20, 12);

    lv_obj_t *metric_card = create_status_card(s_screen, "Metric", 16, 74, 376, 78);
    lv_obj_t *b1 = create_button_with_label(metric_card, "pH", 10, 30, 100, 40);
    lv_obj_t *b2 = create_button_with_label(metric_card, "ORP", 130, 30, 100, 40);
    lv_obj_t *b3 = create_button_with_label(metric_card, "Temp", 250, 30, 100, 40);
    lv_obj_add_event_cb(b1, metric_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)TREND_METRIC_PH);
    lv_obj_add_event_cb(b2, metric_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)TREND_METRIC_ORP);
    lv_obj_add_event_cb(b3, metric_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)TREND_METRIC_TEMP);

    lv_obj_t *range_card = create_status_card(s_screen, "Range", 406, 74, 376, 78);
    lv_obj_t *r1 = create_button_with_label(range_card, "24h", 10, 30, 170, 40);
    lv_obj_t *r2 = create_button_with_label(range_card, "7d", 194, 30, 170, 40);
    lv_obj_add_event_cb(r1, range_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)TREND_RANGE_24H);
    lv_obj_add_event_cb(r2, range_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)TREND_RANGE_7D);

    s_chart = ui_create_trend_chart(s_screen, 16, 162, 766, 236);

    s_value = lv_label_create(s_screen);
    lv_label_set_text(s_value, "Live value --");
    lv_obj_align(s_value, LV_ALIGN_TOP_RIGHT, -20, 412);

    ui_create_nav_bar(s_screen, SCREEN_DATA);
    refresh_chart();
}

void screen_data_update(void)
{
    if (s_screen == NULL) {
        return;
    }

    if (s_metric == TREND_METRIC_PH) {
        lv_label_set_text_fmt(s_value, "Live pH %.2f", g_system_data_view.ph);
    } else if (s_metric == TREND_METRIC_ORP) {
        lv_label_set_text_fmt(s_value, "Live ORP %.0f mV", g_system_data_view.orp);
    } else {
        lv_label_set_text_fmt(s_value, "Live Temp %.1f C", g_system_data_view.temperature);
    }

    refresh_chart();
}

lv_obj_t *get_data_screen(void)
{
    return s_screen;
}
