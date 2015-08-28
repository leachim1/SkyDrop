#include "audio.h"
#include "../drivers/buzzer.h"
#include "fc.h"

Timer audio_timer;

volatile float next_tone = 0;
volatile uint16_t next_length = 0;
volatile uint16_t next_pause = 0;

volatile bool delay_on = false;

#define PERIOD_SOUND		0
#define PERIOD_PAUSE		1
volatile uint8_t audio_period = PERIOD_SOUND;

volatile bool audio_demo = false;
volatile float audio_demo_val = 0;

volatile bool audio_beep = false;

#define AUDIO_LOW_PASS		10.0

//linear aproximation between two points
uint16_t get_near(float vario, volatile uint16_t * src)
{
	vario = vario * 2; //1 point for 50cm
	float findex = floor(vario) +  20;
	float m = vario - floor(vario);

	uint8_t index = findex;

	if (findex > 39)
	{
		index = 39;
		m = 1.0;
	}

	if (findex < 0)
	{
		index = 0;
		m = 0.0;
	}

//	m = round(m * 10) / 10.0;

	int16_t start = src[index];

	start = start + (float)((int16_t)src[index + 1] - start) * m;

	return start;
}

ISR(AUDIO_TIMER_OVF)
{
	if (audio_period == PERIOD_SOUND)
	//pause start
	{
		buzzer_set_vol(0);

		if (next_pause == 0)
		{
			audio_timer.Stop();
			delay_on = false;

			return;
		}

		audio_timer.SetTop(next_pause);

		audio_period = PERIOD_PAUSE;
	}
	else
	//sound start
	{
		if (next_tone > 0)
		{
			buzzer_set_freq(next_tone);
			buzzer_set_vol(fc.audio_volume);
		}

		if (next_length == 0)
		{
			audio_timer.Stop();
			delay_on = false;

			return;
		}

		audio_timer.SetTop(next_length);

		audio_period = PERIOD_SOUND;
	}
}

void audio_set_tone(uint16_t tone)
{
	if (tone == 0)
	{
		next_tone = 0;

		//buzzer is running continuously turn off sound
		if (!delay_on)
		{
			//disable sound output
			buzzer_set_vol(0);
		}
	}
	else
	{
		if (next_tone != 0)
		{
			float tmp = ((float)tone - next_tone) / AUDIO_LOW_PASS;
			next_tone = next_tone + tmp;
		}
		else
		{
			next_tone = tone;
		}

		//buzzer is running continuously update freq now
		if (delay_on == false)
		{
			buzzer_set_freq(next_tone);
			buzzer_set_vol(fc.audio_volume);
		}

		//fluid update is enabled
		if (fc.audio_fluid && audio_period == PERIOD_SOUND)
			buzzer_set_freq(next_tone);
	}
}

void audio_set_delay(uint16_t length, uint16_t pause)
{
	//Continuous sound (sink)
	if (pause == 0 || length == 0)
	{
		next_length = 0;
		next_pause = 0;

	}
	else
	//with pauses (lift)
	{
		//convert from ms and ticks
		next_length = 31 * length;
		next_pause = 31 * pause;

		//if previous sound was continuous (audio_timer is not working)
		if (delay_on == false)
		{
			//restart timer counter
			audio_timer.SetValue(1);

			//set new tone + enable sound
			buzzer_set_freq(next_tone);
			buzzer_set_vol(fc.audio_volume);

			audio_timer.SetTop(next_length);

			//start timer
			audio_timer.Start();

			//set the period state state
			audio_period = PERIOD_SOUND;
		}

		//we have pauses enabled
		delay_on = true;
	}
}

void audio_init()
{
	AUDIO_TIMER_PWR_ON;
	audio_timer.Init(AUDIO_TIMER, timer_div1024);
	audio_timer.EnableInterrupts(timer_overflow);
}

void audio_vario_step(float vario)
{
	uint16_t freq;
	uint16_t length;
	uint16_t pause;

	//GET fresh values from table
	// - climb is float in m/s
	if ((vario * 100 >= fc.audio_lift || vario * 100 <= fc.audio_sink) && (fc.audio_volume > 0))
	{
		if (vario * 100 >= fc.audio_lift)
		{
			//get frequency from the table
			freq = get_near(vario, fc.buzzer_freq);
			length = get_near(vario, fc.buzzer_length);
			pause = get_near(vario, fc.buzzer_pause);
		}
		else //XXX: this is hack we need to fix this, so beeps can be used in sink too
		{
			//get frequency from the table
			freq = get_near(vario, fc.buzzer_freq);
			length = 0;
			pause = 0;
		}

	}
	else
	//no threshold was exceeded -> silent
	{
		freq = 0;
		length = 0;
		pause = 0;
	}

	//update audio with new settings
	audio_set_tone(freq);
	audio_set_delay(length, pause);
}

void audio_step()
{
	//sound effect is high priority
	if (audio_beep)
	{
		audio_beep_loop();
		return;
	}

	//vario demo sound suppress standard vario sound
	if (audio_demo)
	{
		audio_vario_step(audio_demo_val);
		return;
	}

	//barometer data are valid now
	if (fc.baro_valid)
	{
		//audio is suppressed due auto start
		if (fc.audio_supress)
		{
			//vario in flight -> enable sound
			if ( fc.autostart_state == AUTOSTART_FLIGHT)
				audio_vario_step(fc.vario);

			return;
		}
			audio_vario_step(fc.vario);
	}
}

void audio_off()
{
	audio_set_tone(0);
	audio_set_delay(0, 0);
}

volatile const uint16_t * audio_beep_tone_ptr;
volatile const uint16_t * audio_beep_length_ptr;
volatile uint8_t audio_beep_index;
volatile uint8_t audio_beep_len;
volatile uint16_t audio_beep_duration;

void audio_beep_start(const beep_t * beep)
{
	audio_beep = true;

	audio_beep_len = pgm_read_byte(&beep->length);
	audio_beep_tone_ptr = (const uint16_t*)pgm_read_word(&beep->tone_ptr);
	audio_beep_length_ptr = (const uint16_t*)pgm_read_word(&beep->length_ptr);
	audio_beep_index = 0;

	DEBUG("beep start %d %d %d\n", audio_beep_len, audio_beep_tone_ptr, audio_beep_length_ptr);
}

void audio_beep_next()
{
	uint16_t tone = pgm_read_word(&audio_beep_tone_ptr[audio_beep_index]);
	audio_beep_duration = pgm_read_word(&audio_beep_length_ptr[audio_beep_index]);
	audio_beep_index++;

	buzzer_set_vol(fc.audio_volume);
	buzzer_set_freq(tone);

	DEBUG("beep next %d %d\n", tone, audio_beep_index);
}

//audio_step @ 100Hz
#define AUDIO_STEP_MS	10

void audio_beep_loop()
{
	DEBUG("beep loop %d %d\n", audio_beep_duration, audio_beep_index);
	if (audio_beep_duration > AUDIO_STEP_MS)
	{
		audio_beep_duration -= AUDIO_STEP_MS;
	}
	else
	{
		if (audio_beep_index == audio_beep_len)
		{
			audio_beep = false;
			audio_off();
		}
		else
			audio_beep_next();
	}
};
