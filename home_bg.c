#include "home_bg.h"
#include <math.h>
#include <string.h>

// Convert RGB888 to RGB565
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Blend two RGB565 colors by factor t (0.0 = a, 1.0 = b)
static inline uint16_t blend565(uint16_t a, uint16_t b, float t)
{
    uint8_t r1 = (a >> 11) & 0x1F, g1 = (a >> 5) & 0x3F, b1 = a & 0x1F;
    uint8_t r2 = (b >> 11) & 0x1F, g2 = (b >> 5) & 0x3F, b2 = b & 0x1F;
    uint8_t r = (uint8_t)(r1 + (r2 - r1) * t);
    uint8_t g = (uint8_t)(g1 + (g2 - g1) * t);
    uint8_t bl = (uint8_t)(b1 + (b2 - b1) * t);
    return (r << 11) | (g << 5) | bl;
}

// ── Main render function ─────────────────────────────────────────
// Dark navy/deep blue futuristic design with subtle diagonal accents
void home_bg_render(lv_obj_t *canvas)
{
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);

    lv_img_dsc_t *dsc = lv_canvas_get_img(canvas);
    uint16_t *buf = (uint16_t *)dsc->data;
    const int W = 360;
    const int H = 360;

    // ── Color palette (dark theme) ──
    // Top-left: dark navy blue
    // Bottom-right: deep black-blue
    uint8_t tl_r = 0x1a, tl_g = 0x1a, tl_b = 0x2e;  // Dark navy
    uint8_t br_r = 0x0d, br_g = 0x0d, br_b = 0x1a;  // Deep dark blue

    // ── Pass 1: Diagonal gradient (top-left to bottom-right) ──
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float t = ((float)x + (float)y) / (float)(W + H - 2);

            uint8_t r = (uint8_t)(tl_r + (br_r - tl_r) * t);
            uint8_t g = (uint8_t)(tl_g + (br_g - tl_g) * t);
            uint8_t b = (uint8_t)(tl_b + (br_b - tl_b) * t);

            buf[y * W + x] = rgb565(r, g, b);
        }
    }

    // ── Pass 2: Subtle accent lines ──
    // Two sleek diagonal lines — muted accent color on dark background

    // Line 1: Main diagonal with subtle teal/accent glow
    {
        float slope = -1.2f;
        float offset = 520.0f;
        uint16_t line_color_bright = rgb565(0x4e, 0xcc, 0xa3);  // Teal accent
        uint16_t line_color_dim = rgb565(0x2a, 0x5a, 0x4a);     // Dim teal

        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                float line_y = slope * (float)x + offset;
                float dist = fabsf((float)y - line_y);

                if (dist < 1.2f) {
                    float intensity = 1.0f - (dist / 1.2f);
                    buf[y * W + x] = blend565(buf[y * W + x], line_color_bright, intensity * 0.35f);
                } else if (dist < 3.0f) {
                    float intensity = 1.0f - ((dist - 1.2f) / 1.8f);
                    buf[y * W + x] = blend565(buf[y * W + x], line_color_dim, intensity * 0.15f);
                }
            }
        }

        // Shade below line 1 slightly darker
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                float line_y = slope * (float)x + offset;
                if ((float)y > line_y + 3.0f) {
                    uint16_t px = buf[y * W + x];
                    uint8_t r5 = (px >> 11) & 0x1F;
                    uint8_t g6 = (px >> 5) & 0x3F;
                    uint8_t b5 = px & 0x1F;
                    r5 = (uint8_t)(r5 * 0.85f);
                    g6 = (uint8_t)(g6 * 0.85f);
                    b5 = (uint8_t)(b5 * 0.85f);
                    buf[y * W + x] = (r5 << 11) | (g6 << 5) | b5;
                }
            }
        }
    }

    // Line 2: Secondary diagonal — parallel, more subtle
    {
        float slope = -1.2f;
        float offset = 580.0f;
        uint16_t line_color_bright = rgb565(0x30, 0x80, 0x70);  // Muted teal

        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                float line_y = slope * (float)x + offset;
                float dist = fabsf((float)y - line_y);

                if (dist < 0.8f) {
                    float intensity = 1.0f - (dist / 0.8f);
                    buf[y * W + x] = blend565(buf[y * W + x], line_color_bright, intensity * 0.25f);
                } else if (dist < 2.0f) {
                    float intensity = 1.0f - ((dist - 0.8f) / 1.2f);
                    buf[y * W + x] = blend565(buf[y * W + x], line_color_bright, intensity * 0.10f);
                }
            }
        }

        // Shade below line 2 even darker
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                float line_y = slope * (float)x + offset;
                if ((float)y > line_y + 2.0f) {
                    uint16_t px = buf[y * W + x];
                    uint8_t r5 = (px >> 11) & 0x1F;
                    uint8_t g6 = (px >> 5) & 0x3F;
                    uint8_t b5 = px & 0x1F;
                    r5 = (uint8_t)(r5 * 0.88f);
                    g6 = (uint8_t)(g6 * 0.88f);
                    b5 = (uint8_t)(b5 * 0.88f);
                    buf[y * W + x] = (r5 << 11) | (g6 << 5) | b5;
                }
            }
        }
    }

    // ── Pass 3: Vignette for circular display ──
    // Fade to black at edges to blend with the round bezel
    {
        float cx = 180.0f, cy = 180.0f;
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                float dx = (float)x - cx;
                float dy = (float)y - cy;
                float dist = sqrtf(dx * dx + dy * dy);

                if (dist > 140.0f) {
                    float vignette = (dist - 140.0f) / 40.0f;
                    if (vignette > 1.0f) vignette = 1.0f;
                    vignette *= 0.6f;  // Stronger vignette on dark theme

                    uint16_t px = buf[y * W + x];
                    uint8_t r5 = (px >> 11) & 0x1F;
                    uint8_t g6 = (px >> 5) & 0x3F;
                    uint8_t b5 = px & 0x1F;

                    r5 = (uint8_t)(r5 * (1.0f - vignette));
                    g6 = (uint8_t)(g6 * (1.0f - vignette));
                    b5 = (uint8_t)(b5 * (1.0f - vignette));

                    buf[y * W + x] = (r5 << 11) | (g6 << 5) | b5;
                }
            }
        }
    }

    // Invalidate canvas so LVGL redraws it
    lv_obj_invalidate(canvas);
}
