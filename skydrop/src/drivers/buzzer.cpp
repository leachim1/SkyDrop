#include "buzzer.h"

Timer buzzer_timer;

//V = (val / 0xFFF) * 3.3V

#define DAC_MIN		745		//0.6V
#define DAC_MAX		1985	//1.6V
#define DAC_DELTA	(DAC_MAX - DAC_MIN)

ISR(BUZZER_TIMER_OVF)
{

}

void buzzer_set_vol(uint8_t vol)
{
	uint16_t val = DAC_MIN + (DAC_DELTA / 100) * vol;

	DEBUG("vol = %d\n", vol);

	if (val > DAC_MAX)
		val = DAC_MAX;

	if (val < DAC_MIN)
		val = DAC_MIN;

	DacSetCh1(val);

	if (vol == 0)
		buzzer_timer.Stop();
	else
		buzzer_timer.Start();
}

void buzzer_set_freq(uint16_t freq_hz)
{
	DEBUG("freq = %d\n", freq_hz);

	if (freq_hz == 0)
	{
		buzzer_timer.Stop();
		return;
	}

	uint16_t buzzer_period = 31250 / freq_hz;

	buzzer_timer.SetCompare(timer_A, buzzer_period / 5); //20% duty cycle = battery saving
	buzzer_timer.SetTop(buzzer_period);
	if (buzzer_timer.GetValue() > buzzer_period)
		buzzer_timer.SetValue(1);

	buzzer_timer.Start();
}

void buzzer_init()
{
	BUZZER_TIMER_PWR_ON;
	BUZZER_VOL_DAC_PWR_ON;

	DacInit(BUZZER_VOL_DAC);
	DacSetReference(dac_avcc);

	//buzzer
	buzzer_timer.Init(BUZZER_TIMER, timer_div1024);
	buzzer_timer.SetMode(timer_pwm);
	buzzer_timer.EnableOutputs(timer_A);
	buzzer_timer.EnableInterrupts(timer_overflow);

	buzzer_set_vol(0);
}
