#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *send_data;
    lv_obj_t *ui_wifi_status;
    lv_obj_t *ui_ip;
    lv_obj_t *ui_datetime;
    lv_obj_t *ui_firebase_status;
    lv_obj_t *ui_temp;
    lv_obj_t *ui_temp_icon;
    lv_obj_t *ui_upload_spinner_1;
    lv_obj_t *ui_sent_status;
    lv_obj_t *ui_sent_1;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_SEND_DATA = 2,
};

void create_screen_main();
void tick_screen_main();

void create_screen_send_data();
void tick_screen_send_data();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/