#ifndef FOCUSKNOB_ICONS_H
#define FOCUSKNOB_ICONS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Custom icon fonts (Font Awesome 6 Solid + Brands subset)
extern const lv_font_t focusknob_icons_18;
extern const lv_font_t focusknob_icons_24;

// ── Weather Icons (from fa-solid-900) ──
#define FK_ICON_SUN         "\xEF\x86\x85"   // U+F185 fa-sun
#define FK_ICON_CLOUD       "\xEF\x83\x82"   // U+F0C2 fa-cloud
#define FK_ICON_CLOUD_SUN   "\xEF\x9B\x84"   // U+F6C4 fa-cloud-sun
#define FK_ICON_CLOUD_RAIN  "\xEF\x9C\xBD"   // U+F73D fa-cloud-rain
#define FK_ICON_SNOWFLAKE   "\xEF\x8B\x9C"   // U+F2DC fa-snowflake
#define FK_ICON_BOLT        "\xEF\x83\xA7"   // U+F0E7 fa-bolt (thunderstorm)
#define FK_ICON_SMOG        "\xEF\x9D\x9F"   // U+F75F fa-smog (fog/haze)
#define FK_ICON_CLOUD_MOON  "\xEF\x9B\x83"   // U+F6C3 fa-cloud-moon

// ── App Icons (from fa-solid-900) ──
#define FK_ICON_MUSIC       "\xEF\x80\x81"   // U+F001 fa-music
#define FK_ICON_CALENDAR    "\xEF\x84\xB3"   // U+F133 fa-calendar
#define FK_ICON_CLOCK       "\xEF\x80\x97"   // U+F017 fa-clock
#define FK_ICON_BELL        "\xEF\x83\xB3"   // U+F0F3 fa-bell
#define FK_ICON_CHEVRON_LEFT "\xEF\x81\x93"  // U+F053 fa-chevron-left
#define FK_ICON_CLIPBOARD   "\xEF\x92\xB8"   // U+F4B8 fa-clipboard-check

// ── Brand Icons (from fa-brands-400) ──
#define FK_ICON_JIRA        "\xEF\x8E\xB9"   // U+F3B9 fa-jira

#ifdef __cplusplus
}
#endif

#endif // FOCUSKNOB_ICONS_H
