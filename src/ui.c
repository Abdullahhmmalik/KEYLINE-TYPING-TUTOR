#include "ui.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

const Theme THEME = {
    {11,16,25,255}, {15,22,33,255}, {20,30,44,255}, {26,39,55,255},
    {40,55,74,255}, {237,243,250,255}, {151,169,190,255},
    {91,227,191,255}, {125,179,255,255}, {105,230,167,255}, {255,126,142,255}
};
static Font regular, bold, mono;
static int custom_regular, custom_bold, custom_mono;
static RenderTexture2D canvas;
static UiViewport viewport;
static Vector2 mouse;
static int mouse_on_canvas, press_available;

static Font font_or_default(const char *file, int *loaded)
{
    char path[600];
    const char *root = getenv("WINDIR");
    Font result = GetFontDefault();
    if (root) {
        snprintf(path, sizeof path, "%s/Fonts/%s", root, file);
        if (FileExists(path)) { result = LoadFontEx(path, 64, NULL, 0); *loaded = result.texture.id != GetFontDefault().texture.id; }
    }
    if (*loaded) SetTextureFilter(result.texture, TEXTURE_FILTER_BILINEAR);
    return result;
}
void ui_init(void)
{
    regular = font_or_default("segoeui.ttf", &custom_regular);
    bold = font_or_default("segoeuib.ttf", &custom_bold);
    mono = font_or_default("consola.ttf", &custom_mono);
    canvas = LoadRenderTexture(WINDOW_WIDTH, WINDOW_HEIGHT);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
}
void ui_shutdown(void)
{
    UnloadRenderTexture(canvas);
    if (custom_regular) UnloadFont(regular);
    if (custom_bold) UnloadFont(bold);
    if (custom_mono) UnloadFont(mono);
}
UiViewport ui_viewport_for_size(int width,int height)
{
    UiViewport v={0};
    if (width<=0 || height<=0) return v;
    v.scale=fminf(width/(float)WINDOW_WIDTH,height/(float)WINDOW_HEIGHT);
    v.destination=(Rectangle){(width-WINDOW_WIDTH*v.scale)/2,(height-WINDOW_HEIGHT*v.scale)/2,
        WINDOW_WIDTH*v.scale,WINDOW_HEIGHT*v.scale};
    v.valid=1;
    return v;
}
Vector2 ui_to_logical(UiViewport v,Vector2 p)
{
    if (!v.valid || v.scale<=0) return (Vector2){-1,-1};
    return (Vector2){(p.x-v.destination.x)/v.scale,(p.y-v.destination.y)/v.scale};
}
void ui_begin(void)
{
    Vector2 position=GetMousePosition();
    /* Both APIs use window client coordinates; multiplying by DPI here would
     * double-scale input. Snapshot layout and input once, before binding the FBO. */
    viewport=ui_viewport_for_size(GetScreenWidth(),GetScreenHeight());
    mouse=ui_to_logical(viewport,position);
    mouse_on_canvas=viewport.valid && !IsWindowMinimized() && CheckCollisionPointRec(position,viewport.destination);
    press_available=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    BeginTextureMode(canvas);
    ClearBackground(THEME.background);
}
void ui_end(void)
{
    EndTextureMode();
    BeginDrawing();
    ClearBackground(THEME.background);
    if (viewport.valid) DrawTexturePro(canvas.texture, (Rectangle){0,0,WINDOW_WIDTH,-WINDOW_HEIGHT},
        viewport.destination, (Vector2){0,0}, 0, WHITE);
    EndDrawing();
}
int ui_save_screenshot(const char *filename)
{
    Image image=LoadImageFromTexture(canvas.texture);
    int ok;
    if (!image.data) return 0;
    ImageFlipVertical(&image);
    ok=ExportImage(image,filename);
    UnloadImage(image);
    return ok;
}
Vector2 ui_mouse(void)
{
    return mouse;
}
int ui_hover(Rectangle r) { return mouse_on_canvas && CheckCollisionPointRec(ui_mouse(), r); }
int ui_clicked(Rectangle r)
{
    if (!press_available || !ui_hover(r)) return 0;
    press_available=0; /* One physical press performs at most one UI action. */
    return 1;
}
void ui_text(const char *text, float x, float y, float size, Color color)
{
    if (size < 16) size=16;
    DrawTextEx(size >= 28 ? bold : regular, text, (Vector2){x,y}, size, 0, color);
}
void ui_mono(const char *text, float x, float y, float size, Color color)
{ DrawTextEx(mono, text, (Vector2){x,y}, size, 0, color); }
float ui_text_width(const char *text, float size)
{
    if (size < 16) size=16;
    return MeasureTextEx(size >= 28 ? bold : regular, text, size, 0).x;
}
float ui_character_width(float size) { return MeasureTextEx(mono, "M", size, 0).x; }
void ui_panel(Rectangle r, Color color) { DrawRectangleRounded(r, 0.12f, 12, color); }
void ui_outline(Rectangle r, Color color) { DrawRectangleRoundedLinesEx(r, 0.12f, 12, 1, color); }
int ui_button(Rectangle r, const char *label, int style)
{
    int hover = ui_hover(r);
    Color fill = style == 1 ? THEME.accent : style == 2 ? THEME.panel_alt : THEME.panel;
    Color ink = style == 1 ? THEME.background : THEME.text;
    if (hover) { fill = ColorBrightness(fill, style == 1 ? -0.08f : 0.14f); SetMouseCursor(MOUSE_CURSOR_POINTING_HAND); }
    ui_panel(r, fill);
    if (style == 0) ui_outline(r, THEME.border);
    ui_text(label, r.x+(r.width-ui_text_width(label,18))/2, r.y+(r.height-18)/2-1, 18, ink);
    return ui_clicked(r);
}
int ui_input(Rectangle r, char *buffer, int capacity, int *focused, int digits_only)
{
    int changed = 0;
    int previously_focused=*focused;
    size_t n = strlen(buffer);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) *focused = ui_hover(r);
    if (IsKeyPressed(KEY_TAB)) *focused=!*focused;
    /* A character and an outside click may arrive in the same frame. Commit
     * the old field's pending characters before blurring, then allow the button. */
    if (previously_focused || *focused) {
        int c;
        while ((c = GetCharPressed()) != 0) {
            if (c >= 32 && c <= 126 && (!digits_only || (c >= '0' && c <= '9')) && n + 1 < (size_t)capacity) {
                buffer[n++] = (char)c; buffer[n] = 0; changed = 1;
            }
        }
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && n) { buffer[--n] = 0; changed = 1; }
    }
    if (ui_hover(r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) press_available=0;
    ui_panel(r, THEME.background);
    ui_outline(r, *focused ? THEME.accent : THEME.border);
    ui_text(buffer, r.x+16, r.y+14, 20, THEME.text);
    if (*focused && ((int)(GetTime()*2) % 2 == 0)) {
        float x = r.x+16+ui_text_width(buffer,20);
        DrawRectangle((int)x+2, (int)r.y+13, 2, 24, THEME.accent);
    }
    if (ui_hover(r)) SetMouseCursor(MOUSE_CURSOR_IBEAM);
    return changed;
}
void ui_metric(float x, float y, float width, const char *label, const char *value, const char *detail, Color color)
{
    float size=34;
    while (size>20 && ui_text_width(value,size)>width-40) size-=1;
    ui_panel((Rectangle){x,y,width,120}, THEME.panel);
    ui_text(label,x+20,y+17,16,THEME.muted);
    ui_text(value,x+20,y+41,size,color);
    if (detail && *detail) ui_text(detail,x+20,y+91,13,THEME.muted);
}
void ui_progress(Rectangle r, float percent, Color color)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    ui_panel(r, THEME.border);
    if (percent > 0) ui_panel((Rectangle){r.x,r.y,r.width*percent/100,r.height},color);
}
