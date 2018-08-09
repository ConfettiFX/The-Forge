	//----------------------------------------------------------------------------
//
//  Microsoft Visual Studio
//
//  Microsoft Confidential
//  Copyright 1997-1998 Microsoft Corporation.  All Rights Reserved.
//
//  File:	menucmds.h
//	Area:	Help Package Commands
//
//  Contents:
//		Helps System Package Menu, Group, Command IDs
//
//----------------------------------------------------------------------------
#ifndef __HELPIDS_H_
#define __HELPIDS_H_


#ifndef NOGUIDS

#ifdef DEFINE_GUID
  // WB package object CLSID
  DEFINE_GUID (guidHelpCmdId,
    0x4a79114a, 0x19e4, 0x11d3, 0xb8, 0x6b, 0x0, 0xc0, 0x4f, 0x79, 0xf8, 0x2);

  DEFINE_GUID (guidHelpGrpId,
    0x4a79114b, 0x19e4, 0x11d3, 0xb8, 0x6b, 0x0, 0xc0, 0x4f, 0x79, 0xf8, 0x2);

  DEFINE_GUID (guidHelpPkg,
    0x4a791146, 0x19e4, 0x11d3, 0xb8, 0x6b, 0x0, 0xc0, 0x4f, 0x79, 0xf8, 0x2);

  // This is the GUID used by Search Window to do web toolbar visibility. It should be in sync with
  // VsCoreResIds (defined in \env\vscore\package\vscorepackage.cs
  // {E2F8DA06-F098-4508-B732-D8684EC10972}
  DEFINE_GUID (guidHelpSearchCmdUI, 
    0xe2f8da06, 0xf098, 0x4508, 0xb7, 0x32, 0xd8, 0x68, 0x4e, 0xc1, 0x9, 0x72);

#else
// {4A79114A-19E4-11d3-B86B-00C04F79F802}
#define guidHelpCmdId    {0x4a79114a, 0x19e4, 0x11d3, {0xb8, 0x6b, 0x0, 0xc0, 0x4f, 0x79, 0xf8, 0x2 }}

// {4A79114B-19E4-11d3-B86B-00C04F79F802}
#define guidHelpGrpId    {0x4a79114b, 0x19e4, 0x11d3, {0xb8, 0x6b, 0x0, 0xc0, 0x4f, 0x79, 0xf8, 0x2 }}


// The following is the same as CLSID_HelpPackage but for consumption by CTC.
// {4A791146-19E4-11d3-B86B-00C04F79F802}
#define guidHelpPkg			{0x4a791146, 0x19e4, 0x11d3, {0xb8, 0x6b, 0x0, 0xc0, 0x4f, 0x79, 0xf8, 0x2}}

// This is the GUID used by Search Window to do web toolbar visibility. It should be in sync with
// VsCoreResIds (defined in \env\vscore\package\vscorepackage.cs
// {E2F8DA06-F098-4508-B732-D8684EC10972}
#define guidHelpSearchCmdUI {0xe2f8da06, 0xf098, 0x4508, { 0xb7, 0x32, 0xd8, 0x68, 0x4e, 0xc1, 0x9, 0x72}}

#endif //DEFINE_GUID

#endif //NOGUIDS

	
// Menus
#define IDM_HELP_CONTENTS           0x0001
#define IDM_HELP_KEYWORDS           0x0002
#define IDM_HELP_SEARCH             0x0003

#define IDM_HELP_MENU_MSONTHWEB     0x0100

#define IDM_HLPTOC_CTX		    0x0200
#define IDM_HELP_RESLIST_CTX        0x0300
#define IDM_HELP_RESLIST_CTX_SORTBY 0x0400

// Groups
#define IDG_HELP_GRP                       0x0010
#define IDG_HELP_FEEDBACK_GRP              0x0040
#define IDG_HELP_MENU_FEEDBACK_GRP         0x0041

#define IDG_HLPTOC_CTX_PRINT        0x0050

#define IDG_HELP_RESLIST_CTX_SORTBY 0x0060
#define IDG_HELP_RESLIST_CTX_COLUMNS 0x0070

#define IDG_HELP_MSONTHEWEB_NEWS    0x0100
#define IDG_HELP_MSONTHEWEB_INFO    0x0200
#define IDG_HELP_MSONTHEWEB_HOME    0x0300
#define IDM_HELP_SET_PREFS          0X0500
#define IDM_HELP_SET_SUB_PREFS      0x0600
#define IDM_HELP_FEEDBACK           0X0700

//Command IDs
#define icmdHelpViewer                0x0100
#define icmdHelpAskAQuestion          0x0106
#define icmdHelpSendFeedback          0x0107

#define icmdHelpSearchControls        0x010B
#define icmdHelpSearchAddins          0x010C
#define icmdHelpSearchSamples         0x010D
#define icmdHelpSearchSnippets        0x010E
#define icmdHelpSearchStarterKits     0x010F

#define icmdHelpForceSelfDestruct     0x011C

#define icmdHelpManager               0x011D

//#define icmdHelpManager               0x011D

#define icmdHelpPrefOnline           0x011E
#define icmdHelpPrefOffline          0x011F

// TOC contex menu                  
#define icmdPrintTopic              0x0120
#define icmdPrintChildren           0x0121

#define icmdSortByCol1              0x0130
#define icmdSortByCol2              0x0131
#define icmdSortByCol3              0x0132
#define icmdSortByCol4              0x0133
#define icmdSortByCol5              0x0134
#define icmdSortByCol6              0x0135
#define icmdSortByCol7              0x0136
#define icmdSortByCol8              0x0137
#define icmdSortByCol9              0x0138
#define icmdSortByCol10             0x0139

#define icmdSortByColMin            icmdSortByCol1
#define icmdSortByColMax            icmdSortByCol10

#define icmdHelpF1AsyncComplete     0x0300

///////////////////////////////////////////////////////////////////////////////
//Menu cmds Bitmap IDs

#define bmpidVsHelpContentsCmd              1
#define bmpidVsHelpIndexCmd                 2
#define bmpidVsHelpSearchCmd                3
#define bmpidVsHelpIndexResultsCmd          4
#define bmpidVsHelpSearchResultsCmd         5
#define bmpidVSHelpFavWindowCmd             6
#define bmpidVSHelpFavAddCmd                7
#define bmpidVSHelpSaveSearchCmd            8
#define bmpidVSHelpAskAQuestionCmd          9
#define bmpidVSHelpCheckQuestionStatusCmd   10
#define bmpidVSHelpSendProductFeedbackCmd   11
#define bmpidVSHelpHowDoICmd                12


#endif //__HELPIDS_H_
