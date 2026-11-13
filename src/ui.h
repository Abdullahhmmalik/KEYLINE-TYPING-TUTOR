#ifndef UI_H
#define UI_H
#include "raylib.h"
typedef struct {
    Color background, sidebar, panel, panel_alt, border, text, muted, accent, blue, green, red;
} Theme;
extern const Theme THEME;
typedef struct { Rectangle destination; float scale; int valid; } UiViewport;
UiViewport ui_viewport_for_size(int width, int height);
Vector2 ui_to_logical(UiViewport viewport, Vector2 mouse);
void ui_init(void);
void ui_shutdown(void);
void ui_begin(void);
void ui_end(void);
int ui_save_screenshot(const char *filename);
void ui_text(const char *text, float x, float y, float size, Color color);
void ui_mono(const char *text, float x, float y, float size, Color color);
float ui_text_width(const char *text, float size);
float ui_character_width(float size);
void ui_panel(Rectangle r, Color color);
void ui_outline(Rectangle r, Color color);
int ui_button(Rectangle r, const char *label, int style);
int ui_input(Rectangle r, char *buffer, int capacity, int *focused, int digits_only);
void ui_metric(float x, float y, float width, const char *label, const char *value, const char *detail, Color value_color);
void ui_progress(Rectangle r, float percent, Color color);
Vector2 ui_mouse(void);
int ui_hover(Rectangle r);
int ui_clicked(Rectangle r);
#endif
