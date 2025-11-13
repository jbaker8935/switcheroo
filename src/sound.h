#include <stdint.h>
#include <stddef.h>
#define SRAM_SOUND_LOSS 0x6F640
#define SOUND_LOSS_SIZE 5856u
#define SRAM_SOUND_MOVE 0x70D20
#define SOUND_MOVE_SIZE 4553u
#define SRAM_SOUND_RESET_BOARD 0x71EF0
#define SOUND_RESET_BOARD_SIZE 7166u
#define SRAM_SOUND_WIN 0x73AF0
#define SOUND_WIN_SIZE 7686u

typedef enum  {
    SOUND_ID_MOVE = 0,
    SOUND_ID_RESET_BOARD = 1,
    SOUND_ID_WIN = 2,
    SOUND_ID_LOSS = 3,
    SOUND_ID_COUNT = 4
} sound_id_t;

typedef struct {
    uint32_t sram_addr;
    uint16_t size;
} sound_data_t;

static const sound_data_t kSoundData[SOUND_ID_COUNT] = {
    { SRAM_SOUND_MOVE, SOUND_MOVE_SIZE },
    { SRAM_SOUND_RESET_BOARD, SOUND_RESET_BOARD_SIZE },
    { SRAM_SOUND_WIN, SOUND_WIN_SIZE },
    { SRAM_SOUND_LOSS, SOUND_LOSS_SIZE }
};
void init_sounds(void);
void play_sound(sound_id_t id);

