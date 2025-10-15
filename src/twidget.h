#ifndef TWIDGETS_H
#define TWIDGETS_H
#include <stdint.h>
#include <stdbool.h>

/* Maximum limits */
#define MAX_RADIO_ITEMS 16
#define MAX_DROPDOWN_ITEMS 16
#define MAX_LABEL_LENGTH 64
#define MAX_SAVED_CHARS 255

/* Widget types */
#define WIDGET_CHECKBOX 0
#define WIDGET_RADIO_GROUP 1
#define WIDGET_DROPDOWN 2

/* Display mode for characters */
#define DISPLAY_NORMAL 0
#define DISPLAY_HIGHLIGHTED 1

/* Radio button layout orientation */
#define LAYOUT_VERTICAL 0
#define LAYOUT_HORIZONTAL 1

/* Character map for widget rendering */
typedef struct {
    uint8_t radio_unselected;
    uint8_t radio_selected;
    uint8_t checkbox_unchecked;
    uint8_t checkbox_checked;
    uint8_t box_down_right;    /* ┌ */
    uint8_t box_down_left;     /* ┐ */
    uint8_t box_horizontal;    /* ─ */
    uint8_t box_vertical;      /* │ */
    uint8_t box_up_right;      /* └ */
    uint8_t box_up_left;       /* ┘ */
} CharMap;

/* Display callback function type */
typedef void (*DisplayCharCallback)(uint8_t x, uint8_t y, uint8_t character, uint8_t mode);

/* Get character callback function type */
typedef uint8_t (*GetCharCallback)(uint8_t x, uint8_t y);

/* Get mode callback function type */
typedef uint8_t (*GetModeCallback)(uint8_t x, uint8_t y);

/* Position structure */
typedef struct {
    uint8_t x;
    uint8_t y;
} Position;

/* Saved character for dropdown overlay restoration */
typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t character;
    uint8_t mode;
} SavedChar;

/* Checkbox widget */
typedef struct {
    Position pos;
    char label[MAX_LABEL_LENGTH];
    bool checked;
    bool has_box;
} Checkbox;

/* Radio button group widget */
typedef struct {
    Position base_pos;
    Position item_offsets[MAX_RADIO_ITEMS];
    char labels[MAX_RADIO_ITEMS][MAX_LABEL_LENGTH];
    uint8_t item_count;
    uint8_t selected_index;
    uint8_t orientation;
    bool has_box;
} RadioGroup;

/* Dropdown widget */
typedef struct {
    Position pos;
    char items[MAX_DROPDOWN_ITEMS][MAX_LABEL_LENGTH];
    uint8_t item_count;
    uint8_t selected_index;
    uint8_t highlighted_index;
    bool is_expanded;
    bool has_box;
    SavedChar saved_chars[MAX_SAVED_CHARS];
    uint8_t saved_char_count;
} Dropdown;

/* ============================================================================
 * INITIALIZATION - Uses proper types in API
 * ========================================================================== */

/* Initialize the widget system with display callback and character map */
void widget_init(DisplayCharCallback display_callback, GetCharCallback get_char_callback, GetModeCallback get_mode_callback, const CharMap *map);

/* ============================================================================
 * CHECKBOX FUNCTIONS - All APIs use proper types
 * ========================================================================== */

/* Create a checkbox at position (x, y) with label */
void checkbox_create(Checkbox *cb, uint8_t x, uint8_t y, const char *label, bool has_box);

/* Draw the checkbox */
void checkbox_draw(Checkbox *cb);

/* Set checkbox state */
void checkbox_set_checked(Checkbox *cb, bool checked);

/* Get checkbox state */
bool checkbox_is_checked(const Checkbox *cb);

/* Toggle checkbox state and redraw */
void checkbox_toggle(Checkbox *cb);

/* ============================================================================
 * RADIO GROUP FUNCTIONS - All APIs use proper types
 * ========================================================================== */

/* Create a radio button group */
void radio_create(RadioGroup *rg, uint8_t x, uint8_t y, uint8_t orientation, bool has_box);

/* Add an item to the radio group with relative offset */
void radio_add_item(RadioGroup *rg, const char *label, uint8_t offset_x, uint8_t offset_y);

/* Draw the radio group */
void radio_draw(const RadioGroup *rg);

/* Set selected item by index */
void radio_set_selected(RadioGroup *rg, uint8_t index);

/* Get selected item index */
uint8_t radio_get_selected(const RadioGroup *rg);

/* ============================================================================
 * DROPDOWN FUNCTIONS - All APIs use proper types
 * ========================================================================== */

/* Create a dropdown at position (x, y) */
void dropdown_create(Dropdown *dd, uint8_t x, uint8_t y, bool has_box);

/* Add an item to the dropdown */
void dropdown_add_item(Dropdown *dd, const char *item);

/* Draw the dropdown in closed state */
void dropdown_draw(const Dropdown *dd);

/* Expand the dropdown list */
void dropdown_expand(Dropdown *dd);

/* Collapse the dropdown list and restore overlay */
void dropdown_collapse(Dropdown *dd);

/* Set which item is highlighted (for navigation) */
void dropdown_set_highlighted(Dropdown *dd, uint8_t index);

/* Get highlighted item index */
uint8_t dropdown_get_highlighted(const Dropdown *dd);

/* Set the selected item (commits selection) */
void dropdown_set_selected(Dropdown *dd, uint8_t index);

/* Get selected item index */
uint8_t dropdown_get_selected(const Dropdown *dd);

/* Check if dropdown is expanded */
bool dropdown_is_expanded(const Dropdown *dd);

/* ============================================================================
 * HELPER FUNCTIONS - All APIs use proper types
 * ========================================================================== */

void draw_box(uint8_t x, uint8_t y, uint8_t width, uint8_t height);

#endif /* TWIDGETS_H */
