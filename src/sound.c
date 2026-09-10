#include "sound.h"

#include "sram_assets.h"
#include "text_display.h"
#include "timer.h"
#include "playsid.h"
#include "f256lib.h"

#define VS_SCI_CTRL 0xD700
#define VS_SCI_ADDR 0xD701
#define VS_SCI_DATA 0xD702   // 2 bytes
#define VS_FIFO_STAT 0xD704  // 2 bytes
#define VS_FIFO_DATA 0xD707

// VS1053b CTRL modes
#define CTRL_Start 0x01  // 1: start transfer, followed by 0 to stop
#define CTRL_RWn 0x02    // 1: read mode, 0: write mode
#define CTRL_Busy 0x80   // if set, spi transfer is busy

#define VS_SCI_ADDR 0xD701
// VS1053b specific SCI addresses
#define VS_SCI_ADDR_MODE 0x00
#define VS_SCI_ADDR_STATUS 0x01
#define VS_SCI_ADDR_BASS 0x02
#define VS_SCI_ADDR_CLOCKF 0x03
#define VS_SCI_ADDR_WRAM 0x06
#define VS_SCI_ADDR_WRAMADDR 0x07
#define VS_SCI_ADDR_HDAT0 0x08
#define VS_SCI_ADDR_HDAT1 0x09
#define VS_SCI_ADDR_AIADDR 0x0A
#define VS_SCI_ADDR_VOL 0x0B

#define SDI_MAX_TRANSFER_SIZE 32
#define PAR_END_FILL_BYTE 0x1e06 /* VS1063, VS1053 */
#define SDI_END_FILL_BYTES 2050  /* 2050 bytes of endFillByte */
#define SM_CANCEL 0x0008         /* bit 3 */
#define SM_RESET 0x0004          /* bit 2 */

uint16_t vs1053_read_sci(uint8_t addr) {
    POKE(VS_SCI_ADDR, addr);
    POKE(VS_SCI_CTRL, CTRL_Start | CTRL_RWn);
    POKE(VS_SCI_CTRL, 0);
    while (PEEK(VS_SCI_CTRL) & CTRL_Busy)
        ;

    return ((uint16_t)PEEKW(VS_SCI_DATA));
}

void vs1053_write_sci(uint8_t addr, uint16_t data) {
    POKE(VS_SCI_ADDR, addr);
    POKEW(VS_SCI_DATA, data);
    POKE(VS_SCI_CTRL, CTRL_Start);
    POKE(VS_SCI_CTRL, 0);
    // while (PEEK(VS_SCI_CTRL) & CTRL_Busy)
    //     ;
    return;
}

/*
  Read 16-bit value from addr.
*/
uint16_t ReadVS10xxMem(uint16_t addr) {
    vs1053_write_sci(VS_SCI_ADDR_WRAMADDR, addr);
    return vs1053_read_sci(VS_SCI_ADDR_WRAM);
}

void init_sounds(void) {
    // init codec
    POKE(0xD620, 0x1F);
    POKE(0xD621, 0x2A);
    POKE(0xD622, 0x01);
    while (PEEK(0xD622) & 0x01)
        ;

    // Set volume to a reasonable level

    POKE(0xD620, 0x48);
    POKE(0xD621, 0x05);
    POKE(0xD622, 0x01);
    while (PEEK(0xD622) & 0x01)
        ;

    // boost clock
    // target the clock register
    POKE(VS_SCI_ADDR, VS_SCI_ADDR_CLOCKF);
    // aim for 4.5X clock multiplier, no frills
    POKE(VS_SCI_DATA, 0x00);
    POKE(VS_SCI_DATA + 1, 0xc0);
    // trigger the command
    POKE(VS_SCI_CTRL, 1);
    POKE(VS_SCI_CTRL, 0);
    // check to see if it's done
    while (PEEK(VS_SCI_CTRL) & CTRL_Busy)
        ;
}

bool isWave2(void) {
    uint8_t mid;
    mid = PEEK(0xD6A7) & 0x3F;
    return (mid == 0x22 || mid == 0x11);  // 22 is Jr2 and 11 is K2
}

void play_sound(sound_id_t id) {
    // first check for vs1053 presence
    if (isWave2()) {
        // if(!checkAlarm(TIMER_ALARM_SOUND))
        // {
        //     //sound is already playing
        //     return;
        // }
        char str_id[] = {' ', 0};
        str_id[0] = '0' + (char)id;

        // setAlarm(TIMER_ALARM_SOUND,T0_TICK_FREQ); //set alarm for 1s to avoid overlapping sounds
        uint32_t sound_addr = kSoundData[MP3_MODE][id].sram_addr;
        uint16_t sound_size = kSoundData[MP3_MODE][id].size;
        uint16_t rawFIFOCount = PEEKW(VS_FIFO_STAT);
        uint16_t bytesToTopOff = 2048 - (rawFIFOCount & 0x0FFF);  // found how many bytes are left in the 2KB buffer
        uint8_t endFillByte = ReadVS10xxMem(PAR_END_FILL_BYTE);

        while (sound_size > 0) {
            for (uint8_t i = 0; i < bytesToTopOff && sound_size > 0; i++) {
                POKE(VS_FIFO_DATA, FAR_PEEK(sound_addr));
                sound_addr++;
                sound_size--;
            }
            // wait for some space
            do {
                rawFIFOCount = PEEKW(VS_FIFO_STAT);
            } while ((rawFIFOCount & 0x0FFF) > 1792);
            bytesToTopOff = 2048 - (rawFIFOCount & 0x0FFF);  // found how many bytes are left in the 2KB buffer
        }
        sound_size = SDI_END_FILL_BYTES;

        while (sound_size > 0) {
            for (uint8_t i = 0; i < bytesToTopOff && sound_size > 0; i++) {
                POKE(VS_FIFO_DATA, endFillByte);
                sound_addr++;
                sound_size--;
            }
            // wait for some space
            do {
                rawFIFOCount = PEEKW(VS_FIFO_STAT);
            } while ((rawFIFOCount & 0x0FFF) > 1792);
            bytesToTopOff = 2048 - (rawFIFOCount & 0x0FFF);  // found how many bytes are left in the 2KB buffer
        }
    } else {
        // Use SID playback as fallback
        schedule_playback(kSoundData[SID_MODE][id].sram_addr, kSoundData[SID_MODE][id].size);
    }
}
