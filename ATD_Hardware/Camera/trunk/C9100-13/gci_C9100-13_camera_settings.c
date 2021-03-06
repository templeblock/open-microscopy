#include "gci_C9100-13_camera.h" 
#include "gci_C9100-13_camera_settings.h"
#include "gci_camera_ui.h"
#include "gci_C9100-13_camera_ui.h"
#include "string_utils.h"
#include "xml_utils.h"
#include "cvixml.h"
#include "iniparser.h"

#include <utility.h>
#include <formatio.h> 

//////////////////////////////////////////////////////////////////////////////////
// Module to save/recall settings for the Hamatsu Orca camera
// GP/RJL May 2006
//////////////////////////////////////////////////////////////////////////////////

static void CameraShortName (const char *pathName, char *fname)
{
    char dir[MAX_DIRNAME_LEN], drive[MAX_DRIVENAME_LEN];
    
    SplitPath (pathName, drive, dir, fname);
}


int C9100_13CameraSaveCameraSettings(GciC9100_13Camera *orca_camera, const char *filename)
{
	GciCamera *camera = (GciCamera*) orca_camera;

    char sname[MAX_FILENAME_LEN];
	int fsize, ival;
	CVIXMLElement root=-1, el=-1;
	CVIXMLDocument orcaSettings=-1;
	double fval;
	char buffer[50];
	unsigned int left, top, width, height;
	unsigned char auto_centered;
	
	//Save all camera settings in xml format
	if (CVIXMLNewDocument ("OrcaSettings", &orcaSettings))
		return CAMERA_ERROR;
	
	if (CVIXMLGetRootElement (orcaSettings, &root))
		goto Error;
	
	fval = gci_camera_get_exposure_time(camera);
	if (newXmlSettingDbl (root, "Exposure", fval) < 0) 	goto Error;
	
	ival = gci_camera_get_average_frame_number(camera);  
	if (newXmlSettingInt (root, "NoFrames", ival) < 0) 	goto Error;
	
	gci_camera_get_gain(camera, CAMERA_ALL_CHANNELS, &fval) ;
	if (newXmlSettingDbl (root, "Gain", fval) < 0) 		goto Error;
	
	gci_camera_get_size_position(camera, &left, &top, &width, &height, &auto_centered);
	if (newXmlSettingInt (root, "SubWindowLeft", left) < 0) 		goto Error;
	if (newXmlSettingInt (root, "SubWindowTop", top) < 0) 			goto Error;
	if (newXmlSettingInt (root, "SubWindowCols", width) < 0) 		goto Error;
	if (newXmlSettingInt (root, "SubWindowRows", height) < 0) 		goto Error;
	if (newXmlSettingInt (root, "AutoCentre", auto_centered) < 0) 	goto Error;
	
	GetCtrlVal(camera->_extra_ui_panel, EXTRA_PNL_SUBWINDOW_PRSETWINDOW, buffer);
	if (newXmlSettingStr (root, "PresetWindow", buffer) < 0) 		goto Error;
	
	ival = gci_camera_get_data_mode(camera);
	if (newXmlSettingInt (root, "DataMode", ival) < 0) 	goto Error;
	
	ival = gci_camera_get_light_mode(camera);
	if (newXmlSettingInt (root, "LightMode", ival) < 0) 	goto Error;
	
	ival = gci_camera_get_binning_mode(camera);
	if (newXmlSettingInt (root, "Binning", ival) < 0) 	goto Error;
	
	gci_camera_get_blacklevel(camera, CAMERA_ALL_CHANNELS, &fval);
	if (newXmlSettingDbl (root, "BlackLevel", fval) < 0) goto Error;
	
	if (FileExists(filename, &fsize))
		SetFileAttrs (filename, 0, -1, -1, -1);   //clear read only flag
	
	if (CVIXMLSaveDocument (orcaSettings, 0, filename)) goto Error;
	
	CVIXMLDiscardElement(root);
	CVIXMLDiscardDocument (orcaSettings);
	SetFileAttrs (filename, 1, -1, -1, -1);   //set read only flag

    CameraShortName (filename, sname);		 
    SetCtrlVal (camera->_main_ui_panel, CAMERA_PNL_FILENAME, sname);
    
    return CAMERA_SUCCESS;
    
Error:
	if (el >= 0)
		CVIXMLDiscardElement(el);
		
	if (root >= 0)
		CVIXMLDiscardElement(root);
		
	if (orcaSettings >= 0)
		CVIXMLDiscardDocument(orcaSettings);

	return CAMERA_ERROR;
}


int C9100_13CameraLoadCameraSettings(GciC9100_13Camera *orca_camera, const char *filename)
{
	GciCamera *camera = (GciCamera*) orca_camera; 

    char sname[MAX_FILENAME_LEN], str[50];
	int is_live;
	int ival, left, top, width, height, auto_centre;
	double fval;
	CVIXMLElement root=-1, el=-1;
	CVIXMLDocument orcaSettings=-1;

	if (openXmlRoot ((char *)filename, "OrcaSettings", &orcaSettings, &root) < 0)
		return CAMERA_ERROR;

	if (getXmlSettingInt (root, "DataMode", &ival) < 0)   goto Error;
	gci_camera_set_data_mode(camera, ival);
	
	if (getXmlSettingInt (root, "LightMode", &ival) >= 0) 
		gci_camera_set_light_mode(camera, ival);
	
	if (getXmlSettingDbl (root, "Exposure", &fval) < 0)   goto Error;
	gci_camera_set_exposure_time(camera, fval);
	
	if (getXmlSettingInt (root, "NoFrames", &ival) < 0)   goto Error;
	SetCtrlVal(camera->_main_ui_panel, CAMERA_PNL_AVERAGENUMBER, ival);
	gci_camera_set_average_frame_number(camera, ival);
	
	if (getXmlSettingDbl (root, "Gain", &fval) < 0)   goto Error;
	gci_camera_set_gain(camera, CAMERA_ALL_CHANNELS, fval);

	// Sub Window
	if (getXmlSettingInt (root, "SubWindowLeft", &left) < 0)   goto Error;
	if (getXmlSettingInt (root, "SubWindowTop", &top) < 0)   goto Error;
	if (getXmlSettingInt (root, "SubWindowCols", &width) < 0)   goto Error;
	if (getXmlSettingInt (root, "SubWindowRows", &height) < 0)   goto Error;
	if (getXmlSettingInt (root, "AutoCentre", &auto_centre) < 0)   goto Error;
	gci_camera_set_size_position(camera, left, top, width, height, auto_centre);
	
	// Preset Window
	if (getXmlSettingStr (root, "PresetWindow", str, 50) < 0)   goto Error;
	sscanf(str, "%d %d", &width, &height);
	if(width != -1 && height != -1)
		gci_camera_set_size_position(camera, 0, 0, width, height, auto_centre); 
	
	// Binning
	if (getXmlSettingDbl (root, "BlackLevel", &fval) < 0)   goto Error;
	gci_camera_set_blacklevel(camera, CAMERA_ALL_CHANNELS, fval );

	closeXmlRoot (orcaSettings, root);

	// Set the filename in the ui
    CameraShortName (filename, sname);
    SetCtrlVal (camera->_main_ui_panel, CAMERA_PNL_FILENAME, sname);

	is_live = gci_camera_is_live_mode(camera);

	// This prevents timout errors hetting the next image. It preallocates & freeframes etc
	if(is_live) {
		gci_camera_set_live_mode(camera);
		gci_camera_activate_live_display(camera);
	}
	else {
		gci_camera_set_snap_mode(camera);
	}

    return CAMERA_SUCCESS;

Error:
	if (el >= 0)
		CVIXMLDiscardElement(el);
	
	if (root >= 0)
		CVIXMLDiscardElement(root);
	
	if (orcaSettings >= 0)
		CVIXMLDiscardDocument(orcaSettings);

	return CAMERA_ERROR;
}


// INI File Saving



int C9100_13_GetIniSectionString(GciC9100_13Camera *orca_camera, char *buffer)
{
	GciCamera *camera = (GciCamera*) orca_camera;    
	double bl; 
	int binning, lm;
	FILE *fp;
	double gain;
	DataMode dm;
	
	memset(buffer, 0, 1);   
	
	gci_camera_get_gain(camera, CAMERA_CHANNEL1, &gain);
	
	binning = gci_camera_get_binning_mode(camera);
	
    dm = gci_camera_get_data_mode(camera);
	
	gci_camera_get_blacklevel(camera, CAMERA_CHANNEL1, &bl);  
	
	lm = gci_camera_get_light_mode(camera);
	
	// Write the global section
	sprintf(buffer, "Exposure=%f\nGain=%f\nBlacklevel=%f\n"
					"Binning=%d\nDataMode=%d\nLightMode=%d\n\n",
					camera->_exposure_time, gain, bl, (int) binning, (int) dm, lm);
	
	str_change_char(buffer, '\n', '\0'); 
	
	return CAMERA_SUCCESS;  
}


int C9100_13CameraSaveIniCameraSettings(GciC9100_13Camera *orca_camera, const char * filepath, const char *mode)  
{
	GciCamera *camera = (GciCamera*) orca_camera;     
	char buffer[2000] = "";
	FILE *fp = NULL;
	
	C9100_13_GetIniSectionString(orca_camera, buffer);
	
	// clear the file
	fp = fopen(filepath, mode);
	fclose(fp);
	
	if(!WritePrivateProfileSection(UIMODULE_GET_NAME(camera), buffer, filepath))
		return CAMERA_ERROR;  
		
	return CAMERA_SUCCESS; 
}




static void loadFromIniDictionary(GciC9100_13Camera *orca_camera, dictionary* ini)
{
	char buffer[64];
	int tmp;
	double d_tmp;
	GciCamera *camera = (GciCamera*) orca_camera;     
	
	sprintf (buffer, "%s:Exposure", UIMODULE_GET_NAME(camera));
	d_tmp = iniparser_getdouble(ini, buffer, -1.0);
	if (d_tmp>0.0)
		gci_camera_set_exposure_time(camera, d_tmp);   

	sprintf (buffer, "%s:Gain", UIMODULE_GET_NAME(camera));
	d_tmp = iniparser_getdouble(ini, buffer, -1.0);
	if (d_tmp>=0.0)
		gci_camera_set_gain(camera, CAMERA_CHANNEL1, d_tmp);
	
	sprintf (buffer, "%s:Blacklevel", UIMODULE_GET_NAME(camera));
	tmp = iniparser_getint(ini, buffer, -1);
	if (tmp>=0)
		gci_camera_set_blacklevel(camera, CAMERA_CHANNEL1, tmp);     
	
	sprintf (buffer, "%s:Binning", UIMODULE_GET_NAME(camera));
	tmp = iniparser_getint(ini, buffer, -1);
	if (tmp>=0)
		gci_camera_set_binning_mode(camera, tmp);

	sprintf (buffer, "%s:DataMode", UIMODULE_GET_NAME(camera));
	tmp = iniparser_getint(ini, buffer, -1);
	if (tmp>=0)
		gci_camera_set_data_mode(camera, tmp);
	
	sprintf (buffer, "%s:LightMode", UIMODULE_GET_NAME(camera));
	tmp = iniparser_getint(ini, buffer, -1);
	if (tmp>=0)
		gci_camera_set_light_mode(camera, tmp);
}


int C9100_13CameraLoadIniCameraSettings(GciC9100_13Camera *orca_camera, const char * filepath)  
{
	dictionary* ini = iniparser_load  (filepath);    
	
	loadFromIniDictionary(orca_camera, ini);
		
	iniparser_freedict(ini);
	
	return CAMERA_SUCCESS; 
}
