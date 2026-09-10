#if !defined(SRC_PLAYSID_H__)
#define SRC_PLAYSID_H__
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

bool playback(uint32_t siddata, uint16_t sidframes);
void schedule_playback(uint32_t siddata, uint16_t sidframes);
void streaming_sid_service(void);
bool is_sid_playing(void);
void stop_sid_playback(void);
#endif // SRC_PLAYSID_H__
