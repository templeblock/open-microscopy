//#include "iscope90i.h"
#include "cvixml.h"
#include <utility.h>
#include "stage.h"
#include "stage_ui.h"
#include "Corvus_ui.h"	 //Corvus XYZ
//#include "Corvus_90i_ui.h"   //Corvus XY with 90i Z
#include "string_utils.h"
#include "gci_utils.h"
#include "password.h"
#include "xml_utils.h"

#include "GL_CVIRegistry.h"
#include "toolbox.h"
#include "asynctmr.h"
#include <ansi_c.h> 

/////////////////////////////////////////////////////////////////////////////
// XYZ stage module for Marzhauser stage - GP & RJL Jan 2006
//
/////////////////////////////////////////////////////////////////////////////

//#define ASYNCH_TIMER

int  stage_show_errors(Stage* stage, int val)
{
	stage->_show_errors = val;
	return STAGE_SUCCESS;
}


int send_stage_error_text (Stage* stage, char fmt[], ...)
{
	char *buf, *buffer_start;
	char tmp_buf[256];
	
	va_list ap;
	char *p, *sval;
	int ival;
	double dval;
	
	if (!stage->_show_errors)
		return STAGE_SUCCESS;
	
	if(stage == NULL || stage->_error_handler == NULL)
		return STAGE_ERROR;
	
	buffer_start = (char*) malloc(1024);
	buf = buffer_start;
	
	va_start(ap, fmt);
	
	for (p = fmt; *p; p++) {
	
		if (*p != '%') {
			*buf++ = *p;
			continue;
		}
		
		*buf = '\0';
		
		switch (*++p) {
			case 'd':
			case 'i':
				ival = va_arg(ap, int);
				sprintf(tmp_buf, "%d", ival);
				strcat(buf, tmp_buf);
				buf+=strlen(tmp_buf);
				break;
				
			case 'x':
				ival = va_arg(ap, int);
				sprintf(tmp_buf, "%x", ival);
				strcat(buf, tmp_buf);
				buf+=strlen(tmp_buf);
				break;
				
			case 'f':
				dval = va_arg(ap, double);
				sprintf(tmp_buf, "%f", dval);
				strcat(buf, tmp_buf);
				buf+=strlen(tmp_buf);
				break;
				
			case 's':
				sval = va_arg(ap, char *);
				strcat(buf, sval);
				buf+=strlen(sval);
				break;
				
			default:
				*buf++ = *p;
				break;
		}
		
	}
	
	*buf = '\0';
	va_end(ap);
	SetSystemAttribute (ATTR_DEFAULT_MONITOR, 1);
	stage->_error_handler(buffer_start, stage);
	free(buffer_start);
	
	return STAGE_SUCCESS;
}


static void error_handler (char *error_string, Stage *stage)
{
	MessagePopup("Stage Error", error_string); 
}


static void stage_read_or_write_main_panel_registry_settings(Stage *stage, int write)
{
	char buffer[500];
	int visible;

	if(stage == NULL || stage->_main_ui_panel == -1)
		return;

	// load or save panel positions
	
	// make sure the panel is not minimised as this will put v. big values
	// in the reg and at next startup the panel will not be visible!	
	if(write == 1) {
		GetPanelAttribute (stage->_main_ui_panel, ATTR_VISIBLE, &visible);
		if(!visible)
			return;
	
		SetPanelAttribute (stage->_main_ui_panel, ATTR_WINDOW_ZOOM, VAL_NO_ZOOM);
	}
	
	sprintf(buffer, "software\\GCI\\Microscope\\Stage\\%s\\MainPanel\\", stage->_name);
	
	checkRegistryValueForPanelAttribInt(write, REGKEY_HKCU, buffer,  "top",    stage->_main_ui_panel, ATTR_TOP);
	checkRegistryValueForPanelAttribInt(write, REGKEY_HKCU, buffer,  "left",   stage->_main_ui_panel, ATTR_LEFT);
}


static void stage_read_or_write_params_panel_registry_settings(Stage *stage, int write)
{
	char buffer[500];
	int visible;

	if(stage == NULL || stage->_params_ui_panel == -1)
		return;

	// load or save panel positions
	
	// make sure the panel is not minimised as this will put v. big values
	// in the reg and at next startup the panel will not be visible!	
	if(write == 1) {
		GetPanelAttribute (stage->_params_ui_panel, ATTR_VISIBLE, &visible);
		if(!visible)
			return;
	
		SetPanelAttribute (stage->_params_ui_panel, ATTR_WINDOW_ZOOM, VAL_NO_ZOOM);
	}
	
	sprintf(buffer, "software\\GCI\\Microscope\\Stage\\%s\\ParamsPanel\\", stage->_name);
	
	checkRegistryValueForPanelAttribInt(write, REGKEY_HKCU, buffer,  "top",    stage->_params_ui_panel, ATTR_TOP);
	checkRegistryValueForPanelAttribInt(write, REGKEY_HKCU, buffer,  "left",   stage->_params_ui_panel, ATTR_LEFT);
}


int stage_signal_close_handler_connect (Stage* stage, STAGE_EVENT_HANDLER handler, void *callback_data)
{
	if( GCI_Signal_Connect(&(stage->signal_table), "Close", handler, callback_data) == SIGNAL_ERROR) {
		send_stage_error_text(stage, "Can not connect signal handler for Close signal");
		return STAGE_ERROR;
	}

	return STAGE_SUCCESS;
}


int stage_signal_XYZ_change_handler_connect (Stage* stage, STAGE_EVENT_HANDLER handler, void *callback_data)
{
	if( GCI_Signal_Connect(&(stage->signal_table), "XYZ_Changed", handler, callback_data) == SIGNAL_ERROR) {
		send_stage_error_text(stage, "Can not connect signal handler for XYZ_Changed signal");
		return STAGE_ERROR;
	}

	return STAGE_SUCCESS;
}

int stage_signal_initialise_extents_start_handler_connect (Stage* stage, STAGE_EVENT_HANDLER handler, void *callback_data)
{
	if( GCI_Signal_Connect(&(stage->signal_table), "XYInitExtentsStart", handler, callback_data) == SIGNAL_ERROR) {
		send_stage_error_text(stage, "Can not connect signal handler for XYInitExtentsStart signal");
		return STAGE_ERROR;
	}

	return STAGE_SUCCESS;
}


int stage_signal_initialise_extents_end_handler_connect (Stage* stage, STAGE_EVENT_HANDLER handler, void *callback_data)
{
	if( GCI_Signal_Connect(&(stage->signal_table), "XYInitExtentsEnd", handler, callback_data) == SIGNAL_ERROR) {
		send_stage_error_text(stage, "Can not connect signal handler for XYInitExtentsEnd signal");
		return STAGE_ERROR;
	}

	return STAGE_SUCCESS;
}


Stage* stage_new()
{
	Stage* stage = (Stage*) malloc(sizeof(Stage));
	
	if (stage != NULL) 
		CmtNewLock (NULL, 0, &(stage->_lock) );
	
	return stage;
}


static int STAGE_PTR_MARSHALLER (void *handler, void *callback_data, GCI_Signal_Arg* args)
{
	typedef void (*HANDLER) (Stage*, void *);
	HANDLER func;

	assert(handler != NULL);
	
	func = (HANDLER) handler;
	
	func ( (Stage *) args[0].void_ptr_data, callback_data);
	
	return SIGNAL_SUCCESS;	
}


int CVICALLBACK OnStageTimerTick (int panel, int control, int event,
		void *callbackData, int eventData1, int eventData2)
{
	Stage *stage = (Stage *) callbackData;
	double t1;
	
    switch (event)
    {
        case EVENT_TIMER_TICK:
        	t1 = Timer();
			stage_plot_current_position(stage);		
			//printf("Stage tick %f\n", Timer()-t1);
            break;
    }
    
    return 0;
}

int stage_enable_focus_drive_callback(Stage* stage, int enable)
{
	return focus_drive_enable_callback(stage->focus_drive, enable);
}

int stage_params_init(Stage* stage)
{
	//Corvus XYZ
	stage->_params_ui_panel = FindAndLoadUIR(0, "Corvus_ui.uir", XYZ_PARAMS);  
	//Corvus XY with 90i Z
	//stage->_params_ui_panel = FindAndLoadUIR(0, "Corvus_90i_ui.uir", XYZ_PARAMS);  

	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_BAUD, OnSetBaud, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_XY_CAL_SPEED, OnCalSpeed, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_XY_CAL_SPEED_2, OnCalSpeed, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_MOVETYPE, OnSetMoveType, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_SEND, OnParamsSend, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_READ, OnParamsRead, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_LOAD, OnParamsLoad, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_SAVE, OnParamsSave, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_CLOSE, OnParamsClose, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_X_ACC, OnAcceleration, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_Y_ACC, OnAcceleration, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_X_SPEED, OnSpeed, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_Y_SPEED, OnSpeed, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_X_PITCH, OnPitch, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_Y_PITCH, OnPitch, stage) < 0)
		return STAGE_ERROR;
  	
#ifndef XY_ONLY
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_Z_PITCH, OnZPitch, stage) < 0)
		return STAGE_ERROR;
#endif
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_X_REVERSED, OnXdirReversed, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_Y_REVERSED, OnYdirReversed, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_Z_REVERSED, OnZdirReversed, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_X_ENABLED, OnXenabled, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_Y_ENABLED, OnYenabled, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_Z_ENABLED, OnZenabled, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_Z_REVERSED, OnZdirReversed, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_Z_UPPER_LIMIT, OnZLimitChange, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_Z_LOWER_LIMIT, OnZLimitChange, stage) < 0)
		return STAGE_ERROR;
  	
#ifdef XY_ONLY
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_Z_SPEED, OnZSpeedChange, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_Z_TOLERANCE, OnZToleranceChange, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_DIAL_ENABLED, OnZDialEnabled, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_SENSITIVITY, OnZDialSensitivityChange, stage) < 0)
		return STAGE_ERROR;
#else
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_Z_ACC, OnAcceleration, stage) < 0)
		return STAGE_ERROR;
  	
	if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_Z_SPEED, OnSpeed, stage) < 0)
		return STAGE_ERROR;
#endif  	

	//if ( InstallCtrlCallback (stage->_params_ui_panel, XYZ_PARAMS_TEST, OnZtest, stage) < 0)
	//	return STAGE_ERROR;
  	
	stage_load_default_parameters(stage);
	
	return STAGE_SUCCESS;
}

int stage_init (Stage* stage)
{
	Coord coord;
	
	stage_set_name(stage, "Stage");
	stage_set_description(stage, "Stage Description");
	
	stage->_abort_move = 0;
	stage->_timer = -1;
	stage->_init_aborted = 0;
	stage->_advanced_view = 0;
	stage->_software_limit = DEFAULT_SOFTWARE_LIMIT;
	stage->_cal_speed_1 = 10.0;
	stage->_cal_speed_2 = 0.25;
	stage->_port = 1;
	stage->_baud_rate = 9600;
	stage->_move_type = SAFE;
	stage->_joystick_speed = 2.0;
	stage->_limits.min_x = 0.0;
	stage->_limits.min_y = 0.0;
	stage->_limits.min_z = 0.0;
	stage->_limits.max_x = 0.0;
	stage->_limits.max_y = 0.0;
	stage->_limits.max_z = 0.0;
	stage->_roi.min_x = 0.0;
	stage->_roi.min_y = 0.0;
	stage->_roi.min_z = 0.0;
	stage->_roi.max_x = 0.0;
	stage->_roi.max_y = 0.0;
	stage->_roi.max_z = 0.0;
	stage->_powered_up = 0;
	stage->_data_dir = NULL;	  
	stage->_main_ui_panel = -1;
	stage->_params_ui_panel = -1;
	
	stage->_current_pos.x = 0.0;
	stage->_current_pos.y = 0.0;
	stage->_current_pos.z = 0.0;

	coord.x = 0.0;
	coord.y = 0.0;
	coord.z = 0.0;
	
	stage->_datum = coord;
	stage->_focus_offset = 0.0;
	
	stage->_enabled_axis[XAXIS] = 0;
	stage->_enabled_axis[YAXIS] = 0;
	stage->_enabled_axis[ZAXIS] = 0;

	stage->_acceleration[XAXIS] = 0.0;
	stage->_acceleration[YAXIS] = 0.0;
	stage->_acceleration[ZAXIS] = 0.0;
	
	stage->_speed[XAXIS] = 0.0;
	stage->_speed[YAXIS] = 0.0;
	stage->_speed[ZAXIS] = 0.0;

	stage->_safe_region.coerce 	= 0.0;
	stage->_safe_region.shape  	= RECTANGLE2;
	stage->_safe_region.ox 		= 0.0;
	stage->_safe_region.oy 		= 0.0;
	stage->_safe_region.gx 		= 0.0;
	stage->_safe_region.gy 		= 0.0;
	stage->_safe_region.cx 		= 0.0;
	stage->_safe_region.cy 		= 0.0;
	stage->_safe_region.radius 	= 0.0;
	
	GCI_SignalSystem_Create(&(stage->signal_table), 10);
	
	GCI_Signal_New(&(stage->signal_table), "Close", STAGE_PTR_MARSHALLER);
	GCI_Signal_New(&(stage->signal_table), "XYInitExtentsStart", STAGE_PTR_MARSHALLER); 
	GCI_Signal_New(&(stage->signal_table), "XYInitExtentsEnd", STAGE_PTR_MARSHALLER); 
	GCI_Signal_New(&(stage->signal_table), "XYZ_Changed", STAGE_PTR_MARSHALLER); 
	
	stage_set_error_handler(stage, error_handler);
	
	if (stage_params_init(stage) ==STAGE_ERROR)
		return STAGE_ERROR;
	
	if(stage->_cops->init == NULL) {
    
    	send_stage_error_text(stage, "Init operation not implemented for stage");
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->init)(stage) == STAGE_ERROR ) {
	
		send_stage_error_text(stage, "Init operation failed for stage");
		return STAGE_ERROR;
  	}

	stage->_main_ui_panel = FindAndLoadUIR(0, "stage_ui.uir", STAGE_PNL);  
	stage_dim_controls(stage, 1);

	SetPanelAttribute (stage->_main_ui_panel, ATTR_TITLE, stage->_description);
	SetPanelAttribute (stage->_main_ui_panel, ATTR_DIMMED, 1);
	
#ifdef ASYNCH_TIMER
	stage->_timer = NewAsyncTimer (1.0, -1, 1, OnStageTimerTick, stage);
	SetAsyncTimerAttribute (stage->_timer, ASYNC_ATTR_ENABLED,  0);
#else  	
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_TIMER, OnStageTimerTick, stage) < 0)
		return STAGE_ERROR;
#endif
  	
 	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_ABORT_MOVE, OnStageAbortMove, stage) < 0)
		return STAGE_ERROR;
  	
  	if ( InstallCtrlCallback (stage->_main_ui_panel, INIT_PNL_ABORT_INIT, OnStageAbortInit, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_QUIT, OnStageQuit, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_ADVANCED, OnStageAdvancedButton, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_JOYSTICK_XY_SPEED, OnStageJoystickSpeed, stage) < 0)
		return STAGE_ERROR;	
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_JOYSTICK_ENABLE, OnStageJoystickToggled, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_ABOUT, OnAbout, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_SHOW_SETTINGS, OnSettings, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_INITIALISE, OnReinit, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_MOVE_REL_X, MoveByX, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_MOVE_REL_Y, MoveByY, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_MOVE_REL_Z, MoveByZ, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_MOVE_REL_XYZ, MoveByXYZ, stage) < 0)
		return STAGE_ERROR;	
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_MOVE_ABS_X, MoveToX, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_MOVE_ABS_Y, MoveToY, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_MOVE_ABS_Z, MoveToZ, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_MOVE_ABS_XYZ, MoveToXYZ, stage) < 0)
		return STAGE_ERROR;	
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_SET_DATUM_XYZ, OnSetXYZDatum, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_SET_DATUM_XY, OnSetXYDatum, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_SET_DATUM_Z, OnSetZDatum, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_GOTO_XYZ_DATUM, OnGotoXYZDatum, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_GOTO_XY_DATUM, OnGotoXYDatum, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_GOTO_Z_DATUM, OnGotoZDatum, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_TOP_LEFT, MoveTopLeft, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_TOP_RIGHT, MoveTopRight, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_BOTTOM_LEFT, MoveBottomLeft, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_BOTTOM_RIGHT, MoveBottomRight, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_Z_TOP, MoveZTop, stage) < 0)
		return STAGE_ERROR;
		
	if ( InstallCtrlCallback (stage->_main_ui_panel, STAGE_PNL_Z_BOTTOM, MoveZBottom, stage) < 0)
		return STAGE_ERROR;
		
	// We dont want to show the full controls ie the advanced panel
	SetPanelAttribute(stage->_main_ui_panel, ATTR_HEIGHT, 108);  
		
	//if (stage_params_init(stage) ==STAGE_ERROR)
	//	return STAGE_ERROR;

	//dim_xyz_controls(stage);

	if (stage->focus_drive == NULL || stage->focus_drive->_mounted == 0) {
		MessagePopup("Warning", "Z drive not found.");
		SetCtrlVal(stage->_params_ui_panel, XYZ_PARAMS_Z_ENABLED, 0);
		stage->_enabled_axis[ZAXIS] = 0;
	}
	
	return STAGE_SUCCESS;
}

void stage_set_move_type(Stage* stage, int type)
{
	stage->_move_type = type;
}
void stage_get_move_type(Stage* stage, int *type)
{
	*type = stage->_move_type;
}

int stage_reset(Stage* stage)
{
  	if(stage->_cops->reset == NULL) {
    	send_stage_error_text(stage, "reset not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->reset)(stage) == STAGE_ERROR ) {
		send_stage_error_text(stage, "reset failed");
		return STAGE_ERROR;
	}

	return STAGE_SUCCESS;
}


int stage_enable_axis (Stage* stage, Axis axis)
{
  	if(stage->_cops->enable_axis == NULL) {
    	send_stage_error_text(stage, "enable_axis not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	// Already Enabled
	if(stage->_enabled_axis[axis] == 1)
		return STAGE_SUCCESS;
		
	if( (*stage->_cops->enable_axis)(stage, axis) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to enable axis");
		return STAGE_ERROR;
	}
	
	stage->_enabled_axis[axis] = 1;
	
	return STAGE_SUCCESS;
}


int	stage_disable_axis (Stage* stage, Axis axis)
{
  	if(stage->_cops->disable_axis == NULL) {
    	send_stage_error_text(stage, "disable_axis not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	// Already Disabled
	if(stage->_enabled_axis[axis] == 0)
		return STAGE_SUCCESS;	

	if( (*stage->_cops->disable_axis)(stage, axis) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to disable axis");
		return STAGE_ERROR;
	}
	
	stage->_enabled_axis[axis] = 0;
	
	return STAGE_SUCCESS;
}


int	stage_is_axis_enabled (Stage* stage, Axis axis)
{
	return stage->_enabled_axis[axis];
}


int stage_set_timeout (Stage* stage, double timeout) 
{
  	if(stage->_cops->set_timeout == NULL) {
    	send_stage_error_text(stage, "set_timeout not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->set_timeout)(stage, timeout) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to set_timeout");
		return STAGE_ERROR;
	}
	
	return STAGE_SUCCESS;
}


static int stage_setup_graph(Stage* stage)
{
	if(stage->_main_ui_panel == 0)
		return STAGE_ERROR;
	
	// Set up graph axes, correcting for direction of travel
	DeleteGraphPlot (stage->_main_ui_panel, STAGE_PNL_XY_DISP, -1, VAL_IMMEDIATE_DRAW);
	DeleteGraphPlot (stage->_main_ui_panel, STAGE_PNL_Z_DISP, -1, VAL_IMMEDIATE_DRAW); 
	
	SetAxisScalingMode (stage->_main_ui_panel, STAGE_PNL_XY_DISP, VAL_BOTTOM_XAXIS, VAL_MANUAL, stage->_limits.min_x, stage->_limits.max_x);
	
	if (stage->_limits.max_x < stage->_limits.min_x )
		SetCtrlAttribute (stage->_main_ui_panel, STAGE_PNL_XY_DISP, ATTR_XREVERSE, 1);
	else
		SetCtrlAttribute (stage->_main_ui_panel, STAGE_PNL_XY_DISP, ATTR_XREVERSE, 0);  
		
	SetAxisScalingMode (stage->_main_ui_panel, STAGE_PNL_XY_DISP, VAL_LEFT_YAXIS, VAL_MANUAL, stage->_limits.min_y, stage->_limits.max_y);
	
	if (stage->_limits.max_y < stage->_limits.min_y )
		SetCtrlAttribute (stage->_main_ui_panel, STAGE_PNL_XY_DISP, ATTR_YREVERSE, 0);
	else
		SetCtrlAttribute (stage->_main_ui_panel, STAGE_PNL_XY_DISP, ATTR_YREVERSE, 1);  	
		
	
	SetAxisScalingMode (stage->_main_ui_panel, STAGE_PNL_Z_DISP, VAL_LEFT_YAXIS, VAL_MANUAL, stage->_limits.min_z, stage->_limits.max_z);

		
	return STAGE_SUCCESS;
}


int stage_set_current_position(Stage* stage, double x, double y, double z)
{
	if ((FP_Compare(x, stage->_current_pos.x) == 0) &&
		(FP_Compare(y, stage->_current_pos.y) == 0) &&
		(FP_Compare(z, stage->_current_pos.z) == 0)) return STAGE_SUCCESS;	//No change
		 
    
    SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_X_POS, x); 
    SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_Y_POS, y);
    SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_Z_POS, z);

	DeleteGraphPlot (stage->_main_ui_panel, STAGE_PNL_XY_DISP, -1, VAL_IMMEDIATE_DRAW);
	DeleteGraphPlot (stage->_main_ui_panel, STAGE_PNL_Z_DISP, -1, VAL_IMMEDIATE_DRAW); 

	PlotPoint (stage->_main_ui_panel, STAGE_PNL_XY_DISP, 0.0, 0.0, VAL_SOLID_SQUARE, VAL_BLUE);
	PlotPoint (stage->_main_ui_panel, STAGE_PNL_XY_DISP, x, y, VAL_SOLID_SQUARE, VAL_RED);
	PlotPoint (stage->_main_ui_panel, STAGE_PNL_Z_DISP, 0.5, stage->_datum.z, VAL_SOLID_SQUARE, VAL_BLUE);
	PlotPoint (stage->_main_ui_panel, STAGE_PNL_Z_DISP, 0.5, z+stage->_datum.z, VAL_SOLID_SQUARE, VAL_RED);
	
	stage->_current_pos.x = x;
	stage->_current_pos.y = y;
	stage->_current_pos.z = z;
	
	return STAGE_SUCCESS;
}

int stage_plot_current_position(Stage* stage)
{
	double x, y, z;
	
	stage_get_xyz_position (stage, &x, &y, &z);
	
	if ((FP_Compare(x, stage->_current_pos.x) == 0) &&
		(FP_Compare(y, stage->_current_pos.y) == 0) &&
		(FP_Compare(z, stage->_current_pos.z) == 0)) return STAGE_SUCCESS;	//No change
		 
    
    SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_X_POS, x); 
    SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_Y_POS, y);
    SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_Z_POS, z);

	DeleteGraphPlot (stage->_main_ui_panel, STAGE_PNL_XY_DISP, -1, VAL_IMMEDIATE_DRAW);
	DeleteGraphPlot (stage->_main_ui_panel, STAGE_PNL_Z_DISP, -1, VAL_IMMEDIATE_DRAW); 

	PlotPoint (stage->_main_ui_panel, STAGE_PNL_XY_DISP, 0.0, 0.0, VAL_SOLID_SQUARE, VAL_BLUE);
	PlotPoint (stage->_main_ui_panel, STAGE_PNL_XY_DISP, x, y, VAL_SOLID_SQUARE, VAL_RED);
	PlotPoint (stage->_main_ui_panel, STAGE_PNL_Z_DISP, 0.5, stage->_datum.z, VAL_SOLID_SQUARE, VAL_BLUE);
	PlotPoint (stage->_main_ui_panel, STAGE_PNL_Z_DISP, 0.5, z+stage->_datum.z, VAL_SOLID_SQUARE, VAL_RED);
	
	stage->_current_pos.x = x;
	stage->_current_pos.y = y;
	stage->_current_pos.z = z;
	
	GCI_Signal_Emit(&(stage->signal_table), "XYZ_Changed", GCI_VOID_POINTER, stage);

	return STAGE_SUCCESS;
}


int stage_self_test(Stage* stage)
{
  	if(stage->_cops->self_test == NULL) {
    	send_stage_error_text(stage, "Self test operation not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;;
  	}

	if( (*stage->_cops->self_test)(stage) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Self test operation failed for device %s\n", stage->_description);
		return STAGE_ERROR;
	}

  	return STAGE_SUCCESS;

}


int stage_check_user_has_aborted(Stage *stage)
{
	ProcessSystemEvents (); // Force CVI to look for events, in this case an abort init
			
	if (stage->_abort_move)
		return 1;
		
	return 0;
}


int stage_wait_for_stop_moving(Stage *stage, double timeout)
{
	double startTime;
	int is_moving;
	
	stage->_abort_move = 0;
	startTime = Timer();
	
	while(1) {
		
		stage_is_moving (stage, &is_moving);
		if(!is_moving)
			break;
		
		if ((Timer() - startTime) > timeout) return STAGE_ERROR;	
		
		//Only check for button presses during long moves
		if (timeout > 3.0) {
			ProcessSystemEvents (); // Force CVI to look for events, in this case an abort init 
		
			if (stage->_abort_move)
				break;
		}
	}
	
	return STAGE_SUCCESS; 
}

//////////////////////////////////////////////////////////////////////////
//Functions to calculate how long the move should take

static void CalcMoveTime(double dist, double speed, double accel, double *t)
{
	double s, d;
	
	if (dist == 0) {
		*t = 0.0;
		return;
	}

	if ((accel == 0) || (speed == 0)) {
		*t = 10.0;	//Can't calculate it. Assume max move time of 10 seconds
		return;
	}

	d = fabs(dist);
	s = speed*speed/(2*accel);		//distance to reach full speed from rest
	if (s > (d/2)) 
		*t = 2*sqrt(2*(d/2)/accel);	//accelerate then decellerate at same rate
	else {
		*t = 2*(speed/accel);		//Time to reach full speed and decellerate
		*t += (d - (2*s))/speed;	//Time to move remaining distance
	}
}
static void CalcXYZmoveTime(Stage* stage, double x, double y, double z, double *t)
{
	double tx, ty, tz, tmax;
	
	CalcMoveTime(x, stage->_speed[XAXIS]*1000.0, stage->_acceleration[XAXIS]*1000.0, &tx);  //mm to um conversion
	CalcMoveTime(y, stage->_speed[YAXIS]*1000.0, stage->_acceleration[YAXIS]*1000.0, &ty);
	CalcMoveTime(z, stage->_speed[ZAXIS]*1000.0/F_FACTOR, stage->_acceleration[ZAXIS]*1000.0/F_FACTOR, &tz);
	tmax = max(tx, ty);
	tmax = max(tmax, tz);
	*t = tmax;
}

//////////////////////////////////////////////////////////////////////////
static int ReadXYZfromRegistry (double *x, double *y, double *z)
{
	char buffer[20];
	int error, nchars;
	
	// Read stage position
	error = RegReadString (REGKEY_HKCU, "software\\GCI\\microscopy\\Stage", "x", buffer, 20, &nchars);
	if (!error)
		sscanf(buffer, "%lf", x);
	error = RegReadString (REGKEY_HKCU, "software\\GCI\\microscopy\\Stage", "y", buffer, 20, &nchars);
	if (!error)
		sscanf(buffer, "%lf", y);
	error = RegReadString (REGKEY_HKCU, "software\\GCI\\microscopy\\Stage", "z", buffer, 20, &nchars);
	if (!error)
		sscanf(buffer, "%lf", z);
	return error;
}

// This Initialises the extents and finds the original origin
int stage_find_initialise_extents (Stage* stage, int full)
{
	int xDir, yDir;	
	double centre_x, centre_y;
	double x, y, z, lastx, lasty, lastz;
	
	SetWaitCursor(1);
	stage_dim_controls(stage, 1);

	stage_stop_timer(stage);

	if(stage_reset(stage) == STAGE_ERROR)
		goto Error;
	
	if(stage_self_test(stage) == STAGE_ERROR)
		goto Error;
	
	stage_load_default_parameters(stage);
	stage_send_all_params(stage);
	
	stage->_abort_move = 0;
	stage_set_joystick_off (stage);
	stage_enable_axis (stage, XAXIS);
	stage_enable_axis (stage, YAXIS);
#ifndef XY_ONLY
	stage_disable_axis (stage, ZAXIS);
#endif

	// Emit Signal to any interested party.
	if (full)
		GCI_Signal_Emit(&(stage->signal_table), "XYInitExtentsStart", GCI_VOID_POINTER, stage);
		
	// Initialise the stage controller - also determines the limits if full is true.
	if(stage_calibrate_extents (stage, full, &(stage->_limits.min_x), &(stage->_limits.min_y),
								   &(stage->_limits.max_x), &(stage->_limits.max_y)) == STAGE_ERROR) {
		goto Error;
	}
		
  	if(stage_set_speed (stage, ALL_AXIS, stage->_default_speed[XAXIS]) == STAGE_ERROR)
		goto Error;
  	
  	if(stage_set_acceleration (stage, ALL_AXIS, stage->_acceleration[XAXIS]) == STAGE_ERROR)
		goto Error;
		
#ifdef XY_ONLY
	stage->_speed[ZAXIS] = stage->focus_drive->_speed;
	stage->_acceleration[ZAXIS] = 700;		//guess for use with CalcMoveTime();
#endif

	if (full) {
		if (focus_drive_set_datum(stage->focus_drive, 0.0) == FOCUS_DRIVE_ERROR) {
			send_stage_error_text(stage, "Failed to set z datum");
			return STAGE_ERROR;
		}
		stage->_datum.z = 0.0;
		
		//Initialisation is done before any direction reversal is implemented
		//otherwise it all goes horribly wrong
		xDir = stage->_dir[XAXIS];
		yDir = stage->_dir[YAXIS];
		stage->_dir[XAXIS] = 1;
		stage->_dir[YAXIS] = 1;

		 //Set arbitrary limits for disabled axes - needed for graph plotting 
		if (!stage->_enabled_axis[XAXIS]) {stage->_limits.min_x = -25000; stage->_limits.max_x = 25000;} 
		if (!stage->_enabled_axis[YAXIS]) {stage->_limits.min_y = -25000; stage->_limits.max_y = 25000;} 

		assert(stage->_limits.max_x > stage->_limits.min_x);
		assert(stage->_limits.max_y > stage->_limits.min_y); 
		
		// Set software limits 2mm inside the hardware limits 
		stage->_limits.min_x += stage->_software_limit;
		stage->_limits.min_y += stage->_software_limit;
		stage->_limits.max_x -= stage->_software_limit;
		stage->_limits.max_y -= stage->_software_limit;

#ifdef XY_ONLY
		stage->_limits.min_z = stage->focus_drive->_min;
		stage->_limits.max_z = stage->focus_drive->_max;
#else
		stage->_limits.min_z = Z_STAGE_MIN;
		stage->_limits.max_z = Z_STAGE_MAX;
#endif

		centre_x = stage->_limits.min_x + (stage->_limits.max_x - stage->_limits.min_x) / 2;
		centre_y = stage->_limits.min_y + (stage->_limits.max_y - stage->_limits.min_y) / 2;

		// Move to the centre
		stage_goto_xy_position (stage, centre_x, centre_y); 

		// Set a shorter timeout to prevent user inflicted
		// violence to PC in event of a comms failure.
		if(stage_set_timeout (stage, 5.0) == STAGE_ERROR)
			goto Error;

		stage_set_xy_datum (stage);
		
		stage_save_limits(stage);
		
		stage->_dir[XAXIS] = xDir;
		stage->_dir[YAXIS] = yDir;

		// Emit Signal to any interested party.
		GCI_Signal_Emit(&(stage->signal_table), "XYInitExtentsEnd", GCI_VOID_POINTER, stage);
	}
	else {
		//Assume stage has not moved since last run
		stage_load_limits(stage);
		
		ReadXYZfromRegistry (&lastx, &lasty, &lastz);
	}
	
	stage_send_all_params(stage);

	//SetCtrlVal(stage->_params_ui_panel, XYZ_PARAMS_Z_LOW_LIMIT, stage->_limits.min_z);
	stage_setup_graph(stage);

	stage_set_joystick_on (stage);

	//Set up initial roi to match the physical limits
	stage->_roi.min_x = stage->_limits.min_x;
	stage->_roi.max_x = stage->_limits.max_x;
	stage->_roi.min_y = stage->_limits.min_y;
	stage->_roi.max_y = stage->_limits.max_y;
	stage->_roi.min_z = stage->_limits.min_z;
	stage->_roi.max_z = stage->_limits.max_z;

	//Avoid accidental Z drive calamities
	stage_get_xyz_position(stage, &x, &y, &z);
	SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_Z_ABS, z);
		
	stage_start_timer(stage);
	stage_dim_controls(stage, 0);
	SetWaitCursor(0);
	return STAGE_SUCCESS;
	
Error:
	stage_dim_controls(stage, 0);
	SetWaitCursor(0);
	return STAGE_ERROR; 
}


int stage_set_min_z(Stage* stage, double z)
{
	double curz;
	
	//Physical Z range for the TE2000 is 0 to 10000um, for 90i it's -15000 to 2000um, 
	//however, there may be reasons why it cannot go to the bottom.
	stage->_limits.min_z = z;
	stage->focus_drive->_min = z;
	
	stage_save_limits(stage);
	stage_setup_graph(stage);
	
	if (stage_get_z_position(stage, &curz) == STAGE_ERROR)
		return STAGE_ERROR;
	if (curz + stage->_datum.z < z) {
		return stage_goto_z_position (stage, z);
	}
	
	return STAGE_SUCCESS;
}

int stage_set_max_z(Stage* stage, double z)
{
	double curz;
	
	//Physical Z range for the TE2000 is 0 to 10000um, for 90i it's -15000 to 2000um, 
	//however, there may be reasons why it cannot go to the top.
	stage->_limits.max_z = z;
	stage->focus_drive->_max = z;
	
	stage_save_limits(stage);
	stage_setup_graph(stage);
	
	if (stage_get_z_position(stage, &curz) == STAGE_ERROR)
		return STAGE_ERROR;
	if (curz + stage->_datum.z > z) {
		return stage_goto_z_position (stage, z);
	}
	
	return STAGE_SUCCESS;
}

int stage_set_port (Stage* stage, int port)
{
	stage->_port = port;

	return STAGE_SUCCESS;
}


int stage_get_port_string (Stage* stage, char *port_str)
{
	if(port_str == NULL)
		return STAGE_ERROR;

	sprintf(port_str, "COM%d", stage->_port);

	return STAGE_SUCCESS;
}


int stage_set_baud_rate (Stage* stage, int baud_rate)
{
	if(stage->_cops->set_baud_rate == NULL) {
    	send_stage_error_text(stage, "set_baud_rate not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->set_baud_rate)(stage, baud_rate) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to set baud rate");
		return STAGE_ERROR;
	}
	
	stage->_baud_rate = baud_rate;
	
	return STAGE_SUCCESS;
}


int stage_get_baud_rate (Stage* stage, int *baud_rate)
{
	*baud_rate = stage->_baud_rate;
	
	return STAGE_SUCCESS;
}


void stage_set_operations(Stage* stage, struct stage_operations* cops)
{
	stage->_cops = cops;
}


void stage_set_error_handler( Stage* stage, void (*handler) (char *error_string, Stage *stage) )
{
	stage->_error_handler = handler;
}


void stage_stop_timer(Stage* stage)
{
#ifdef ASYNCH_TIMER
	SetAsyncTimerAttribute (stage->_timer, ASYNC_ATTR_ENABLED,  0);
#else	
	SetCtrlAttribute (stage->_main_ui_panel, STAGE_PNL_TIMER, ATTR_ENABLED, 0);
#endif	
}

void stage_start_timer(Stage* stage)
{
#ifdef ASYNCH_TIMER
	SetAsyncTimerAttribute (stage->_timer, ASYNC_ATTR_ENABLED,  1);
#else	
	SetCtrlAttribute (stage->_main_ui_panel, STAGE_PNL_TIMER, ATTR_ENABLED, 1);
#endif	
}


int stage_destroy(Stage* stage)
{
	stage_stop_timer(stage);
#ifdef ASYNCH_TIMER
	DiscardAsyncTimer(stage->_timer);
#endif	

#ifdef XY_ONLY
	focus_drive_destroy(stage->focus_drive);
  	if(stage->_cops->destroy == NULL) {
    
    	send_stage_error_text(stage, "Destroy operation not implemented for device %s\n", stage->_description);

    	return STAGE_ERROR;
  	}
#endif

	if( (*stage->_cops->destroy)(stage) == STAGE_ERROR ) {
	
		send_stage_error_text(stage, "Destroy operation failed for device %s\n", stage->_description);
		return STAGE_ERROR;
	}

	stage_destroy_params_ui(stage);

	stage_read_or_write_main_panel_registry_settings(stage, 1);
	stage_read_or_write_params_panel_registry_settings(stage, 1);
	
	DiscardPanel(stage->_main_ui_panel);

	stage->_main_ui_panel = -1;

	if(stage->_data_dir != NULL) {
	
		free(stage->_data_dir);
		stage->_data_dir = NULL;
	}

	if(stage->_description != NULL) {
	
  		free(stage->_description);
  		stage->_description = NULL;
  	}
  	
  	if(stage->_name != NULL) {
  	
  		free(stage->_name);
  		stage->_name = NULL;
  	}
  	
  	CmtDiscardLock (stage->_lock);
  	
  	free(stage);
  	
  	return STAGE_SUCCESS;
}

int stage_set_port_timeout(Stage* stage, double timeout)
{
  	if(stage->_cops->set_port_timeout == NULL) {
    	send_stage_error_text(stage, "set_port_timeout not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->set_port_timeout)(stage, timeout) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to set_port_timeout");
		return STAGE_ERROR;
	}
	
	return STAGE_SUCCESS;
}



int stage_set_description(Stage* stage, const char *description)
{
  	stage->_description = (char *)malloc(strlen(description) + 1);

  	if(stage->_description != NULL) {
    	strcpy(stage->_description, description);
  	}
  	
  	return STAGE_SUCCESS;
}


int stage_get_description(Stage* stage, char *description)
{
  	if(stage->_description != NULL) {
    
    	strcpy(description, stage->_description);
    
    	return STAGE_SUCCESS;
  	}
  
  	return STAGE_ERROR;
}


int stage_set_data_dir(Stage* stage, const char *dir)
{
  	stage->_data_dir = (char *)malloc(strlen(dir) + 1);

	memset(stage->_data_dir, 0, strlen(dir));

  	if(stage->_data_dir != NULL) {
    	strcpy(stage->_data_dir, dir);
  	}
  	
  	return STAGE_SUCCESS;
}


int stage_get_data_dir(Stage* stage, char* dir)
{
  	if(stage->_data_dir != NULL) {
    	strcpy(dir, stage->_data_dir);
    	return STAGE_SUCCESS;
  	}
  
  	return STAGE_ERROR;
}


int stage_set_name(Stage* stage, char *name)
{
  	stage->_name = (char *)malloc(strlen(name) + 1);

  	if(stage->_name != NULL)
    	strcpy(stage->_name, name);
  	
  	return STAGE_SUCCESS;
}


int stage_get_name(Stage* stage, char *name)
{
  	if(stage->_name != NULL) {
    	strcpy(name, stage->_name);
    	return STAGE_SUCCESS;
  	}
  
  	return STAGE_ERROR;
}


int stage_power_up(Stage* stage)
{
	if(stage->_powered_up > 0)
		return STAGE_SUCCESS;

	if(stage->_cops->power_up == NULL) {
    	send_stage_error_text(stage, "Power up operation not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->power_up)(stage) == STAGE_ERROR ) {
		
		send_stage_error_text(stage, "Power up operation failed for device %s\n", stage->_description);
		return STAGE_ERROR;
	}
	
	stage->_powered_up = 1;
	
	stage_set_joystick_off (stage);

  	return STAGE_SUCCESS; 
}


int stage_is_powered_up(Stage* stage)
{
	return stage->_powered_up;
}


int stage_power_down(Stage* stage)
{
	if(stage->_powered_up == 0)
		return STAGE_SUCCESS;

  	if(stage->_cops->power_down == NULL) {
    	send_stage_error_text(stage, "Power down operation not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;;
  	}

	if( (*stage->_cops->power_down)(stage) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Power down operation failed for device %s\n", stage->_description);
		return STAGE_ERROR;
	}

	stage->_powered_up = 0;

  	return STAGE_SUCCESS;
}

int stage_set_pitch(Stage* stage, Axis axis, double pitch)
{
  	if(stage->_cops->set_pitch == NULL) {
    	send_stage_error_text(stage, "Set pitch not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->set_pitch)(stage, axis, pitch) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to set pitch");
		return STAGE_ERROR;
	}
	
	stage->_pitch[axis] = pitch;
	if (axis == ALL_AXIS) {
		stage->_pitch[XAXIS] = pitch;
		stage->_pitch[YAXIS] = pitch;
		stage->_pitch[ZAXIS] = pitch;
	}
	
	return STAGE_SUCCESS;
}

void stage_get_pitch(Stage* stage, Axis axis, double *pitch)
{
	*pitch = stage->_pitch[axis];
}

int stage_set_speed(Stage* stage, Axis axis, double speed)
{
  	if(stage->_cops->set_speed == NULL) {
    	send_stage_error_text(stage, "Set speed not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->set_speed)(stage, axis, speed) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to set speed");
		return STAGE_ERROR;
	}
	
	stage->_speed[axis] = speed;
	if (axis == ALL_AXIS) {
		stage->_speed[XAXIS] = speed;
		stage->_speed[YAXIS] = speed;
		stage->_speed[ZAXIS] = speed;
	}
	
	return STAGE_SUCCESS;
}

int stage_set_default_speed(Stage* stage)
{
  	if(stage->_cops->set_speed == NULL) {
    	send_stage_error_text(stage, "Set speed not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->set_speed)(stage, XAXIS, stage->_default_speed[XAXIS]) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to set speed");
		return STAGE_ERROR;
	}
	if( (*stage->_cops->set_speed)(stage, YAXIS, stage->_default_speed[YAXIS]) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to set speed");
		return STAGE_ERROR;
	}
	if( (*stage->_cops->set_speed)(stage, ZAXIS, stage->_default_speed[ZAXIS]) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to set speed");
		return STAGE_ERROR;
	}
	
	stage->_speed[ALL_AXIS] = stage->_default_speed[ALL_AXIS];
	stage->_speed[XAXIS] = stage->_default_speed[XAXIS];
	stage->_speed[YAXIS] = stage->_default_speed[YAXIS];
	stage->_speed[ZAXIS] = stage->_default_speed[ZAXIS];
	
	return STAGE_SUCCESS;
}


int  stage_get_speed (Stage* stage, Axis axis, double *speed)
{
  	if(stage->_cops->get_speed == NULL) {
    	send_stage_error_text(stage, "Get speed not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->get_speed)(stage, axis, speed) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to get speed");
		return STAGE_ERROR;
	}
	
	return STAGE_SUCCESS;
}


int  stage_set_joystick_on (Stage* stage)
{
	if(stage->_cops->set_joystick_on == NULL) {
    	send_stage_error_text(stage, "set_joystick_on not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if(stage->_joystick_status == 1)
		return STAGE_SUCCESS;

	if( (*stage->_cops->set_joystick_on)(stage) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to set joystick on");
		return STAGE_ERROR;
	}
	
	//Set the joystick speed
	if( (*stage->_cops->set_joystick_speed)(stage, stage->_joystick_speed) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to set joystick speed");
		return STAGE_ERROR;
	}
	
	stage->_joystick_status = 1;
	
	SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_JOYSTICK_ENABLE, stage->_joystick_status);
	SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_JOY_ON, stage->_joystick_status);

	GCI_Signal_Emit(&(stage->signal_table), "XYZ_Changed", GCI_VOID_POINTER, stage);

	return STAGE_SUCCESS;
}


int stage_set_joystick_off (Stage* stage)
{
	if(stage->_cops->set_joystick_off == NULL) {
    	send_stage_error_text(stage, "set_joystick_off not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}
  	
  	if(stage->_joystick_status == 0)
		return STAGE_SUCCESS;

	if( (*stage->_cops->set_joystick_off)(stage) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to set joystick off");
		return STAGE_ERROR;
	}
	
	//Set normal speed
	if (stage_set_speed(stage, ALL_AXIS, stage->_speed[XAXIS]) == STAGE_ERROR)
		send_stage_error_text(stage, "Failed to set normal speed");
	
	stage->_joystick_status = 0;

	SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_JOYSTICK_ENABLE, stage->_joystick_status);
	SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_JOY_ON, stage->_joystick_status);
	
	GCI_Signal_Emit(&(stage->signal_table), "XYZ_Changed", GCI_VOID_POINTER, stage);

	return STAGE_SUCCESS;
}


int  stage_get_joystick_status (Stage* stage)
{
	return stage->_joystick_status; 
}


int stage_get_joystick_speed(Stage* stage, double *speed)
{
	*speed = stage->_joystick_speed;

	return STAGE_SUCCESS;
}


int stage_set_joystick_speed(Stage* stage, double speed)
{
	int status = stage_get_joystick_status(stage);

	if (!status) return STAGE_SUCCESS;  //It's not enabled
	
  	if(stage->_cops->set_joystick_speed == NULL) {
    	send_stage_error_text(stage, "Set joystick speed not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->set_joystick_speed)(stage, speed) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to set joystick speed");
		return STAGE_ERROR;
	}
	
	stage->_joystick_speed = speed;

	return STAGE_SUCCESS;
}

int  stage_abort_move (Stage* stage)
{
	if(stage->_cops->abort_move == NULL) {
    	send_stage_error_text(stage, "abort_move not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}
  	
	if( (*stage->_cops->abort_move)(stage) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to abort the move");
		return STAGE_ERROR;
	}
	
#ifdef XY_ONLY
	focus_drive_abort(stage->focus_drive);
#endif
	
	return STAGE_SUCCESS;
}

int  stage_send_command (Stage* stage, char* command)
{
	if(stage->_cops->send_command == NULL) {
    	send_stage_error_text(stage, "send_command not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}
  	
	if( (*stage->_cops->send_command)(stage, command) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to send the command");
		return STAGE_ERROR;
	}
	
	return STAGE_SUCCESS;
}

int  stage_get_response (Stage* stage, char* response)
{
	if(stage->_cops->get_response == NULL) {
    	send_stage_error_text(stage, "get_response not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}
  	
	if( (*stage->_cops->get_response)(stage, response) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to get the response");
		return STAGE_ERROR;
	}
	
	return STAGE_SUCCESS;
}

int stage_calibrate_extents (Stage* stage, int full, double *min_x, double *min_y, double *max_x, double *max_y)
{
	if(stage->_cops->calibrate_extents == NULL) {
    	send_stage_error_text(stage, "calibrate_extents not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->calibrate_extents)(stage, full, min_x, min_y, max_x, max_y) == STAGE_ERROR ) {
		send_stage_error_text(stage, "calibrate_extents failed");
		return STAGE_ERROR;
	}
	
	return STAGE_SUCCESS;
}


int stage_set_acceleration(Stage* stage, Axis axis, double acceleration)
{
  	if(stage->_cops->set_acceleration == NULL) {
    	send_stage_error_text(stage, "Set acceleration not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->set_acceleration)(stage, axis, acceleration) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to set acceleration");
		return STAGE_ERROR;
	}
	
	stage->_acceleration[axis] = acceleration;
	if (axis == ALL_AXIS) {
		stage->_acceleration[XAXIS] = acceleration;
		stage->_acceleration[YAXIS] = acceleration;
		stage->_acceleration[ZAXIS] = acceleration;
	}
	
	return STAGE_SUCCESS;
}


int stage_get_acceleration(Stage* stage, Axis axis, double *acceleration)
{
  	if(stage->_cops->get_acceleration == NULL) {
    	send_stage_error_text(stage, "Get acceleration not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->get_acceleration)(stage, axis, acceleration) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to get acceleration");
		return STAGE_ERROR;
	}

	return STAGE_SUCCESS;
}

void stage_set_z_tolerance (Stage* stage, int tolerance)
{

#ifndef XY_ONLY
	return;
#endif

	//If there's no focus drive or this isn't implemented, nothing will happen
	if (focus_drive_set_tolerance(stage->focus_drive, tolerance) == FOCUS_DRIVE_ERROR) 
		send_stage_error_text(stage, "Failed to set z tolerance");
}

void stage_get_z_tolerance (Stage* stage, int *tolerance)
{

#ifndef XY_ONLY
	*tolerance = 9;
	return;
#endif

	*tolerance = stage->focus_drive->_tolerance;
}

void stage_set_z_speed (Stage* stage, int speed)
{

#ifndef XY_ONLY
	return;
#endif

	//If there's no focus drive or this isn't implemented, nothing will happen
	if (focus_drive_set_speed(stage->focus_drive, speed) == FOCUS_DRIVE_ERROR) 
		send_stage_error_text(stage, "Failed to set z speed");
}

void stage_get_z_speed (Stage* stage, int *speed)
{

#ifndef XY_ONLY
	*speed = 1;
	return;
#endif

	*speed = stage->focus_drive->_speed;
}

int stage_get_x_position (Stage* stage, double *x)
{
	double tmp_y;
	
	if(stage_get_xy_position(stage, x, &tmp_y) == STAGE_ERROR)
		return STAGE_ERROR;
		
	return STAGE_SUCCESS;
}


int stage_get_y_position (Stage* stage, double *y)
{
	double tmp_x;
	
	if(stage_get_xy_position(stage, &tmp_x, y) == STAGE_ERROR)
		return STAGE_ERROR;
		
	return STAGE_SUCCESS;
}


int stage_get_z_position (Stage* stage, double *z)
{
#ifndef XY_ONLY
	double tmp_x, tmp_y;
	
	if(stage_get_xyz_position(stage, &tmp_x, &tmp_y, z) == STAGE_ERROR)
		return STAGE_ERROR;
		
	return STAGE_SUCCESS;
#endif

	//*z = stage->focus_drive->_position;
	if (focus_drive_get_pos(stage->focus_drive, z) == FOCUS_DRIVE_ERROR) {
		send_stage_error_text(stage, "Failed to get z position");
		return STAGE_ERROR;
	}
	
	return STAGE_SUCCESS;
}


int stage_goto_x_position (Stage* stage, double x)
{
	double xCur, yCur, zCur;

	if (stage->_move_type == FAST) { //assume no problems
		return stage_async_goto_x_position (stage, x);
	}
	
	if(stage_get_xyz_position(stage, &xCur, &yCur, &zCur) == STAGE_ERROR)
		return STAGE_ERROR;	

	return stage_goto_xyz_position (stage, x, yCur, zCur, xCur, yCur, zCur);
}


int stage_goto_y_position (Stage* stage, double y)
{
	double xCur, yCur, zCur;

	if (stage->_move_type == FAST) { //assume no problems
		return stage_async_goto_y_position (stage, y);
	}
	
	if(stage_get_xyz_position(stage, &xCur, &yCur, &zCur) == STAGE_ERROR)
		return STAGE_ERROR;	

	return stage_goto_xyz_position (stage, xCur, y, zCur, xCur, yCur, zCur);
}


int stage_goto_z_position (Stage* stage, double z)
{
	int retval, tolerance, outsideStageArea = 0;
	
	//First try xyz controller
  	if(stage->_cops->async_goto_z_position != NULL) {
		if( (*stage->_cops->async_goto_z_position)(stage, z) == STAGE_ERROR ) {
			send_stage_error_text(stage, "async_goto_z_position failed");
			return STAGE_ERROR;
		}
		return STAGE_SUCCESS;
	}	

	//If there's no focus drive or this isn't implemented, nothing will happen
	if (stage->_move_type == FAST) { //assume no problems
		stage_get_z_tolerance(stage, &tolerance);
		if (tolerance != 9) stage_set_z_tolerance(stage, 9);
		retval = focus_drive_set_pos(stage->focus_drive, z);
		if (tolerance != 9) stage_set_z_tolerance(stage, tolerance);
		return retval;
	}

    SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_ACTIVE, 1); 

	if (focus_drive_set_pos(stage->focus_drive, z) == FOCUS_DRIVE_ERROR) {
		send_stage_error_text(stage, "Failed to goto z position");
		return STAGE_ERROR;
	}
	
    SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_ACTIVE, 0); 

	return STAGE_SUCCESS;
}


int stage_goto_xy_position (Stage* stage, double x, double y)
{
	double xCur, yCur, zCur;

	if (stage->_move_type == FAST) { //assume no problems
		return stage_async_goto_xy_position (stage, x, y);
	}
	
	if(stage_get_xyz_position(stage, &xCur, &yCur, &zCur) == STAGE_ERROR)
		return STAGE_ERROR;	

	return stage_goto_xyz_position (stage, x, y, zCur, xCur, yCur, zCur);
}

static int stage_check_safe_region(Stage* stage, double x,double y)
{
	int unsafe = 0;
	double xdiff=0.0, ydiff=0.0;
	
	//Check to see if the requested move will go outside the safe region
	if (stage->_safe_region.coerce) {
		if (stage->_safe_region.shape == RECTANGLE2) {
			if (stage->_enabled_axis[XAXIS]) {if ((x < stage->_safe_region.ox) || (x > (stage->_safe_region.ox+stage->_safe_region.gx))) unsafe = 1;}
			if (stage->_enabled_axis[YAXIS]) {if ((y < stage->_safe_region.oy) || (y > (stage->_safe_region.oy+stage->_safe_region.gy))) unsafe = 1;}
		}
		else {  //circular
			if (stage->_enabled_axis[XAXIS]) xdiff = x - stage->_safe_region.cx;
			if (stage->_enabled_axis[YAXIS]) ydiff = y - stage->_safe_region.cy;
			if (sqrt((xdiff*xdiff)+(ydiff*ydiff)) > stage->_safe_region.radius)
				unsafe = 1;
		}
		if (unsafe) 
			MessagePopup ("XY Stage Error","The requested co-ordinates are outside the safe area");
	}
	return unsafe;
}

void stage_set_safe_region(Stage* stage, safeRegion safe_region)
{
	memcpy ((void *)&stage->_safe_region, (void *)&safe_region, sizeof(safeRegion));
}


int stage_goto_xyz_position (Stage* stage, double x, double y, double z, double xCur, double yCur, double zCur)
{
	double xMove, yMove, zMove, t, t1;
	int joystick_on, outsideStageArea = 0, backlash=0, timer_enabled;
	int unsafe;
	
	t1 = Timer();
	stage->_abort_move = 0;
	
    SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_ACTIVE, 1); 
    
#ifdef ASYNCH_TIMER
	GetAsyncTimerAttribute (stage->_timer, ASYNC_ATTR_ENABLED,  &timer_enabled);
#else	
	GetCtrlAttribute (stage->_main_ui_panel, STAGE_PNL_TIMER, ATTR_ENABLED, &timer_enabled);
#endif	
	stage_stop_timer(stage);
	
	//joystick_on = stage_get_joystick_status(stage);
	//stage_set_joystick_off (stage);	//Disables joystick if it's on
	//printf("t1 %f\n", Timer() - t1);
	t1 = Timer();
	
	xCur *= stage->_dir[XAXIS];
	yCur *= stage->_dir[YAXIS];
	zCur *= stage->_dir[ZAXIS];
	//printf("t2 %f\n", Timer() - t1);
	t1 = Timer();

	//Check to see if the requested move will go outside the stage area
	if (!stage->_enabled_axis[XAXIS]) 
		x = xCur;
	else {
		if (x < stage->_limits.min_x || x > stage->_limits.max_x)  outsideStageArea = 1;
	}
	
	if (!stage->_enabled_axis[YAXIS]) 
		y = yCur;
	else {
		if (y < stage->_limits.min_y || y > stage->_limits.max_y)  outsideStageArea = 1;
	}
	
	if (!stage->_enabled_axis[ZAXIS]) 
		z = zCur;
	else {
		if (z+stage->_datum.z < stage->_limits.min_z || z+stage->_datum.z > stage->_limits.max_z)  outsideStageArea = 1;
	}
	
	if(outsideStageArea) { 
		MessagePopup ("XY Stage Error","The requested co-ordinates are outside the stage area");
		goto Error;
	}
	
	//Check to see if the requested move will go outside the safe region
	unsafe = stage_check_safe_region(stage, x, y);
	if (unsafe) goto Error;

	//Check to see if backlash compensation if required
	xMove = x;
	if (FP_Compare (stage->_backlash[XAXIS], 0.0) != 0) {	//backlash X is not zero
		//If moving in negative direction do backlash
		if (FP_Compare (xCur, x) == 1) {
			xMove = x - stage->_backlash[XAXIS]; 
			backlash = 1;
		}
	}
	yMove = y;
	if (FP_Compare (stage->_backlash[YAXIS], 0.0) != 0) {	//backlashY is not zero
		//If moving in negative direction do backlash
		if (FP_Compare (yCur, y) == 1) {
			yMove = y - stage->_backlash[YAXIS]; 
			backlash = 1;
		}
	}
	zMove = z;
	if (FP_Compare (stage->_backlash[ZAXIS], 0.0) != 0) {	//backlashZ is not zero
		//If moving in negative direction do backlash
		if (FP_Compare (zCur, z) == 1) {
			zMove = z - stage->_backlash[ZAXIS]; 
			backlash = 1;
		}
	}

	//printf("t3 %f\n", Timer() - t1);
	t1 = Timer();
	if (backlash) {
		CalcXYZmoveTime(stage, fabs(xMove-xCur), fabs(yMove-yCur), fabs(zMove-zCur), &t);
		if (stage_async_goto_xyz_position(stage, xMove, yMove, zMove) == STAGE_ERROR) goto Error;

		if (stage->_move_type == FAST)  //assume no problems
			Delay(t+0.02);				//unreliable without the extra delay
		else {
			if (stage_wait_for_stop_moving(stage, t+1) == STAGE_ERROR) 
				goto Error;	//Pause while stage is moving 
		}
		xCur = xMove; yCur = yMove; zCur = zMove; 
	} 

	//printf("t4 %f\n", Timer() - t1);
	t1 = Timer();
	if (!stage->_abort_move) {
		CalcXYZmoveTime(stage, fabs(x-xCur), fabs(y-yCur), fabs(z-zCur), &t);
		if (stage_async_goto_xyz_position(stage, x, y, z) == STAGE_ERROR) goto Error;

		if (stage->_move_type == FAST)  //assume no problems
			Delay(t+0.02);
		else {
			if (stage_wait_for_stop_moving(stage, t+1) == STAGE_ERROR) 
				goto Error;	//Pause while stage is moving 
		}
	}
	
	//printf("t5 %f\n", Timer() - t1);
	t1 = Timer();
	//if (joystick_on) stage_set_joystick_on (stage);	//Re-enable joystick if it was on
	if (timer_enabled) stage_start_timer(stage);

    SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_ACTIVE, 0); 
    
	//printf("t6 %f\n", Timer() - t1);
	t1 = Timer();
	return STAGE_SUCCESS;
	
Error:
	//if (joystick_on)
	//	stage_set_joystick_on (stage);	//Re-enable joystick if it was on
	if (timer_enabled) stage_start_timer(stage);

	return STAGE_ERROR;	
}


int stage_async_goto_x_position (Stage* stage, double x)
{
	x *= stage->_dir[XAXIS];

    SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_ACTIVE, 1); 

  	if(stage->_cops->async_goto_x_position == NULL) {
    	send_stage_error_text(stage, "async_goto_x_position not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->async_goto_x_position)(stage, x) == STAGE_ERROR ) {
		send_stage_error_text(stage, "async_goto_x_position failed");
		return STAGE_ERROR;
	}
	
    SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_ACTIVE, 0); 

	return STAGE_SUCCESS;
}


int stage_async_goto_y_position (Stage* stage, double y)
{
	y *= stage->_dir[YAXIS];

    SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_ACTIVE, 1); 

  	if(stage->_cops->async_goto_y_position == NULL) {
    	send_stage_error_text(stage, "async_goto_y_position not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->async_goto_y_position)(stage, y) == STAGE_ERROR ) {
		send_stage_error_text(stage, "async_goto_y_position failed");
		return STAGE_ERROR;
	}
	
    SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_ACTIVE, 0); 

	return STAGE_SUCCESS;
}


int stage_async_goto_z_position (Stage* stage, double z)
{
	z *= stage->_dir[ZAXIS];

    SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_ACTIVE, 1); 

	//First try xyz controller
  	if(stage->_cops->async_goto_z_position != NULL) {
		if( (*stage->_cops->async_goto_z_position)(stage, z) == STAGE_ERROR ) {
			send_stage_error_text(stage, "async_goto_z_position failed");
			return STAGE_ERROR;
		}
		return STAGE_SUCCESS;
	}	

	//Try separate Z controller
	if (focus_drive_set_pos(stage->focus_drive, z) == FOCUS_DRIVE_ERROR) {
		send_stage_error_text(stage, "Failed to goto z position");
		return STAGE_ERROR;
	}
	
    SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_ACTIVE, 0); 

	return STAGE_SUCCESS;
}


int stage_async_goto_xy_position (Stage* stage, double x, double y)
{
	x *= stage->_dir[XAXIS];
	y *= stage->_dir[YAXIS];

  	if(stage->_cops->async_goto_xy_position == NULL) {
    	send_stage_error_text(stage, "async_goto_xy_position not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->async_goto_xy_position)(stage, x, y) == STAGE_ERROR ) {
		send_stage_error_text(stage, "async_goto_xy_position failed");
		return STAGE_ERROR;
	}
	
	return STAGE_SUCCESS;
}


int stage_async_goto_xyz_position (Stage* stage, double x, double y, double z)
{
	x *= stage->_dir[XAXIS];
	y *= stage->_dir[YAXIS];
	z *= stage->_dir[ZAXIS];

  	if(stage->_cops->async_goto_xyz_position == NULL) {
    	send_stage_error_text(stage, "async_goto_xyz_position not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->async_goto_xyz_position)(stage, x, y, z) == STAGE_ERROR ) {
		send_stage_error_text(stage, "async_goto_xyz_position failed");
		return STAGE_ERROR;
	}
	
#ifdef XY_ONLY
	if (focus_drive_set_pos(stage->focus_drive, z) == FOCUS_DRIVE_ERROR) {
		send_stage_error_text(stage, "Failed to goto z position");
		return STAGE_ERROR;
	}
#endif

	return STAGE_SUCCESS;
}

static int SaveXYZinRegistry (double x, double y, double z)
{
	char buffer[20];
	int err = 0;
	
	// Save stage position
	sprintf(buffer, "%.2f", x);
	err = RegWriteString (REGKEY_HKCU, "software\\GCI\\microscopy\\Stage", "x", buffer);
	sprintf(buffer, "%.2f", y);
	err |= RegWriteString (REGKEY_HKCU, "software\\GCI\\microscopy\\Stage", "y", buffer);
	sprintf(buffer, "%.2f", z);
	err |= RegWriteString (REGKEY_HKCU, "software\\GCI\\microscopy\\Stage", "z", buffer);
	
	return err;
}

int stage_get_xyz_position (Stage* stage, double *x, double *y, double *z)
{
  	if(stage->_cops->get_xyz_position == NULL) {
    	send_stage_error_text(stage, "Get xyz position not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->get_xyz_position)(stage, x, y, z) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to get xyz position");
		return STAGE_ERROR;
	}
	
#ifdef XY_ONLY
	if (focus_drive_get_pos(stage->focus_drive, z) == FOCUS_DRIVE_ERROR) {
		send_stage_error_text(stage, "Failed to get z position");
		return STAGE_ERROR;
	}
#endif

	//Each time the co-ords are read put them in the registry
	//The they can be retrieved following a crash.
	SaveXYZinRegistry(*x, *y, *z);

	(*x) *= stage->_dir[XAXIS];
	(*y) *= stage->_dir[YAXIS];
	(*z) *= stage->_dir[ZAXIS];
	
	return STAGE_SUCCESS;
}

int stage_get_xy_position (Stage* stage, double *x, double *y)
{
	double z;
	
  	if(stage->_cops->get_xyz_position == NULL) {
    	send_stage_error_text(stage, "Get xyz position not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->get_xyz_position)(stage, x, y, &z) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to get xyz position");
		return STAGE_ERROR;
	}
	
	(*x) *= stage->_dir[XAXIS];
	(*y) *= stage->_dir[YAXIS];
	
	return STAGE_SUCCESS;
}

int stage_fast_get_xyz_position(Stage* stage, double *x, double *y, double *z)
{
	//Return the values in the registry if present
	if (ReadXYZfromRegistry (x, y, z) == 0)
		return STAGE_SUCCESS;

	return stage_get_xyz_position (stage, x, y, z);
}

/*
int stage_get_xy_position (Stage* stage, double *x, double *y)
{
	double tmp_x, tmp_y, tmp_z;
	
	if(stage_get_xyz_position(stage, &tmp_x, &tmp_y, &tmp_z) == STAGE_ERROR)
		return STAGE_ERROR;
		
	*x = tmp_x;
	*y = tmp_y;
		
	return STAGE_SUCCESS;
}
*/


int stage_async_rel_move_by (Stage* stage, double x, double y, double z)
{
	int retval;
	
	//This will send xyz to the controller. If there's no z no problem.
	
	x *= stage->_dir[XAXIS];
	y *= stage->_dir[YAXIS];

	if(stage->_cops->async_rel_move_by == NULL) {
    	send_stage_error_text(stage, "async_rel_move_by not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->async_rel_move_by)(stage, x, y, z) == STAGE_ERROR ) {
		send_stage_error_text(stage, "async_rel_move_by failed");
		return STAGE_ERROR;
	}
	
#ifndef XY_ONLY
	return STAGE_SUCCESS;
#endif

	if (FP_Compare(0.0, z) == 0) return STAGE_SUCCESS;
	
	//This will send Z to the z controller if present
	retval = stage_rel_move_by_z (stage, z);

	return retval;
}


int stage_rel_move_by (Stage* stage, double x, double y, double z)
{
	double xCur, yCur, zCur;
	
	if (stage->_move_type == FAST) { //assume no problems
		return stage_async_rel_move_by (stage, x, y, z);
	}
	
	if(stage_get_xyz_position(stage, &xCur, &yCur, &zCur) == STAGE_ERROR)
		return STAGE_ERROR;	

	return stage_goto_xyz_position (stage, xCur+x, yCur+y, zCur+z, xCur, yCur, zCur);
}

int stage_rel_move_by_z (Stage* stage, double z)
{
#ifndef XY_ONLY
	return stage_rel_move_by (stage, 0.0, 0.0, z);
#else

	int retval, tolerance, speed;
	double zCur;
	
	if (stage->_move_type == FAST) { //assume no problems
		stage_get_z_tolerance(stage, &tolerance);
		if (tolerance != 9) stage_set_z_tolerance(stage, 9);
		stage_get_z_speed(stage, &speed);
		if (speed != 1) stage_set_z_speed(stage, 1);
	}

	if (focus_drive_get_pos(stage->focus_drive, &zCur) == FOCUS_DRIVE_ERROR) {
		send_stage_error_text(stage, "Failed to get z position");
		return STAGE_ERROR;
	}

	retval = stage_goto_z_position (stage, zCur+z);

	if (stage->_move_type == FAST) { 
		if (tolerance != 9) stage_set_z_tolerance(stage, tolerance);
		if (speed != 1) stage_set_z_speed(stage, speed);
	}
	
	return retval;
#endif
}

/*
int stage_set_x_datum (Stage* stage, double x)
{
	double tmp_x, tmp_y, tmp_z;
	
	if(stage_get_xyz_datum(stage, &tmp_x, &tmp_y, &tmp_z) == STAGE_ERROR)
		return STAGE_ERROR;
		
	if(stage_set_xyz_datum(stage, x, tmp_y, tmp_z) == STAGE_ERROR)
		return STAGE_ERROR;	
		
	return STAGE_SUCCESS;
}


int stage_set_y_datum (Stage* stage, double y)
{
	double tmp_x, tmp_y, tmp_z;
	
	if(stage_get_xyz_datum(stage, &tmp_x, &tmp_y, &tmp_z) == STAGE_ERROR)
		return STAGE_ERROR;
		
	if(stage_set_xyz_datum(stage, tmp_x, y, tmp_z) == STAGE_ERROR)
		return STAGE_ERROR;	
		
	return STAGE_SUCCESS;
}

*/
int stage_goto_z_datum (Stage* stage)
{
	int err;
	
#ifdef XY_ONLY
	err = focus_drive_goto_datum(stage->focus_drive);
#else
	err = stage_goto_z_position (stage, 0.0);
#endif

	return err;
}

int stage_set_z_datum (Stage* stage)
{
	double cur_z, x, y;
	
#ifdef XY_ONLY
	if (focus_drive_get_pos(stage->focus_drive, &cur_z) == FOCUS_DRIVE_ERROR) {
		send_stage_error_text(stage, "Failed to get z position");
		return STAGE_ERROR;
	}

	if (!stage->_enabled_axis[ZAXIS]) cur_z = 0.0;

	if (focus_drive_set_datum(stage->focus_drive, cur_z) == FOCUS_DRIVE_ERROR) {
		send_stage_error_text(stage, "Failed to set z datum");
		return STAGE_ERROR;
	}
#else
  	if(stage->_cops->get_xyz_position == NULL) {
    	send_stage_error_text(stage, "Get xyz position not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->get_xyz_position)(stage, &x, &y, &cur_z) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to get xyz position");
		return STAGE_ERROR;
	}
#endif

	//Avoid accidental Z drive calamities
	SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_Z_ABS, 0.0);
	
	stage->_datum.z = cur_z; 
	
	stage_save_limits(stage);

	return STAGE_SUCCESS;
}


int stage_set_xy_datum (Stage* stage)
{
	double cur_x, cur_y, cur_z;
	
	if(stage->_cops->set_xy_datum == NULL) {
    	send_stage_error_text(stage, "set_xy_datum not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if(stage_get_xyz_position(stage, &cur_x, &cur_y, &cur_z) == STAGE_ERROR)
		return STAGE_ERROR;

	if (!stage->_enabled_axis[XAXIS]) cur_x = 0.0;
	if (!stage->_enabled_axis[YAXIS]) cur_y = 0.0;
	
	//Set new values of X and Y limits
	stage->_limits.min_x -= cur_x; stage->_limits.max_x -= cur_x;
	stage->_limits.min_y -= cur_y; stage->_limits.max_y -= cur_y;
	//Adjust the roi position
	stage->_roi.min_x -= cur_x; stage->_roi.max_x -= cur_x;
	stage->_roi.min_y -= cur_y; stage->_roi.max_y -= cur_y;
	stage_setup_graph(stage);
	stage_save_limits(stage);

	//Tell stage controller that current xy position is to become zero
	if( (*stage->_cops->set_xy_datum)(stage, 0.0, 0.0) == STAGE_ERROR ) {
		send_stage_error_text(stage, "set_xy_datum failed");
		return STAGE_ERROR;
	}
	
	stage->_datum.x = cur_x; 
	stage->_datum.y = cur_y; 
	
	return STAGE_SUCCESS;
}


int stage_set_xyz_datum (Stage* stage)
{
	if (stage_set_xy_datum(stage) == STAGE_ERROR)
		return STAGE_ERROR;
	
	return stage_set_z_datum (stage);
}


int stage_get_x_datum (Stage* stage, double *x)
{
	double tmp_x, tmp_y, tmp_z;
	
	if(stage_get_xyz_datum(stage, &tmp_x, &tmp_y, &tmp_z) == STAGE_ERROR)
		return STAGE_ERROR;
		
	*x = tmp_x;	
		
	return STAGE_SUCCESS;
}


int stage_get_y_datum (Stage* stage, double *y)
{
	double tmp_x, tmp_y, tmp_z;
	
	if(stage_get_xyz_datum(stage, &tmp_x, &tmp_y, &tmp_z) == STAGE_ERROR)
		return STAGE_ERROR;
		
	*y = tmp_y;	
		
	return STAGE_SUCCESS;
}


int stage_get_z_datum (Stage* stage, double *z)
{
	*z =stage->_datum.z;
	
	return STAGE_SUCCESS;
}


int stage_get_xyz_datum (Stage* stage, double *x, double *y, double *z)
{
	*x = stage->_datum.x;
	*y = stage->_datum.y;
	*z = stage->_datum.z;
	
	return STAGE_SUCCESS;
}

void  stage_set_z_offset (Stage* stage, double z)
{
	stage->_focus_offset = z;
}


int stage_is_moving (Stage* stage, int *status)
{
	if(stage->_cops->is_moving == NULL) {
    	send_stage_error_text(stage, "is_moving not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->is_moving)(stage, status) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to execute is_moving");
		return STAGE_ERROR;
	}
	
	return STAGE_SUCCESS;

}


int stage_set_roi(Stage* stage, Roi roi)
{
	stage->_roi = roi;
	
	return STAGE_SUCCESS;
}

int stage_get_roi(Stage* stage, Roi *roi)
{
	*roi = stage->_roi;

	return STAGE_SUCCESS;
}


int stage_get_info(Stage* stage, char *info)
{
  	if(stage->_cops->get_info == NULL) {
    	send_stage_error_text(stage, "Get info not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

	if( (*stage->_cops->get_info)(stage, info) == STAGE_ERROR ) {
		send_stage_error_text(stage, "Failed to get info");
		return STAGE_ERROR;
	}
	
	return STAGE_SUCCESS;
}


int  stage_save_settings_as_default(Stage* stage)
{
	char data_dir[500];
	
	if(stage->_data_dir == NULL)
		str_get_path_for_my_documents(data_dir);	
	else
		strcpy(data_dir, stage->_data_dir);
	
	if(stage_save_settings(stage, data_dir) == STAGE_ERROR)
		return STAGE_ERROR;
	
	return STAGE_SUCCESS;
}


int stage_load_default_settings(Stage* stage)
{
	char data_dir[500];
	
	if(stage->_data_dir == NULL)
		str_get_path_for_my_documents(data_dir);	
	else
		strcpy(data_dir, stage->_data_dir);
	
	if(stage_load_settings(stage, data_dir) == STAGE_ERROR)
		return STAGE_ERROR;
		
	return STAGE_SUCCESS;
}


int stage_save_settings(Stage* stage, const char *filename)
{
	if(filename == NULL)
    	return STAGE_ERROR;

	if(stage->_cops->save_settings == NULL) {
    	send_stage_error_text(stage,"Save settings not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

  	if( (*stage->_cops->save_settings)(stage, filename)  == STAGE_ERROR) {
  		send_stage_error_text(stage,"Can not save settings for device %s\n", stage->_description);
  		return STAGE_ERROR;
  	}
	
	return STAGE_SUCCESS;
}


int stage_load_settings(Stage* stage, const char *filename)
{
	if(filename == NULL)
    	return STAGE_ERROR;

	if(stage->_cops->load_settings == NULL) {
    	send_stage_error_text(stage,"Load settings not implemented for device %s\n", stage->_description);
    	return STAGE_ERROR;
  	}

  	if( (*stage->_cops->load_settings)(stage, filename)  == STAGE_ERROR)
  		return STAGE_ERROR;
	
	return STAGE_SUCCESS;
}

int stage_save_limits (Stage* stage)
{
	CVIXMLElement root;
	CVIXMLDocument stageLimits;
	char path[MAX_PATHNAME_LEN];
	int fsize, ret;
	
	//Save limits of travel relative to current datum in xml format
	GetProjectDir(path);
	strcat(path, "\\Microscope Data\\");
	if (!FileExists(path, &fsize)) MakeDir (path);
	strcat(path,"stageLimits.xml");
	if (FileExists(path, &fsize))
		SetFileAttrs (path, 0, -1, -1, -1);   //clear read only flag
	
	ret = CVIXMLNewDocument ("StageLimits", &stageLimits);
	ret = CVIXMLGetRootElement (stageLimits, &root);
	
	if (newXmlSettingDbl (root, "xMin", stage->_limits.min_x)) goto Error;
	if (newXmlSettingDbl (root, "xMax", stage->_limits.max_x)) goto Error;
	if (newXmlSettingDbl (root, "yMin", stage->_limits.min_y)) goto Error;
	if (newXmlSettingDbl (root, "yMax", stage->_limits.max_y)) goto Error;
	if (newXmlSettingDbl (root, "zMin", stage->_limits.min_z)) goto Error;
	if (newXmlSettingDbl (root, "zMax", stage->_limits.max_z)) goto Error;
	if (newXmlSettingDbl (root, "zDatum", stage->_datum.z)) goto Error;

	ret = CVIXMLSaveDocument (stageLimits, 0, path);
	CVIXMLDiscardElement(root);
	CVIXMLDiscardDocument (stageLimits);

	SetFileAttrs (path, 1, -1, -1, -1);   //set read only flag
  	
  	return STAGE_SUCCESS;

Error:

	if (root >= 0) CVIXMLDiscardElement(root);
	if (stageLimits >= 0) CVIXMLDiscardDocument(stageLimits);

	return STAGE_ERROR;
}	

int stage_load_limits (Stage* stage)
{
	CVIXMLElement root;
	CVIXMLDocument stageLimits;
	char path[MAX_PATHNAME_LEN];
	int ret;
	
	GetProjectDir(path);
	strcat(path, "\\Microscope Data\\stageLimits.xml");
	if (!FileExists(path, NULL)) return STAGE_ERROR;
	
	ret = CVIXMLLoadDocument (path, &stageLimits);
	ret = CVIXMLGetRootElement (stageLimits, &root);
	
	if (getXmlSettingDbl (root, "xMin", &stage->_limits.min_x))	goto Error;
	if (getXmlSettingDbl (root, "xMax", &stage->_limits.max_x))	goto Error;
	if (getXmlSettingDbl (root, "yMin", &stage->_limits.min_y))	goto Error;
	if (getXmlSettingDbl (root, "yMax", &stage->_limits.max_y))	goto Error;
	if (getXmlSettingDbl (root, "zMin", &stage->_limits.min_z))	goto Error;
	if (getXmlSettingDbl (root, "zMax", &stage->_limits.max_z))	goto Error;
	if (getXmlSettingDbl (root, "zDatum", &stage->_datum.z)) goto Error;

	CVIXMLDiscardElement(root);
	CVIXMLDiscardDocument (stageLimits);
  	
	if (focus_drive_set_datum(stage->focus_drive, stage->_datum.z) == FOCUS_DRIVE_ERROR) {
		send_stage_error_text(stage, "Failed to set z datum");
		return STAGE_ERROR;
	}
	//Avoid accidental Z drive calamities
	SetCtrlVal (stage->_main_ui_panel, STAGE_PNL_Z_ABS, 0.0);

  	return STAGE_SUCCESS;

Error:
	if (root >= 0)          CVIXMLDiscardElement(root);
	if (stageLimits >= 0) CVIXMLDiscardDocument(stageLimits);

	return STAGE_ERROR;
}	

int stage_display_main_ui(Stage* stage)
{
	if(stage->_params_ui_panel != -1)
		SetCtrlAttribute(stage->_main_ui_panel, STAGE_PNL_ADVANCED, ATTR_DIMMED, 0); 

	if(stage->_main_ui_panel != -1) {
		stage_read_or_write_main_panel_registry_settings(stage, 0); 
		DisplayPanel(stage->_main_ui_panel);
	}
	
	return STAGE_SUCCESS;
}


int  stage_hide_main_ui(Stage* stage)
{
	if(stage->_main_ui_panel != -1) {
		stage_read_or_write_main_panel_registry_settings(stage, 1);
		HidePanel(stage->_main_ui_panel);
	}
	
	return STAGE_SUCCESS;
}


int  stage_display_params_ui(Stage* stage)
{
	if(stage->_params_ui_panel != -1) {
		//Restore last used panel positions
		stage_read_or_write_params_panel_registry_settings(stage, 0);
		
		GCI_ShowPasswordProtectedPanel(stage->_params_ui_panel);
	}
		
	return STAGE_SUCCESS;
}


int  stage_hide_params_ui(Stage* stage)
{
  	if(stage->_params_ui_panel != -1) {
  		stage_read_or_write_params_panel_registry_settings(stage, 1);
		HidePanel(stage->_params_ui_panel);
	}
		
	return STAGE_SUCCESS;
}


int  stage_hide_ui(Stage* stage)
{
	if(stage->_main_ui_panel != -1) {
		stage_read_or_write_main_panel_registry_settings(stage, 1);
		HidePanel(stage->_main_ui_panel);
	}

	if(stage->_params_ui_panel != -1) {
  		stage_read_or_write_params_panel_registry_settings(stage, 1);
		HidePanel(stage->_params_ui_panel);
	}
		
	return STAGE_SUCCESS;
}


int  stage_destroy_params_ui(Stage* stage)
{
  	if(stage->_params_ui_panel != -1) {
  		stage_read_or_write_params_panel_registry_settings(stage, 1);
  		DiscardPanel(stage->_params_ui_panel);
  	}
  	
  	stage->_params_ui_panel = -1;
  	
  	return STAGE_SUCCESS;
}

