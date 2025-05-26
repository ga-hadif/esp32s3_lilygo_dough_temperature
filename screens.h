#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *obj0;
    lv_obj_t *ui_wifi_status;
    lv_obj_t *time_panel;
    lv_obj_t *ui_time;
    lv_obj_t *header_panel;
    lv_obj_t *header_label_panel;
    lv_obj_t *ui_test_datetime;
    lv_obj_t *time_panel_1;
    lv_obj_t *ui_temp;
    lv_obj_t *obj1;
    lv_obj_t *ui_upload_status;
    lv_obj_t *header_label_panel_1;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_MAIN = 1,
};

void create_screen_main();
void tick_screen_main();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/