//--------------------------------------------------------------------------
// Microsoft Visual Studio
//
// Copyright (c) 1998 - 2003 Microsoft Corporation Inc.
// All rights reserved
//
//
// venusids.h
// Venus command table ids
//---------------------------------------------------------------------------
//NOTE: billhie. CTC compiler cannot handle #pragma once (it issues a warning)
#ifndef __VENUSIDS_H__
#define __VENUSIDS_H__

#include "sharedvenusids.h"
#include "venuscmddef.h"

//----------------------------------------------------------------------------
//
// GUID Identifiers
//
// Define CommandSet GUIDs in two ways - C compiler and CTC compiler.
// ** MAKE UPDATES TO BOTH GUID DECLS BELOW **
//----------------------------------------------------------------------------
#ifdef DEFINE_GUID

//guidDirPkgGrpId
// {5ADFC620-064F-40ec-88D1-F3F4F01EFC6F}
//guidDirPkgCmdId

// {883D561D-1199-49f3-A19E-78B5ADE9C6C1}
DEFINE_GUID(guidVenusStartPageCmdId, 
0x883d561d, 0x1199, 0x49f3, 0xa1, 0x9e, 0x78, 0xb5, 0xad, 0xe9, 0xc6, 0xc1);

//{9685C4E9-4D67-4a43-BC3E-CF405F9DAC05}
DEFINE_GUID(guidSilverlightCmdId, 
0x9685C4E9, 0x4D67, 0x4a43, 0xBC, 0x3E, 0xCF, 0x40, 0x5F, 0x9D, 0xAC, 0x05);

// XML editor guid
//{FA3CD31E-987B-443A-9B81-186104E8DAC1}
DEFINE_GUID(guidXmlEditor, 0XFA3CD31E, 0X987B, 0X443A, 0X9B, 0X81, 0X18, 0X61, 0X04, 0XE8, 0XDA, 0XC1);

#else

// {883D561D-1199-49f3-A19E-78B5ADE9C6C1}
#define guidVenusStartPageCmdId { 0x883d561d, 0x1199, 0x49f3, { 0xa1, 0x9e, 0x78, 0xb5, 0xad, 0xe9, 0xc6, 0xc1 } }

//{9685C4E9-4D67-4a43-BC3E-CF405F9DAC05}
#define guidSilverlightCmdId { 0x9685C4E9, 0x4D67, 0x4a43, { 0xBC, 0x3E, 0xCF, 0x40, 0x5F, 0x9D, 0xAC, 0x05 }}

// XML editor guid
//{FA3CD31E-987B-443A-9B81-186104E8DAC1}
#define guidXmlEditor { 0XFA3CD31E, 0X987B, 0X443A, { 0X9B, 0X81, 0X18, 0X61, 0X04, 0XE8, 0XDA, 0XC1 }};

// {69021D88-2F43-46E0-8A43-7F00F5B24176}
#define guidDeploymentImages { 0x69021d88, 0x2f43, 0x46e0, { 0x8a, 0x43, 0x7f, 0x0, 0xf5, 0xb2, 0x41, 0x76 } }


#endif

//---------------------------------------------------------------------------
// Comand Table Version
//---------------------------------------------------------------------------
#define COMMANDTABLE_VERSION		1

// web package menus
#define IDM_VENUS_CSCD_ADDWEB      6
#define IDM_VENUS_WEB              8
#define IDM_VENUS_CSCD_ADDFOLDER   9
#define IDM_VENUS_CTXT_ADDREFERENCE 10
#define IDM_VENUS_CTXT_ITEMWEBREFERENCE 11
#define IDM_VENUS_TOOLS_WEBPI           15

// "Add Web" Menu Groups
#define IDG_VENUS_ADDWEB_CASCADE          25
#define IDG_VENUS_ADDFOLDER               26
#define IDG_VENUS_CTX_REFERENCE           27
#define IDG_VENUS_PACKAGE                 30
#define IDG_VENUS_CTXT_PACKAGE            31

//Command IDs
#define icmdNewWeb                      0x002B
#define icmdOpenExistingWeb             0x002C
#define icmdAddNewWeb                   0x002D
#define icmdAddExistingWeb              0x002E
#define icmdValidatePage                0x002F
#define icmdOpenSubWeb                  0x0032
#define icmdAddAppAssemblyFolder        0x0034
#define icmdAddAppCodeFolder            0x0035
#define icmdAddAppGlobalResourcesFolder 0x0036
#define icmdAddAppLocalResourcesFolder  0x0037
#define icmdAddAppWebReferencesFolder   0x0038
#define icmdAddAppDataFolder            0x0039
#define icmdAddAppBrowsersFolder        0x0040
#define icmdAddAppThemesFolder          0x0041
#define icmdRunFxCop                    0x0042
#define icmdFxCopConfig                 0x0043
#define icmdBuildLicenseDll             0x0044
#define icmdUpdateReference             0x0045
#define icmdRemoveWebReference          0x0046
#define icmdCreatePackage               0x0050
#define icmdCleanPackage                0x0051
#define icmdContextCreatePackage        0x0052
#define icmdContextCleanPackage         0x0053
#define icmdPackageSettings             0x0054
#define icmdContextPackageSettings      0x0055
#define icmdNewVirtualFolder            0x0058

// This command never appears on a menu or toolbar. It is used internally to invoke browse with behavior
// from the debug controller.
#define icmdDebugStartupBrowseWith      0x0080

// "Web" Menu Groups - Start at 0x100 - they share the same menu guid with 
// commands "guidVenusCmdId"
#define IDG_VS_BUILD_VAILIDATION        0x0100
#define IDG_VENUS_CTX_SUBWEB            0x0101
#define IDG_CTX_REFERENCE               0x0102
#define IDG_CTX_PUBLISH                 0x0103
#define IDG_CTX_BUILD                   0x0104
#define IDG_VENUS_RUN_FXCOP             0x0105
#define IDG_VENUS_RUN_FXCOP_CTXT_PROJ   0x0106
#define IDG_VENUS_CTX_ITEM_WEBREFERENCE  0x0107
#define IDG_VENUS_CTXT_CONFIG_TRANSFORM 0x0108


// Start Page commands (introduced in Whidbey, some re-used in Orcas)
// *** These are referenced in Web.vssettings and WebExpress.vssettings 
// do not change the numbers without updating that file as well!
#define cmdidStartPageCreatePersonalWebSite		0x5000
#define cmdidStartPageCreateWebSite				0x5001
#define cmdidStartPageCreateWebService			0x5002
#define cmdidStartPageStarterKit				0x5003
#define cmdidStartPageCommunity					0x5004
#define cmdidStartPageIntroduction				0x5005
#define cmdidStartPageGuidedTour				0x5006
#define cmdidStartPageWhatsNew					0x5007
#define cmdidStartPageHowDoI					0x5008

// Silverlight commmands
#define cmdidSLOpenInBlend          100
#define cmdidSLAddJScriptCode       101

// Orcas Start Page commands for VWDExpress and other SKUs 
// *** These are referenced in WebExpress.vssettings 
// do not change the numbers without updating that file as well!

#define cmdidVWDStartPageVideoFeatureTour                0x5009
#define cmdidVWDStartPageLearnWebDevelopment             0x500A
#define cmdidVWDStartPageWhatsNew                        0x500B
#define cmdidVWDStartPageBeginnerDeveloperLearningCenter 0x500C
#define cmdidVWDStartPageASPNETDownloads                 0x500D
#define cmdidVWDStartPageASPNETForums                    0x500E
#define cmdidVWDStartPageASPNETCommunitySite             0x500F
#define cmdidVWDStartPageCreateYourFirstWebSite          0x5010
#define cmdidVWDStartPageExplore3rdPartyExtensions       0x5011

// Silverlight defined command id's (from silverlightmenuids.h)
#define cmdAddSilverlightLink          102

#define CreatePackageImage 1
#define PackageSettingsImage 2

#endif
// End of venusids.h
