/**
 * @file twidget.c
 * @brief Text rendering implementation
 */

/*
Example usage:

Checkbox cb1 = { 2, 2, "Enable Sound", 0 };
Radio rb1 = { 2, 4, "Option A", 1 };
Radio rb2 = { 2, 5, "Option B", 0 };
const char *choices[] = {"Low","Medium","High"};
RadioGroup rg1 = { 2, 7, choices, 3, 0, 0 };
Textbox tb1 = { 2, 11, 10, "", 0 };
const char *ddchoices[] = {"Low","Medium","High"};
Dropdown dd1 = { 2, 13, ddchoices, 3, 0, 0 };

draw_checkbox(&cb1);
draw_radio(&rb1);
draw_radio(&rb2);
draw_radiogroup(&rg1);
draw_textbox(&tb1);
draw_dropdown(&dd1);

// Setup
widgets[0] = (Widget){ WIDGET_CHECKBOX, &cb1, (void(*)(void*))draw_checkbox, handle_checkbox };
widgets[1] = (Widget){ WIDGET_RADIO, &rb1, (void(*)(void*))draw_radio, handle_radio };
widgets[2] = (Widget){ WIDGET_RADIO, &rb2, (void(*)(void*))draw_radio, handle_radio };
widgets[3] = (Widget){ WIDGET_RADIOGROUP, &rg1, (void(*)(void*))draw_radiogroup, handle_radiogroup };
widgets[4] = (Widget){ WIDGET_TEXTBOX, &tb1, (void(*)(void*))draw_textbox, handle_textbox };
widgets[5] = (Widget){ WIDGET_DROPDOWN, &dd1, (void(*)(void*))draw_dropdown, handle_dropdown };
widget_count = 6;

// Main loop
while (1) {
    Event ev = poll_keyboard();   // your routine to map keys → Event
    if (ev.type != EVT_NONE) {
        if (ev.type == EVT_KEY_TAB) next_widget();
        else dispatch_event(&ev);
    }
}

*/

#include "f256lib.h"
#include "../src/twidget.h"
#include "../src/input.h"
#include "../src/board.h"
#include <string.h>

void vram_put(uint8_t x, uint8_t y, char c) {
	static char s[2] = { 0, 0 };
	s[0] = c;
    textGotoXY(x, y);
	textPrint(s);
}

void vram_puts(uint8_t x, uint8_t y, const char *s) {
    textGotoXY(x, y);
    textPrint((char*)s);
}

void draw_checkbox(const Checkbox *cb) {
    // Draw "[ ]" or "[X]" at (x,y), then label
    vram_put(cb->x, cb->y, '[');
    vram_put(cb->x+1, cb->y, cb->checked ? 'X' : ' ');
    vram_put(cb->x+2, cb->y, ']');
    vram_puts(cb->x+4, cb->y, cb->label);
}

void draw_radiogroup(const RadioGroup *rg) {
    for (uint8_t i = 0; i < rg->count; i++) {
        uint8_t y_pos = rg->y + i;
        vram_put(rg->x, y_pos, '(');
        vram_put(rg->x+1, y_pos, (i == rg->selected) ? 'o' : ' ');
        vram_put(rg->x+2, y_pos, ')');
        vram_puts(rg->x+4, y_pos, rg->labels[i]);
    }
}

void handle_checkbox(void *data, Event *ev) {
    Checkbox *cb = (Checkbox*)data;
    if (ev->type == EVT_KEY_SPACE || ev->type == EVT_KEY_ENTER) {
        cb->checked = !cb->checked;
    }
}

void handle_radiogroup(void *data, Event *ev) {
    RadioGroup *rg = (RadioGroup*)data;
    if (ev->type == EVT_KEY_UP) {
        if (rg->current > 0) rg->current--;
    } else if (ev->type == EVT_KEY_DOWN) {
        if (rg->current < rg->count - 1) rg->current++;
    } else if (ev->type == EVT_KEY_SPACE || ev->type == EVT_KEY_ENTER) {
        rg->selected = rg->current;
    }
}

Widget widgets[MAX_WIDGETS];
uint8_t widget_count = 0;
uint8_t focus = 0;

void dispatch_event(Event *ev) {
    if (widget_count == 0) return;
    Widget *w = &widgets[focus];
    w->handle(w->data, ev);
    w->draw(w->data);
}

void next_widget() {
    focus = (focus + 1) % widget_count;
}

