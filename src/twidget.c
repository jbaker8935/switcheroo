
/* ============================================================================
 * IMPLEMENTATION
 * ========================================================================== */

/* twidgets.c */

#include <string.h>
#include "../src/twidget.h"

typedef struct {
    DisplayCharCallback display_char;
    CharMap char_map;
} WidgetSystem;

static WidgetSystem g_widget_system;

/* Helper function to put a character on screen */
static void put_char(unsigned char x, unsigned char y, unsigned char ch, unsigned char mode) {
    if (g_widget_system.display_char) {
        g_widget_system.display_char(x, y, ch, mode);
    }
}

/* Helper function to put a string on screen */
static void put_string(unsigned char x, unsigned char y, const char *str, unsigned char mode) {
    unsigned char i = 0;
    while (str[i] != '\0') {
        put_char(x + i, y, (unsigned char)str[i], mode);
        i++;
    }
}

/* Helper function to draw a box */
static void draw_box(unsigned char x, unsigned char y, unsigned char width, unsigned char height) {
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

void widget_init(void *callback, void *map) {
    g_widget_system.display_char = (DisplayCharCallback)callback;
    if (map) {
        memcpy(&g_widget_system.char_map, map, sizeof(CharMap));
    }
}

/* ============================================================================
 * CHECKBOX IMPLEMENTATION
 * ========================================================================== */

void checkbox_create(void *cb, unsigned char x, unsigned char y, const char *label, unsigned char has_box) {
    Checkbox *checkbox = (Checkbox *)cb;
    checkbox->pos.x = x;
    checkbox->pos.y = y;
    checkbox->checked = 0;
    checkbox->has_box = has_box;
    strncpy(checkbox->label, label, MAX_LABEL_LENGTH - 1);
    checkbox->label[MAX_LABEL_LENGTH - 1] = '\0';
}

void checkbox_draw(void *cb) {
    Checkbox *checkbox = (Checkbox *)cb;
    CharMap *cm = &g_widget_system.char_map;
    unsigned char ch = checkbox->checked ? cm->checkbox_checked : cm->checkbox_unchecked;
    
    if (checkbox->has_box) {
        unsigned char width = 4 + strlen(checkbox->label);
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

void checkbox_set_checked(void *cb, unsigned char checked) {
    Checkbox *checkbox = (Checkbox *)cb;
    checkbox->checked = checked ? 1 : 0;
    checkbox_draw(cb);
}

unsigned char checkbox_is_checked(void *cb) {
    Checkbox *checkbox = (Checkbox *)cb;
    return checkbox->checked;
}

void checkbox_toggle(void *cb) {
    Checkbox *checkbox = (Checkbox *)cb;
    checkbox->checked = !checkbox->checked;
    checkbox_draw(cb);
}

/* ============================================================================
 * RADIO GROUP IMPLEMENTATION
 * ========================================================================== */

void radio_create(void *rg, unsigned char x, unsigned char y, unsigned char orientation, unsigned char has_box) {
    RadioGroup *radio = (RadioGroup *)rg;
    radio->base_pos.x = x;
    radio->base_pos.y = y;
    radio->item_count = 0;
    radio->selected_index = 0;
    radio->orientation = orientation;
    radio->has_box = has_box;
}

void radio_add_item(void *rg, const char *label, unsigned char offset_x, unsigned char offset_y) {
    RadioGroup *radio = (RadioGroup *)rg;
    if (radio->item_count < MAX_RADIO_ITEMS) {
        unsigned char idx = radio->item_count;
        radio->item_offsets[idx].x = offset_x;
        radio->item_offsets[idx].y = offset_y;
        strncpy(radio->labels[idx], label, MAX_LABEL_LENGTH - 1);
        radio->labels[idx][MAX_LABEL_LENGTH - 1] = '\0';
        radio->item_count++;
    }
}

void radio_draw(void *rg) {
    RadioGroup *radio = (RadioGroup *)rg;
    CharMap *cm = &g_widget_system.char_map;
    unsigned char i;
    
    if (radio->has_box) {
        unsigned char max_width = 0;
        unsigned char max_height = 0;
        
        /* Calculate box dimensions */
        for (i = 0; i < radio->item_count; i++) {
            unsigned char item_width = 2 + strlen(radio->labels[i]) + radio->item_offsets[i].x;
            unsigned char item_height = 1 + radio->item_offsets[i].y;
            if (item_width > max_width) max_width = item_width;
            if (item_height > max_height) max_height = item_height;
        }
        
        draw_box(radio->base_pos.x, radio->base_pos.y, max_width + 2, max_height + 2);
        
        /* Draw items inside box */
        for (i = 0; i < radio->item_count; i++) {
            unsigned char ch = (i == radio->selected_index) ? cm->radio_selected : cm->radio_unselected;
            unsigned char item_x = radio->base_pos.x + 1 + radio->item_offsets[i].x;
            unsigned char item_y = radio->base_pos.y + 1 + radio->item_offsets[i].y;
            
            put_char(item_x, item_y, ch, DISPLAY_NORMAL);
            put_char(item_x + 1, item_y, ' ', DISPLAY_NORMAL);
            put_string(item_x + 2, item_y, radio->labels[i], DISPLAY_NORMAL);
        }
    } else {
        /* Draw items without box */
        for (i = 0; i < radio->item_count; i++) {
            unsigned char ch = (i == radio->selected_index) ? cm->radio_selected : cm->radio_unselected;
            unsigned char item_x = radio->base_pos.x + radio->item_offsets[i].x;
            unsigned char item_y = radio->base_pos.y + radio->item_offsets[i].y;
            
            put_char(item_x, item_y, ch, DISPLAY_NORMAL);
            put_char(item_x + 1, item_y, ' ', DISPLAY_NORMAL);
            put_string(item_x + 2, item_y, radio->labels[i], DISPLAY_NORMAL);
        }
    }
}

void radio_set_selected(void *rg, unsigned char index) {
    RadioGroup *radio = (RadioGroup *)rg;
    if (index < radio->item_count) {
        radio->selected_index = index;
        radio_draw(rg);
    }
}

unsigned char radio_get_selected(void *rg) {
    RadioGroup *radio = (RadioGroup *)rg;
    return radio->selected_index;
}

/* ============================================================================
 * DROPDOWN IMPLEMENTATION
 * ========================================================================== */

void dropdown_create(void *dd, unsigned char x, unsigned char y, unsigned char has_box) {
    Dropdown *dropdown = (Dropdown *)dd;
    dropdown->pos.x = x;
    dropdown->pos.y = y;
    dropdown->item_count = 0;
    dropdown->selected_index = 0;
    dropdown->highlighted_index = 0;
    dropdown->is_expanded = 0;
    dropdown->has_box = has_box;
    dropdown->saved_char_count = 0;
}

void dropdown_add_item(void *dd, const char *item) {
    Dropdown *dropdown = (Dropdown *)dd;
    if (dropdown->item_count < MAX_DROPDOWN_ITEMS) {
        strncpy(dropdown->items[dropdown->item_count], item, MAX_LABEL_LENGTH - 1);
        dropdown->items[dropdown->item_count][MAX_LABEL_LENGTH - 1] = '\0';
        dropdown->item_count++;
    }
}

void dropdown_draw(void *dd) {
    Dropdown *dropdown = (Dropdown *)dd;
    if (dropdown->item_count == 0) return;
    
    if (dropdown->has_box) {
        unsigned char width = strlen(dropdown->items[dropdown->selected_index]) + 4;
        draw_box(dropdown->pos.x, dropdown->pos.y, width, 3);
        put_string(dropdown->pos.x + 1, dropdown->pos.y + 1, dropdown->items[dropdown->selected_index], DISPLAY_NORMAL);
        put_char(dropdown->pos.x + width - 2, dropdown->pos.y + 1, 'v', DISPLAY_NORMAL);
    } else {
        put_string(dropdown->pos.x, dropdown->pos.y, dropdown->items[dropdown->selected_index], DISPLAY_NORMAL);
        put_char(dropdown->pos.x + strlen(dropdown->items[dropdown->selected_index]) + 1, dropdown->pos.y, 'v', DISPLAY_NORMAL);
    }
}

void dropdown_expand(void *dd) {
    Dropdown *dropdown = (Dropdown *)dd;
    unsigned char i;
    unsigned char max_width = 0;
    unsigned char base_x, base_y;
    
    if (dropdown->item_count == 0 || dropdown->is_expanded) return;
    
    dropdown->is_expanded = 1;
    dropdown->highlighted_index = dropdown->selected_index;
    dropdown->saved_char_count = 0;
    
    /* Calculate max width */
    for (i = 0; i < dropdown->item_count; i++) {
        unsigned char len = strlen(dropdown->items[i]);
        if (len > max_width) max_width = len;
    }
    
    if (dropdown->has_box) {
        base_x = dropdown->pos.x;
        base_y = dropdown->pos.y;
        draw_box(base_x, base_y, max_width + 4, dropdown->item_count + 2);
        
        /* Draw all items */
        for (i = 0; i < dropdown->item_count; i++) {
            unsigned char mode = (i == dropdown->highlighted_index) ? DISPLAY_INVERTED : DISPLAY_NORMAL;
            put_string(base_x + 1, base_y + 1 + i, dropdown->items[i], mode);
            
            /* Pad with spaces to fill width */
            unsigned char len = strlen(dropdown->items[i]);
            unsigned char j;
            for (j = len; j < max_width + 2; j++) {
                put_char(base_x + 1 + j, base_y + 1 + i, ' ', mode);
            }
        }
    } else {
        base_x = dropdown->pos.x;
        base_y = dropdown->pos.y;
        
        /* Draw all items */
        for (i = 0; i < dropdown->item_count; i++) {
            unsigned char mode = (i == dropdown->highlighted_index) ? DISPLAY_INVERTED : DISPLAY_NORMAL;
            put_string(base_x, base_y + i, dropdown->items[i], mode);
        }
    }
}

void dropdown_collapse(void *dd) {
    Dropdown *dropdown = (Dropdown *)dd;
    if (!dropdown->is_expanded) return;
    
    dropdown->is_expanded = 0;
    dropdown_draw(dd);
}

void dropdown_set_highlighted(void *dd, unsigned char index) {
    Dropdown *dropdown = (Dropdown *)dd;
    unsigned char base_x, base_y;
    
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
        unsigned char len = strlen(dropdown->items[dropdown->highlighted_index]);
        unsigned char max_width = 0;
        unsigned char i, j;
        for (i = 0; i < dropdown->item_count; i++) {
            unsigned char item_len = strlen(dropdown->items[i]);
            if (item_len > max_width) max_width = item_len;
        }
        for (j = len; j < max_width + 2; j++) {
            put_char(base_x + j, base_y + dropdown->highlighted_index, ' ', DISPLAY_NORMAL);
        }
    }
    
    /* Draw new highlighted item as inverted */
    dropdown->highlighted_index = index;
    put_string(base_x, base_y + index, dropdown->items[index], DISPLAY_INVERTED);
    if (dropdown->has_box) {
        unsigned char len = strlen(dropdown->items[index]);
        unsigned char max_width = 0;
        unsigned char i, j;
        for (i = 0; i < dropdown->item_count; i++) {
            unsigned char item_len = strlen(dropdown->items[i]);
            if (item_len > max_width) max_width = item_len;
        }
        for (j = len; j < max_width + 2; j++) {
            put_char(base_x + j, base_y + index, ' ', DISPLAY_INVERTED);
        }
    }
}

unsigned char dropdown_get_highlighted(void *dd) {
    Dropdown *dropdown = (Dropdown *)dd;
    return dropdown->highlighted_index;
}

void dropdown_set_selected(void *dd, unsigned char index) {
    Dropdown *dropdown = (Dropdown *)dd;
    if (index < dropdown->item_count) {
        dropdown->selected_index = index;
        if (dropdown->is_expanded) {
            dropdown_collapse(dd);
        } else {
            dropdown_draw(dd);
        }
    }
}

unsigned char dropdown_get_selected(void *dd) {
    Dropdown *dropdown = (Dropdown *)dd;
    return dropdown->selected_index;
}

unsigned char dropdown_is_expanded(void *dd) {
    Dropdown *dropdown = (Dropdown *)dd;
    return dropdown->is_expanded;
}
