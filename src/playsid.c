#include "f256lib.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "input.h"
#include "playsid.h"
#include "timer.h"

static uint32_t s_siddata_address = 0;
static uint16_t s_siddata_frames = 0;
static bool s_playback_active = false;

static uint32_t poke_sid_frame(uint32_t siddata) {
    byte saved_io = PEEK(MMU_IO_CTRL);
    POKE_MEMMAP(MMU_IO_CTRL, MMU_IO_PAGE_0);
    for (uint8_t i = 0; i < 25; i++) {
        POKE(SID1 + i, FAR_PEEK(siddata));
        siddata++;
    }
    POKE_MEMMAP(MMU_IO_CTRL, saved_io);
    return siddata;
}

void schedule_playback(uint32_t siddata, uint16_t sidframes) {
    s_siddata_address = siddata;
    s_siddata_frames = sidframes;
    s_playback_active = true;
}

// non-blocking sid playback service — called once per Timer0 tick from timer_service
void streaming_sid_service(void) {
    if (!s_playback_active) {
        return;
    }

    if (s_siddata_frames > 0u) {
        s_siddata_address = poke_sid_frame(s_siddata_address);
        s_siddata_frames--;
    } else {
        s_playback_active = false;
        clearSIDRegisters();
    }
}

// blocking sid playback with keyboard interrupt; returns true if skipped by key
bool playback(uint32_t siddata, uint16_t sidframes) {
    kernelNextEvent();
    input_event_t event;
    if (input_translate_event(&event) && event.type == INPUT_EVENT_KEY_DOWN) {
        clearSIDRegisters();
        return true;
    }
    while (sidframes > 0u) {
        if (isTimerDone()) {
            siddata = poke_sid_frame(siddata);
            sidframes--;
            gameSetTimer0();
            kernelNextEvent();
            if (input_translate_event(&event) && event.type == INPUT_EVENT_KEY_DOWN) {
                clearSIDRegisters();
                return true;
            }
        }
    }
    clearSIDRegisters();
    return false;
}

bool is_sid_playing(void) {
    return s_playback_active;
}

void stop_sid_playback(void) {
    s_playback_active = false;
    clearSIDRegisters();
}
