
/* ============================================================================
 * IMPLEMENTATION
 * ========================================================================== */

/* twidgets.c */
#include <string.h>
#include "../src/twidget.h"
#include <stdbool.h>

typedef struct {
    DisplayCharCallback display_char;
    GetCharCallback get_char;
    GetModeCallback get_mode;
    CharMap char_map;
} WidgetSystem;
static WidgetSystem g_widget_system;

/* Helper function to put a character on screen */
static void put_char(uint8_t x, uint8_t y, uint8_t ch, uint8_t mode) {
    if (g_widget_system.display_char) {
        g_widget_system.display_char(x, y, ch, mode);
    }
}

/* Helper function to get a character from screen */
static uint8_t get_char(uint8_t x, uint8_t y) {
    if (g_widget_system.get_char) {
        return g_widget_system.get_char(x, y);
    }
    return ' ';
}

/* Helper function to get the display mode from screen */
static uint8_t get_mode(uint8_t x, uint8_t y) {
    if (g_widget_system.get_mode) {
        return g_widget_system.get_mode(x, y);
    }
    return DISPLAY_NORMAL;
}

/* Helper function to put a string on screen */
static void put_string(uint8_t x, uint8_t y, const char *str, uint8_t mode) {
    uint8_t i = 0;
    while (str[i] != '\0') {
        put_char(x + i, y, (uint8_t)str[i], mode);
        i++;
    }
}

/* Helper function to draw a box - exposed to widget API */
void draw_box(uint8_t x, uint8_t y, uint8_t width, uint8_t height) {
    CharMap *cm = &g_widget_system.char_map;
    unsigned char i;
    
    /* Top-left corner */
    put_char(x, y, cm->box_down_right, DISPLAY_NORMAL);
    
    /* Top edge */
    for (i = 1; i < width - 1; i++) {
        put_char(x + i, y, cm->box_horizontal, DISPLAY_NORMAL);
    }
    
    /* Top-right corner */
    put_char(x + width - 1, y, cm->box_down_left, DISPLAY_NORMAL);
    
    /* Sides */
    for (i = 1; i < height - 1; i++) {
        put_char(x, y + i, cm->box_vertical, DISPLAY_NORMAL);
        put_char(x + width - 1, y + i, cm->box_vertical, DISPLAY_NORMAL);
    }
    
    /* Bottom-left corner */
    put_char(x, y + height - 1, cm->box_up_right, DISPLAY_NORMAL);
    
    /* Bottom edge */
    for (i = 1; i < width - 1; i++) {
        put_char(x + i, y + height - 1, cm->box_horizontal, DISPLAY_NORMAL);
    }
    
    /* Bottom-right corner */
    put_char(x + width - 1, y + height - 1, cm->box_up_left, DISPLAY_NORMAL);
}

/* ============================================================================
 * INITIALIZATION
 * ========================================================================== */

void widget_init(DisplayCharCallback display_callback, GetCharCallback get_char_callback, GetModeCallback get_mode_callback, const CharMap *map) {
    g_widget_system.display_char = display_callback;
    g_widget_system.get_char = get_char_callback;
    g_widget_system.get_mode = get_mode_callback;
    if (map) {
        memcpy(&g_widget_system.char_map, map, sizeof(CharMap));
    }
}

/* ============================================================================
 * CHECKBOX IMPLEMENTATION
 * ========================================================================== */

void checkbox_create(Checkbox *checkbox, uint8_t x, uint8_t y, const char *label, bool has_box) {
    checkbox->pos.x = x;
    checkbox->pos.y = y;
    checkbox->checked = false;
    checkbox->has_box = has_box;
    strncpy(checkbox->label, label, MAX_LABEL_LENGTH - 1);
    checkbox->label[MAX_LABEL_LENGTH - 1] = '\0';
}

void checkbox_draw(Checkbox *checkbox) {
    CharMap *cm = &g_widget_system.char_map;
    uint8_t ch = checkbox->checked ? cm->checkbox_checked : cm->checkbox_unchecked;
    if (checkbox->has_box) {
        uint8_t width = 4 + strlen(checkbox->label);
        draw_box(checkbox->pos.x, checkbox->pos.y, width, 3);
        put_char(checkbox->pos.x + 1, checkbox->pos.y + 1, ch, DISPLAY_NORMAL);
        put_char(checkbox->pos.x + 2, checkbox->pos.y + 1, ' ', DISPLAY_NORMAL);
        put_string(checkbox->pos.x + 3, checkbox->pos.y + 1, checkbox->label, DISPLAY_NORMAL);
    } else {
        put_char(checkbox->pos.x, checkbox->pos.y, ch, DISPLAY_NORMAL);
        put_char(checkbox->pos.x + 1, checkbox->pos.y, ' ', DISPLAY_NORMAL);
        put_string(checkbox->pos.x + 2, checkbox->pos.y, checkbox->label, DISPLAY_NORMAL);
    }
}

void checkbox_set_checked(Checkbox *checkbox, bool checked) {
    checkbox->checked = checked;
    checkbox_draw(checkbox);
}

bool checkbox_is_checked(const Checkbox *checkbox) {
    return checkbox->checked;
}

void checkbox_toggle(Checkbox *checkbox) {
    checkbox->checked = !checkbox->checked;
    checkbox_draw(checkbox);
}

/* ============================================================================
 * RADIO GROUP IMPLEMENTATION
 * ========================================================================== */

void radio_create(RadioGroup *radio, uint8_t x, uint8_t y, uint8_t orientation, bool has_box) {
    radio->base_pos.x = x;
    radio->base_pos.y = y;
    radio->item_count = 0;
    radio->selected_index = 0;
    radio->orientation = orientation;
    radio->has_box = has_box;
}

void radio_add_item(RadioGroup *radio, const char *label, uint8_t offset_x, uint8_t offset_y) {
    if (radio->item_count < MAX_RADIO_ITEMS) {
        uint8_t idx = radio->item_count;
        radio->item_offsets[idx].x = offset_x;
        radio->item_offsets[idx].y = offset_y;
        strncpy(radio->labels[idx], label, MAX_LABEL_LENGTH - 1);
        radio->labels[idx][MAX_LABEL_LENGTH - 1] = '\0';
        radio->item_count++;
    }
}

void radio_draw(const RadioGroup *radio) {
    CharMap *cm = &g_widget_system.char_map;
    uint8_t i;
    if (radio->has_box) {
        uint8_t max_width = 0;
        uint8_t max_height = 0;
        /* Calculate box dimensions */
        for (i = 0; i < radio->item_count; i++) {
            uint8_t item_width = 2 + (uint8_t)strlen(radio->labels[i]) + radio->item_offsets[i].x;
            uint8_t item_height = 1 + radio->item_offsets[i].y;
            if (item_width > max_width) max_width = item_width;
            if (item_height > max_height) max_height = item_height;
        }
        draw_box(radio->base_pos.x, radio->base_pos.y, max_width + 2, max_height + 2);
        /* Draw items inside box */
        for (i = 0; i < radio->item_count; i++) {
            uint8_t ch = (i == radio->selected_index) ? cm->radio_selected : cm->radio_unselected;
            uint8_t item_x = radio->base_pos.x + 1 + radio->item_offsets[i].x;
            uint8_t item_y = radio->base_pos.y + 1 + radio->item_offsets[i].y;
            put_char(item_x, item_y, ch, DISPLAY_NORMAL);
            put_char(item_x + 1, item_y, ' ', DISPLAY_NORMAL);
            put_string(item_x + 2, item_y, radio->labels[i], DISPLAY_NORMAL);
        }
    } else {
        /* Draw items without box */
        for (i = 0; i < radio->item_count; i++) {
            uint8_t ch = (i == radio->selected_index) ? cm->radio_selected : cm->radio_unselected;
            uint8_t item_x = radio->base_pos.x + radio->item_offsets[i].x;
            uint8_t item_y = radio->base_pos.y + radio->item_offsets[i].y;
            put_char(item_x, item_y, ch, DISPLAY_NORMAL);
            put_char(item_x + 1, item_y, ' ', DISPLAY_NORMAL);
            put_string(item_x + 2, item_y, radio->labels[i], DISPLAY_NORMAL);
        }
    }
}

void radio_set_selected(RadioGroup *radio, uint8_t index) {
    if (index < radio->item_count) {
        radio->selected_index = index;
        radio_draw(radio);
    }
}

uint8_t radio_get_selected(const RadioGroup *radio) {
    return radio->selected_index;
}


/* ============================================================================
 * DROPDOWN IMPLEMENTATION
 * ========================================================================== */

void dropdown_create(Dropdown *dropdown, uint8_t x, uint8_t y, bool has_box) {
    dropdown->pos.x = x;
    dropdown->pos.y = y;
    dropdown->item_count = 0;
    dropdown->selected_index = 0;
    dropdown->highlighted_index = 0;
    dropdown->is_expanded = false;
    dropdown->has_box = has_box;
    dropdown->saved_char_count = 0;
}

void dropdown_add_item(Dropdown *dropdown, const char *item) {
    if (dropdown->item_count < MAX_DROPDOWN_ITEMS) {
        strncpy(dropdown->items[dropdown->item_count], item, MAX_LABEL_LENGTH - 1);
        dropdown->items[dropdown->item_count][MAX_LABEL_LENGTH - 1] = '\0';
        dropdown->item_count++;
    }
}

void dropdown_draw(const Dropdown *dropdown) {
    if (dropdown->item_count == 0) return;
    if (dropdown->has_box) {
        uint8_t width = strlen(dropdown->items[dropdown->selected_index]) + 4;
        draw_box(dropdown->pos.x, dropdown->pos.y, width, 3);
        put_string(dropdown->pos.x + 1, dropdown->pos.y + 1, dropdown->items[dropdown->selected_index], DISPLAY_NORMAL);
        put_char(dropdown->pos.x + width - 2, dropdown->pos.y + 1, 'v', DISPLAY_NORMAL);
    } else {
        put_string(dropdown->pos.x, dropdown->pos.y, dropdown->items[dropdown->selected_index], DISPLAY_NORMAL);
        put_char(dropdown->pos.x + strlen(dropdown->items[dropdown->selected_index]) + 1, dropdown->pos.y, 'v', DISPLAY_NORMAL);
    }
}

void dropdown_expand(Dropdown *dropdown) {
    uint8_t i;
    uint8_t max_width = 0;
    uint8_t base_x, base_y, width, height;
    if (dropdown->item_count == 0 || dropdown->is_expanded) return;
    dropdown->is_expanded = true;
    dropdown->highlighted_index = dropdown->selected_index;
    dropdown->saved_char_count = 0;
    /* Calculate max width */
    for (i = 0; i < dropdown->item_count; i++) {
        uint8_t len = strlen(dropdown->items[i]);
        if (len > max_width) max_width = len;
    }
    if (dropdown->has_box) {
        base_x = dropdown->pos.x;
        base_y = dropdown->pos.y;
        width = max_width + 4;
        height = dropdown->item_count + 2;
    } else {
        base_x = dropdown->pos.x;
        base_y = dropdown->pos.y;
        width = max_width;
        height = dropdown->item_count;
    }
    /* Save the characters that will be overwritten */
    for (uint8_t yy = 0; yy < height; yy++) {
        for (uint8_t xx = 0; xx < width; xx++) {
            uint8_t sx = base_x + xx;
            uint8_t sy = base_y + yy;
            if (dropdown->saved_char_count < MAX_SAVED_CHARS) {
                dropdown->saved_chars[dropdown->saved_char_count].x = sx;
                dropdown->saved_chars[dropdown->saved_char_count].y = sy;
                dropdown->saved_chars[dropdown->saved_char_count].character = get_char(sx, sy);
                dropdown->saved_chars[dropdown->saved_char_count].mode = get_mode(sx, sy);
                dropdown->saved_char_count++;
            }
        }
    }
    /* Now draw the expanded dropdown */
    if (dropdown->has_box) {
        draw_box(base_x, base_y, width, height);
        /* Draw all items */
        for (i = 0; i < dropdown->item_count; i++) {
            uint8_t mode = (i == dropdown->highlighted_index) ? DISPLAY_HIGHLIGHTED : DISPLAY_NORMAL;
            put_string(base_x + 1, base_y + 1 + i, dropdown->items[i], mode);
            /* Pad with spaces to fill width */
            uint8_t len = strlen(dropdown->items[i]);
            uint8_t j;
            for (j = len; j < max_width + 2; j++) {
                put_char(base_x + 1 + j, base_y + 1 + i, ' ', mode);
            }
        }
    } else {
        /* Draw all items */
        for (i = 0; i < dropdown->item_count; i++) {
            uint8_t mode = (i == dropdown->highlighted_index) ? DISPLAY_HIGHLIGHTED : DISPLAY_NORMAL;
            put_string(base_x, base_y + i, dropdown->items[i], mode);
        }
    }
}

void dropdown_collapse(Dropdown *dropdown) {
    if (!dropdown->is_expanded) return;
    /* Restore the saved characters */
    for (uint8_t i = 0; i < dropdown->saved_char_count; i++) {
        SavedChar *sc = &dropdown->saved_chars[i];
        put_char(sc->x, sc->y, sc->character, sc->mode);
    }
    dropdown->is_expanded = false;
    dropdown_draw(dropdown);
}

void dropdown_set_highlighted(Dropdown *dropdown, uint8_t index) {
    uint8_t base_x, base_y;
    if (!dropdown->is_expanded || index >= dropdown->item_count || index == dropdown->highlighted_index) {
        return;
    }
    if (dropdown->has_box) {
        base_x = dropdown->pos.x + 1;
        base_y = dropdown->pos.y + 1;
    } else {
        base_x = dropdown->pos.x;
        base_y = dropdown->pos.y;
    }
    /* Redraw old highlighted item as normal */
    put_string(base_x, base_y + dropdown->highlighted_index, dropdown->items[dropdown->highlighted_index], DISPLAY_NORMAL);
    if (dropdown->has_box) {
        uint8_t len = strlen(dropdown->items[dropdown->highlighted_index]);
        uint8_t max_width = 0;
        uint8_t i, j;
        for (i = 0; i < dropdown->item_count; i++) {
            uint8_t item_len = strlen(dropdown->items[i]);
            if (item_len > max_width) max_width = item_len;
        }
        for (j = len; j < max_width + 2; j++) {
            put_char(base_x + j, base_y + dropdown->highlighted_index, ' ', DISPLAY_NORMAL);
        }
    }
    /* Draw new highlighted item as highlighted */
    dropdown->highlighted_index = index;
    put_string(base_x, base_y + index, dropdown->items[index], DISPLAY_HIGHLIGHTED);
    if (dropdown->has_box) {
        uint8_t len = strlen(dropdown->items[index]);
        uint8_t max_width = 0;
        uint8_t i, j;
        for (i = 0; i < dropdown->item_count; i++) {
            uint8_t item_len = strlen(dropdown->items[i]);
            if (item_len > max_width) max_width = item_len;
        }
        for (j = len; j < max_width + 2; j++) {
            put_char(base_x + j, base_y + index, ' ', DISPLAY_HIGHLIGHTED);
        }
    }
}

uint8_t dropdown_get_highlighted(const Dropdown *dropdown) {
    return dropdown->highlighted_index;
}

void dropdown_set_selected(Dropdown *dropdown, uint8_t index) {
    if (index < dropdown->item_count) {
        dropdown->selected_index = index;
        if (dropdown->is_expanded) {
            dropdown_collapse(dropdown);
        } else {
            dropdown_draw(dropdown);
        }
    }
}

uint8_t dropdown_get_selected(const Dropdown *dropdown) {
    return dropdown->selected_index;
}

bool dropdown_is_expanded(const Dropdown *dropdown) {
    return dropdown->is_expanded;
}
