#include "ATD_CubeSlider_Dummy.h"
#include "ATD_CubeSlider_A.h"
//#include "Nikon_CubeSlider_90i.h"
#include "gci_utils.h"

#include "icsviewer_window.h"

#include <utility.h>

#include "profile.h"
#include "config.h"

static int error_handler (UIModule *module, const char *title, const char *error_string, void *callback_data)
{
	MessagePopup("Error", error_string);
	
	return UIMODULE_ERROR_NONE;
}

static void OnCloseOrHideEvent (UIModule *module, void *data)
{
	QuitUserInterface(0);
}

int CVICALLBACK StressTestCubes(void *callback)
{
	int i, number_of_cubes;
	FluoCubeManager* cm = (FluoCubeManager*) callback;

	cube_manager_get_number_of_cubes(cm, &number_of_cubes);

	while(1) {
		for(i=1; i <= number_of_cubes; i++) {
			cube_manager_move_to_position(cm, i);
			Delay(0.1);
			ProcessSystemEvents();
		}
	}
}

int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
                       LPSTR lpszCmdLine, int nCmdShow)
{
	int stress=1;
	DeviceInfo info;
    FluoCubeManager* cm = NULL;
	
  	if (InitCVIRTE (hInstance, 0, 0) == 0)
  		return -1;    /* out of memory */

	if(ShowStandaloneIniDevicesController(CONFIG_INI_FILEPATH,
		"ATD_CubeSlider_Dummy;ATD_CubeSlider_A;", &stress, &info) < 0) {
		return -1;
	}

	if(strcmp(info.type, "ATD_CubeSlider_Dummy") == 0)
	{
		cm = Manual_fluo_cube_manager_new(info.name, "CubeSlider",
			TOPLEVEL_SOURCE_DIRECTORY "\\DataDir", "DumyCubeData1.ini", error_handler, NULL);
	}
	else if(strcmp(info.type, "ATD_CubeSlider_A") == 0)
	{
		cm = atd_cubeslider_a_new(info.name, "CubeSlider",
			TOPLEVEL_SOURCE_DIRECTORY "\\DataDir", "CubeData1.ini", error_handler, NULL);
	}
	else
	{
		MessagePopup("Error", "Device does not exist ?");
		return -1;
	}

	ui_module_main_panel_hide_or_close_handler_connect (UIMODULE_CAST(cm), OnCloseOrHideEvent, NULL);

	cube_manager_initialise(cm, 0);
	cube_manager_hardware_initialise(cm);

	ui_module_display_main_panel(UIMODULE_CAST(cm));

	if(stress) 
		CmtScheduleThreadPoolFunction (gci_thread_pool(), StressTestCubes, cm, NULL);

	RunUserInterface();
	
	cube_manager_destroy(cm);
	
  	return 0;
}
