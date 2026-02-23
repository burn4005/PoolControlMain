#include "hmi_main.h"

#include "theme_pool.h"
#include "ui_components.h"

static const char *TAG = "GUI_MANAGER";

static screen_id_t s_current_screen = SCREEN_DASHBOARD;
static lv_obj_t *s_screens[SCREEN_COUNT] = {NULL};
static lv_obj_t *s_conn_banner;
static lv_obj_t *s_estop_btn;

static void emergency_confirm_cb(bool confirmed, void *ctx)
{
    (void)ctx;
    if (!confirmed) {
        return;
    }

    send_command_bool("emergency_stop", "all", true);

    lv_obj_t *active = lv_scr_act();
    lv_obj_set_style_border_width(active, 6, 0);
    lv_obj_set_style_border_color(active, lv_color_hex(0xFF1744), 0);
}

static void emergency_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_create_confirm_dialog("Emergency Stop", "Stop all equipment and chemical dosing?", emergency_confirm_cb, NULL);
    }
}

void gui_manager_init(void)
{
    screen_dashboard_create();
    screen_manual_create();
    screen_lighting_create();
    screen_settings_create();
    screen_alarms_create();
    screen_calibration_create();
    screen_sensor_calibration_create();
    screen_data_create();

    s_screens[SCREEN_DASHBOARD] = get_dashboard_screen();
    s_screens[SCREEN_MANUAL] = get_manual_screen();
    s_screens[SCREEN_LIGHTING] = get_lighting_screen();
    s_screens[SCREEN_SETTINGS] = get_settings_screen();
    s_screens[SCREEN_ALARMS] = get_alarms_screen();
    s_screens[SCREEN_CALIBRATION] = get_calibration_screen();
    s_screens[SCREEN_SENSOR_CALIBRATION] = get_sensor_calibration_screen();
    s_screens[SCREEN_DATA] = get_data_screen();

    lv_scr_load_anim(s_screens[SCREEN_DASHBOARD], LV_SCR_LOAD_ANIM_MOVE_RIGHT, 240, 0, false);
    s_current_screen = SCREEN_DASHBOARD;

    s_conn_banner = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_conn_banner, 340, 46);
    lv_obj_align(s_conn_banner, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_color(s_conn_banner, lv_color_hex(0xFF1744), 0);
    lv_obj_add_flag(s_conn_banner, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *conn_lbl = lv_label_create(s_conn_banner);
    lv_label_set_text(conn_lbl, "CONNECTION LOST");
    lv_obj_center(conn_lbl);

    s_estop_btn = lv_btn_create(lv_layer_top());
    lv_obj_set_size(s_estop_btn, 84, 84);
    lv_obj_align(s_estop_btn, LV_ALIGN_BOTTOM_RIGHT, -12, -88);
    lv_obj_set_style_radius(s_estop_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_estop_btn, lv_color_hex(0xFF1744), 0);
    ui_attach_touch_feedback(s_estop_btn);
    lv_obj_add_event_cb(s_estop_btn, emergency_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *estop_lbl = lv_label_create(s_estop_btn);
    lv_label_set_text(estop_lbl, "STOP");
    lv_obj_center(estop_lbl);

    ESP_LOGI(TAG, "GUI manager initialized");
}

void gui_manager_switch_screen(screen_id_t screen_id)
{
    if (screen_id < 0 || screen_id >= SCREEN_COUNT || s_screens[screen_id] == NULL || screen_id == s_current_screen) {
        return;
    }

    const lv_scr_load_anim_t anim = (screen_id > s_current_screen) ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT;
    lv_scr_load_anim(s_screens[screen_id], anim, 220, 0, false);
    s_current_screen = screen_id;
    gui_manager_update_data();
}

screen_id_t gui_manager_get_current_screen(void)
{
    return s_current_screen;
}

void gui_manager_update_data(void)
{
    switch (s_current_screen) {
        case SCREEN_DASHBOARD: screen_dashboard_update(); break;
        case SCREEN_MANUAL: screen_manual_update(); break;
        case SCREEN_LIGHTING: screen_lighting_update(); break;
        case SCREEN_SETTINGS: screen_settings_update(); break;
        case SCREEN_ALARMS: screen_alarms_update(); break;
        case SCREEN_CALIBRATION: screen_calibration_update(); break;
        case SCREEN_SENSOR_CALIBRATION: screen_sensor_calibration_update(); break;
        case SCREEN_DATA: screen_data_update(); break;
        default: break;
    }
}

void gui_manager_periodic(void)
{
    const uint64_t now_ms = (uint64_t)(esp_timer_get_time() / 1000ULL);
    const bool stale = (g_last_data_rx_ms == 0) || ((now_ms - g_last_data_rx_ms) > 30000ULL);

    if (stale) {
        lv_obj_clear_flag(s_conn_banner, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_conn_banner, LV_OBJ_FLAG_HIDDEN);
    }
}
