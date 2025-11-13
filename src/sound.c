#include "f256lib.h"
#include "../src/sound.h"

#define VS_SCI_CTRL  0xD700
#define VS_SCI_ADDR  0xD701
#define VS_SCI_DATA  0xD702   //2 bytes
#define VS_FIFO_STAT 0xD704   //2 bytes
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

EMBED(sound_loss,"../assets/sounds/loss.ogg",0x6F640u);
EMBED(sound_move,"../assets/sounds/move.ogg",0x71150u);
EMBED(sound_reset_board,"../assets/sounds/reset_board.ogg",0x72620u);
EMBED(sound_win,"../assets/sounds/win.ogg",0x74730u);

void init_sounds(void)
{
    //init codec
	POKE(0xD620, 0x1F);
	POKE(0xD621, 0x2A);
	POKE(0xD622, 0x01);
	while(PEEK(0xD622) & 0x01)
    ;

    // Set volume to a reasonable level

    POKE(0xD620, 0x48);
    POKE(0xD621, 0x05);
    POKE(0xD622, 0x01);
    while(PEEK(0xD622) & 0x01)
        ;

    // boost clock
    //target the clock register
    POKE(VS_SCI_ADDR,0x03);
    //aim for 2.5X clock multiplier, no frills
    POKE(VS_SCI_DATA,0x00);
    POKE(VS_SCI_DATA+1,0xc0);
    //trigger the command
    POKE(VS_SCI_CTRL,1);
    POKE(VS_SCI_CTRL,0);
    //check to see if it's done
        while (PEEK(VS_SCI_CTRL) & 0x80)
            ;    
}

bool isWave2(void)
{
    uint8_t mid;
    mid = PEEK(0xD6A7)&0x3F;
    return (mid == 0x22 || mid == 0x11); //22 is Jr2 and 11 is K2
}

void play_sound(sound_id_t id)
{
    return;
    // first check for vs1053 presence
    // if (!isWave2())
    // return;


    uint32_t sound_addr = kSoundData[id].sram_addr;
    uint16_t sound_size = kSoundData[id].size;

    while(sound_size > 0)
    {

        POKE(VS_FIFO_DATA, FAR_PEEK(sound_addr));
        sound_addr++;
        sound_size--;
    }
    sound_size = SDI_END_FILL_BYTES;
    while(sound_size > 0)
    {

        POKE(VS_FIFO_DATA, 0x00);
        sound_size--;
    }

}
