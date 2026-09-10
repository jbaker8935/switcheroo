#include "f256lib.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "playsid.h"
#include "timer.h"

static uint32_t s_siddata_address = 0;
static uint16_t s_siddata_frames = 0;
static bool s_playback_active = false;

void schedule_playback(uint32_t siddata, uint16_t sidframes) {
    s_siddata_address = siddata;
    s_siddata_frames = sidframes;
    s_playback_active = true;
}

// non-blocking sid playback service
// must be called once per tick by main loop
void streaming_sid_service(void) {
    if (s_playback_active) {
        if (s_siddata_frames > 0) {
            for (uint8_t i = 0; i < 25; i++) {
                POKE(SID1 + i, FAR_PEEK(s_siddata_address));
                s_siddata_address++;
            }
            s_siddata_frames--;
        } else {
            s_playback_active = false;
            clearSIDRegisters();
        }
    }
}

// for blocking sid playback with keyboard interrupt
void playback(uint32_t siddata, uint16_t sidframes) {
    while (sidframes) {
        if (isTimerDone()) {
            for (uint8_t i = 0; i < 25; i++) {
                POKE(SID1 + i, FAR_PEEK(siddata));
                siddata++;
            }
            timer_service();

            sidframes--;
        }
        kernelNextEvent();
        if (kernelEventData.type == kernelEvent(key.PRESSED)) {
            break;
        }
    }
    clearSIDRegisters();
}

bool is_sid_playing(void) {
    return s_playback_active;
}
void stop_sid_playback(void) {
    s_playback_active = false;
    clearSIDRegisters();
}
