#include "task_active.h"
#include "../gui/gui.h"
#include "../fc/fc.h"
#include "../gui/splash.h"

void task_active_init()
{
	DEBUG(" *** THIS IS TASK ACTIVE ***\n");

	//init gui
	gui_init();
	gui_trigger_backlight();

	if (fw_info.test_pass == APP_INFO_TEST_hex)
	{
		gui_splash_set_mode(SPLASH_ON);
		gui_switch_task(GUI_SPLASH);
		gui_force_loop();
	}
	else
	{
		gui_switch_task(GUI_FTEST);
	}

	if (storage_init())
	{
		//Handle update files
		if (LoadEEPROM())
		{
			gui_load_eeprom();
			f_unlink("UPDATE.EE");
		}
		f_unlink("UPDATE.FW");
	}

	//init flight computer
	fc_init();
}

void task_active_stop()
{
	fc_deinit();
	gui_stop();
	storage_deinit();
}

void task_active_loop()
{

	fc_step();

	gui_loop();

	storage_step();

}

void task_active_irqh(uint8_t type, uint8_t * buff)
{
	switch (type)
	{
	case(TASK_IRQ_USB):
		if (*buff && fc.usb_mode == USB_MODE_MASSSTORAGE)
			task_set(TASK_USB);
		break;

	default:
		gui_irqh(type, buff);
	}
}
