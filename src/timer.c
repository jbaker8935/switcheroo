#ifdef AI_AGENT_HOST_TEST
#include "../tests/include/f256lib_host.h"
#else
#include "f256lib.h"
#endif
#include "../src/timer.h"

static uint16_t alarm_ticks = 0;



void setTimer0()
{
	resetTimer0();
	POKE(T0_CMP_CTR, T0_CMP_CTR_RECLEAR); //when the target is reached, bring it back to value 0x000000
	POKE(T0_CMP_L,T0_TICK_CMP_L);POKE(T0_CMP_M,T0_TICK_CMP_M);POKE(T0_CMP_H,T0_TICK_CMP_H); //inject the compare value as max value
}

void resetTimer0()
{
	POKE(T0_CTR, CTR_CLEAR);
	POKE(T0_CTR, CTR_UPDOWN | CTR_ENABLE);
	POKE(T0_PEND,0x10); //clear pending timer0 
}
uint32_t readTimer0()
{
	return (uint32_t)((PEEK(T0_VAL_H)))<<16 | (uint32_t)((PEEK(T0_VAL_M)))<<8 | (uint32_t)((PEEK(T0_VAL_L)));
}

bool isTimerDone()
{
	return (PEEK(T0_PEND) & 0x10) != 0;
}

uint16_t getAlarmTicks() {
	return alarm_ticks;
}
	
void setAlarm(uint16_t ticks) {
	setTimer0();
	alarm_ticks = ticks;
}

bool checkAlarm() {
	
	if (isTimerDone()) {
		if (alarm_ticks > 0) {
			alarm_ticks--;
		}
		setTimer0(); 
	}
	
	return (alarm_ticks == 0);
}
