#include <stdint.h>
#include <stddef.h>
#include "../src/sram_assets.h"
typedef enum  {
    SOUND_ID_MOVE = 0,
    SOUND_ID_RESET_BOARD = 1,
    SOUND_ID_WIN = 2,
    SOUND_ID_LOSS = 3,
    SOUND_ID_START = 4,
    SOUND_ID_COUNT = 5
} sound_id_t;

typedef struct {
    uint32_t sram_addr;
    uint16_t size;
} sound_data_t;

static const sound_data_t kSoundData[SOUND_ID_COUNT] = {
    { SRAM_SOUND_MOVE, SOUND_MOVE_SIZE },
    { SRAM_SOUND_RESET_BOARD, SOUND_RESET_BOARD_SIZE },
    { SRAM_SOUND_WIN, SOUND_WIN_SIZE },
    { SRAM_SOUND_LOSS, SOUND_LOSS_SIZE },
    { SRAM_SOUND_START, SOUND_START_SIZE }
};
void init_sounds(void);
void play_sound(sound_id_t id);

