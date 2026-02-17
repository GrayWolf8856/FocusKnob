#ifndef HOME_BG_H
#define HOME_BG_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Render the gradient sky + mountain silhouette background onto a canvas.
 * The canvas must be 360x360 pixels, RGB565 format.
 * Should be called once at boot — the background is static.
 */
void home_bg_render(lv_obj_t *canvas);

#ifdef __cplusplus
}
#endif

#endif // HOME_BG_H
