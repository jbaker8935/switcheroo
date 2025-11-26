#if !defined(SRC_PLAYSID_H__)
#define SRC_PLAYSID_H__
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define SID_SYS1  0xD6A1  //SID system register 1

//main base SID addresses
#define SID1           0xD400
#define SID2           0xD500
void playback(uint32_t siddata, uint16_t sidframes);
void setMonoSID();
void clearSIDRegisters();
void schedule_playback(uint32_t siddata, uint16_t sidframes);
void streaming_sid_service(void);
bool is_sid_playing(void);
void stop_sid_playback(void);
#endif // SRC_PLAYSID_H__
