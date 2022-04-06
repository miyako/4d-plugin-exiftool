/* --------------------------------------------------------------------------------
 #
 #	4DPlugin-ExifTool.h
 #	source generated by 4D Plugin Wizard
 #	Project : ExifTool
 #	author : miyako
 #	2022/04/06
 #  
 # --------------------------------------------------------------------------------*/

#ifndef PLUGIN_EXIFTOOL_H
#define PLUGIN_EXIFTOOL_H

#include "4DPluginAPI.h"
#include "4DPlugin-JSON.h"

#if VERSIONMAC
#import <Cocoa/Cocoa.h>
#endif

#include "ExifTool.h"

#pragma mark -

void ExifTool_Get_Metadata(PA_PluginParameters params);
void ExifTool_Set_Metadata(PA_PluginParameters params);

static void u16_to_u8(CUTF16String& u16, std::string& u8);
static void u8_to_u16(std::string& u8, CUTF16String& u16);

#endif /* PLUGIN_EXIFTOOL_H */