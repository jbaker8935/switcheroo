#define TEXTBOX_MAX 32
#define MAX_WIDGETS 10

typedef struct {
    uint8_t x, y;
    const char *label;
    uint8_t checked;   // 0 = off, 1 = on
} Checkbox;

typedef struct {
    uint8_t x, y;
    const char **labels;  // array of label strings
    uint8_t count;        // number of radio buttons
    uint8_t selected;     // index of currently selected radio (0-based)
    uint8_t current;      // index of currently focused radio (0-based)
} RadioGroup;

typedef enum {
    EVT_NONE,
    EVT_KEY_LEFT,
    EVT_KEY_RIGHT,
    EVT_KEY_UP,
    EVT_KEY_DOWN,
    EVT_KEY_ENTER,
    EVT_KEY_SPACE,
    EVT_KEY_CHAR,
    EVT_KEY_BACKSPACE   // for textboxes
} EventType;

typedef struct {
    EventType type;
    char ch;   // valid only if type == EVT_KEY_CHAR
} Event;

typedef enum { WIDGET_CHECKBOX,WIDGET_RADIOGROUP } WidgetType;

typedef struct Widget {
    WidgetType type;
    void *data;   // points to Checkbox, RadioGroup, etc.
    void (*draw)(void *data);
    void (*handle)(void *data, Event *ev);
} Widget;

