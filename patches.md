# LVGL Patches

LVGL v9.2.2 is included in `libs/lvgl` with the following modifications:

## Disable Scroll Animation

**File:** `libs/lvgl/src/core/lv_obj_scroll.c` (lines 21-22)

**Change:**
```c
// Before:
#define SCROLL_ANIM_TIME_MIN    200    /*ms*/
#define SCROLL_ANIM_TIME_MAX    400    /*ms*/

// After:
#define SCROLL_ANIM_TIME_MIN    0    /*ms*/
#define SCROLL_ANIM_TIME_MAX    0    /*ms*/
```

**Reason:** The textarea widget hardcodes `LV_ANIM_ON` when scrolling to keep the cursor visible. This causes a noticeable animation delay when moving the cursor. Setting the animation times to 0 makes scrolling snap immediately.
