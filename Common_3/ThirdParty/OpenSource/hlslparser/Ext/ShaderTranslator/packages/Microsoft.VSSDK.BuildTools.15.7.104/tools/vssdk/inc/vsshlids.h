//////////////////////////////////////////////////////////////////////////////
//
//Copyright 1996-2003 Microsoft Corporation.  All Rights Reserved.
//
//File: VSShlIds.H
//
//Contents:
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _VSSHLIDS_H_
#define _VSSHLIDS_H_



//////////////////////////////////////////////////////////////////////////////
//
// GUID Identifiers, created by Visual Studio Shell
//
//////////////////////////////////////////////////////////////////////////////
#ifndef NOGUIDS

#ifdef DEFINE_GUID
  // Guid if using Office provided icons
  DEFINE_GUID (guidOfficeIcon,
    0xd309f794, 0x903f, 0x11d0, 0x9e, 0xfc, 0x00, 0xa0, 0xc9, 0x11, 0x00, 0x4f);

  // Guid for shell icons
  DEFINE_GUID(guidVsShellIcons, 
    0x9cd93c42, 0xceef, 0x45ab, 0xb1, 0xb5, 0x60, 0x40, 0x88, 0xc, 0x95, 0x43);

  // Guid for the duplicate accelerator keys
  DEFINE_GUID (guidKeyDupe,
    0xf17bdae0, 0xa16d, 0x11d0, 0x9f, 0x4,  0x0,  0xa0, 0xc9, 0x11, 0x0,  0x4f);

  // Guid for Shell's group and menu ids
  DEFINE_GUID (guidSHLMainMenu,
    0xd309f791, 0x903f, 0x11d0, 0x9e, 0xfc, 0x00, 0xa0, 0xc9, 0x11, 0x00, 0x4f);

  // Guid for ClassView menu ids
  DEFINE_GUID(guidClassViewMenu, 
    0xfb61dcfe, 0xc9cb, 0x4964, 0x84, 0x26, 0xc2, 0xd3, 0x83, 0x34, 0x07, 0x8c);

  // Guid for DocOutline package commands
  DEFINE_GUID (guidDocOutlinePkg,
    0x21af45b0, 0xffa5, 0x11d0, 0xb6, 0x3f, 0x00, 0xa0, 0xc9, 0x22, 0xe8, 0x51);

  // Guid for CommonIDE package
  DEFINE_GUID (guidCommonIDEPackage,
    0x6E87CFAD, 0x6C05, 0x4adf, 0x9C, 0xD7, 0x3B, 0x79, 0x43, 0x87, 0x5B, 0x7C);

  // Guid for CommonIDE package commands
  DEFINE_GUID (guidCommonIDEPackageCmd,
    0x6767e06b, 0x5789, 0x472b, 0x8e, 0xd7, 0x1f, 0x20, 0x73, 0x71, 0x6e, 0x8c);

  // UIContext guid specifying that we're not in View Source mode
  DEFINE_GUID(guidNotViewSourceMode,
        0x7174c6a0, 0xb93d, 0x11d1, 0x9f, 0xf4, 0x0, 0xa0, 0xc9, 0x11, 0x0, 0x4f);

  // Guid for shared groups
  // {234A7FC1-CFE9-4335-9E82-061F86E402C1}
  DEFINE_GUID(guidSharedMenuGroup,
    0x234a7fc1, 0xcfe9, 0x4335, 0x9e, 0x82, 0x6, 0x1f, 0x86, 0xe4, 0x2, 0xc1);

  DEFINE_GUID(guidBuildCmdIcons,
    0x952691c5, 0x34d6, 0x462b, 0xac, 0x56, 0x9a, 0xb0, 0x97, 0x70, 0xa3, 0x0d);

  DEFINE_GUID(CMDSETID_StandardCommandSet2K,
    0x1496A755, 0x94DE, 0x11D0, 0x8C, 0x3F, 0x00, 0xC0, 0x4F, 0xC2, 0xAA, 0xE2);

  // new command set for Dev10
  DEFINE_GUID(CMDSETID_StandardCommandSet10, 
    0x5dd0bb59, 0x7076, 0x4c59, 0x88, 0xd3, 0xde, 0x36, 0x93, 0x1f, 0x63, 0xf0);

  // new command set for Dev11
  DEFINE_GUID(CMDSETID_StandardCommandSet11, 
    0xd63db1f0, 0x404e, 0x4b21, 0x96, 0x48, 0xca, 0x8d, 0x99, 0x24, 0x5e, 0xc3);

  // new command set for Dev12
  DEFINE_GUID(CMDSETID_StandardCommandSet12,
    0x2A8866DC, 0x7BDE, 0x4dc8, 0xA3, 0x60, 0xA6, 0x06, 0x79, 0x53, 0x43, 0x84);

  // new command set for Dev14
  // {4C7763BF-5FAF-4264-A366-B7E1F27BA958}
  DEFINE_GUID(CMDSETID_StandardCommandSet14,
    0x4c7763bf, 0x5faf, 0x4264, 0xa3, 0x66, 0xb7, 0xe1, 0xf2, 0x7b, 0xa9, 0x58);

  // new command set for Dev15
  // {712C6C80-883B-4AAD-B430-BBCA5256FA9D}
  DEFINE_GUID(CMDSETID_StandardCommandSet15,
    0x712c6c80, 0x883b, 0x4aad, 0xb4, 0x30, 0xbb, 0xca, 0x52, 0x56, 0xfa, 0x9d);

  // {489EE5BF-F001-41c9-91C7-6E89D9C111AD}
  DEFINE_GUID(CMDSETID_EzMDI, 
    0x489ee5bf, 0xf001, 0x41c9, 0x91, 0xc7, 0x6e, 0x89, 0xd9, 0xc1, 0x11, 0xad);

  // Emacs editor emulation
  // {9A95F3AF-F86A-4aa2-80E6-012BF65DBBC3}
  DEFINE_GUID(guidEmacsCommandGroup, 
    0x9a95f3af, 0xf86a, 0x4aa2, 0x80, 0xe6, 0x1, 0x2b, 0xf6, 0x5d, 0xbb, 0xc3);

  // Brief editor emulation
  // {7A500D8A-8258-46c3-8965-6AC53ED6B4E7}
  DEFINE_GUID(guidBriefCommandGroup, 
    0x7a500d8a, 0x8258, 0x46c3, 0x89, 0x65, 0x6a, 0xc5, 0x3e, 0xd6, 0xb4, 0xe7);

  // {501822E1-B5AF-11d0-B4DC-00A0C91506EF}
  DEFINE_GUID(guidDataCmdId,
    0x501822e1, 0xB5AF, 0x11D0, 0xB4, 0xDC, 0x00, 0xA0, 0xC9, 0x15, 0x06, 0xEF);

  // {4614107F-217D-4bbf-9DFE-B9E165C65572}
  DEFINE_GUID(guidVSData, 
    0x4614107F, 0x217D, 0x4bbf, 0x9D, 0xFE, 0xB9, 0xE1, 0x65, 0xC6, 0x55, 0x72);

  //{732abe75-cd80-11d0-a2db-00aa00a3efff}
  DEFINE_GUID(CMDSETID_DaVinciDataToolsCommandSet,
    0x732abe75, 0xcd80, 0x11d0, 0xa2, 0xdb, 0x00, 0xaa, 0x00, 0xa3, 0xef, 0xff);

  // Guid for Extension Manager Package
  //{E7576C05-1874-450c-9E98-CF3A0897A069}
  DEFINE_GUID(guidExtensionManagerPkg,
    0xe7576c05, 0x1874, 0x450c, 0x9e, 0x98, 0xcf, 0x3a, 0x08, 0x97, 0xa0, 0x69);

  // Guid for Extension Manager Icon
  DEFINE_GUID(guidExtensionManagerIcon,
    0x12ffec2c, 0x2df7, 0x49eb, 0xa2, 0x92, 0x05, 0xc4, 0xa5, 0xf9, 0xc3, 0x54);

  // Guid for puslish web Icon
  // {69DE971C-8BB7-4032-9E7D-3D7C115A6329}
  DEFINE_GUID(guidPublishWebIcon, 
    0x69de971c, 0x8bb7, 0x4032, 0x9e, 0x7d, 0x3d, 0x7c, 0x11, 0x5a, 0x63, 0x29);

  // WM_APPCOMMAND handling
  // The active ole command targets will receive CMDSETID_WMAppCommand:cmdID, where
  // cmdID is one of APPCOMMAND_****, defined in winuser.h for _WIN32_WINNT >= 0x0500
  // (use common\inc\wmappcmd.h to have this commands defined for all target platforms)
  // If command is not handled, we will look in the registry for the mapped command:
  // HKLM\<appid hive>\WMAppCommand
  //    val <AppCmdID> = {<guidCmdSet>}:<cmdID>

  // {12F1A339-02B9-46e6-BDAF-1071F76056BF}
  DEFINE_GUID(CMDSETID_WMAppCommand,
        0x12f1a339, 0x02b9, 0x46e6, 0xbd, 0xaf, 0x10, 0x71, 0xf7, 0x60, 0x56, 0xbf);

  DEFINE_GUID(CLSID_VsCommunityPackage,
        0x490508dd, 0x32ce, 0x45e8, 0x80, 0x8c, 0xfa, 0xeb, 0xf4, 0x68, 0xb1, 0x86);

  // {0x462b036f,0x7349,0x4835,{0x9e,0x21,0xbe,0xc6,0x0e,0x98,0x9b,0x9c}}
  // {462B036F-7349-4835-9E21-BEC60E989B9C}
  DEFINE_GUID(guidVDTFlavorCmdSet,
      0x462b036f, 0x7349, 0x4835, 0x9e, 0x21, 0xbe, 0xc6, 0x0e, 0x98, 0x9b, 0x9c);
      
  // Reference Manager Providers command set guid
  // {8206e3a8-09d6-4f97-985f-7b980b672a97}
  DEFINE_GUID(guidReferenceManagerProvidersPackageCmdSet, 
      0xa8e30682, 0xd609, 0x974f, 0x98, 0x5f, 0x7b, 0x98, 0x0b, 0x67, 0x2a, 0x97);

  // -------------------------------------
  // Class View Selection UIContext guids.
  // -------------------------------------

  // {48903663-A165-4e4b-867D-90622B1E6E9C}
  DEFINE_GUID(guidClassViewSelectionNamespace, 
      0x48903663, 0xa165, 0x4e4b, 0x86, 0x7d, 0x90, 0x62, 0x2b, 0x1e, 0x6e, 0x9c);

  // {010FA539-D664-45c2-BD28-7C36F2AAA816}
  DEFINE_GUID(guidClassViewMultiSelectionNamespaces, 
      0x10fa539, 0xd664, 0x45c2, 0xbd, 0x28, 0x7c, 0x36, 0xf2, 0xaa, 0xa8, 0x16);

  // {C5F62498-4EEE-423b-B12E-EA6FB3217215}
  DEFINE_GUID(guidClassViewSelectionClass, 
      0xc5f62498, 0x4eee, 0x423b, 0xb1, 0x2e, 0xea, 0x6f, 0xb3, 0x21, 0x72, 0x15);

  // {767AF915-7282-49da-806E-9AC9614E78FC}
  DEFINE_GUID(guidClassViewMultiSelectionClasses, 
      0x767af915, 0x7282, 0x49da, 0x80, 0x6e, 0x9a, 0xc9, 0x61, 0x4e, 0x78, 0xfc);

  // {AF5D60D7-9F6C-4824-98E6-074E258790F8}
  DEFINE_GUID(guidClassViewSelectionMember, 
      0xaf5d60d7, 0x9f6c, 0x4824, 0x98, 0xe6, 0x07, 0x4e, 0x25, 0x87, 0x90, 0xf8);

  // {C46D1701-7623-4bb2-A7E2-FB059D2B33E9}
  DEFINE_GUID(guidClassViewMultiSelectionMembers, 
      0xc46d1701, 0x7623, 0x4bb2, 0xa7, 0xe2, 0xfb, 0x5, 0x9d, 0x2b, 0x33, 0xe9);

  // {5EE0E92B-13BD-491b-9518-40B2936F5E21}
  DEFINE_GUID(guidClassViewMultiSelectionMixed, 
      0x5ee0e92b, 0x13bd, 0x491b, 0x95, 0x18, 0x40, 0xb2, 0x93, 0x6f, 0x5e, 0x21);

  // {57817069-31B7-4d3a-8B2C-8195EB7D216F}
  DEFINE_GUID(guidClassViewSelectionPhysicalContainer, 
      0x57817069, 0x31b7, 0x4d3a, 0x8b, 0x2c, 0x81, 0x95, 0xeb, 0x7d, 0x21, 0x6f);

  // {D584640A-388C-4e66-BB81-80969620D404}
  DEFINE_GUID(guidClassViewMultiSelectionPhysicalContainers, 
      0xd584640a, 0x388c, 0x4e66, 0xbb, 0x81, 0x80, 0x96, 0x96, 0x20, 0xd4, 0x4);

  // {F19997FD-8C6E-4972-88BC-063181D4E88C}
  DEFINE_GUID(guidClassViewSelectionHierarchy, 
      0xf19997fd, 0x8c6e, 0x4972, 0x88, 0xbc, 0x6, 0x31, 0x81, 0xd4, 0xe8, 0x8c);

  // {2D502DA9-629C-4293-8B14-1312F4EBD89A}
  DEFINE_GUID(guidClassViewSelectionMemberHierarchy, 
      0x2d502da9, 0x629c, 0x4293, 0x8b, 0x14, 0x13, 0x12, 0xf4, 0xeb, 0xd8, 0x9a);
      
  // {2D502DA9-629C-4293-8B14-1312F4EBD89A}
  DEFINE_GUID(guidClassViewSelectionSupportsClassDesigner,
      0xc53a8676, 0x1a8f, 0x4673, 0x91, 0x47, 0x09, 0xa3, 0xe7, 0xd5, 0x6c, 0xda);
      

  // -----------------------------------------
  // End Class View Selection UIContext guids.
  // -----------------------------------------


  // {84571F7F-1A90-41E0-9781-2610297FB09D}
  DEFINE_GUID(guidDExploreApplicationObject,
      0x84571F7F, 0x1A90, 0x41E0, 0x97, 0x81, 0x26, 0x10, 0x29, 0x7F, 0xB0, 0x9D);

  // {8D8529D3-625D-4496-8354-3DAD630ECC1B}
  DEFINE_GUID(guid_VSDesignerPackage, 
    0x8D8529D3, 0x625D, 0x4496, 0x83, 0x54, 0x3D, 0xAD, 0x63, 0x0E, 0xCC, 0x1B);

  // {640F725F-1B2D-4831-A9FD-874847682010}
  DEFINE_GUID(guidServerExpIcon,
      0x640F725F, 0x1B2D, 0x4831, 0xA9, 0xFD, 0x87, 0x48, 0x47, 0x68, 0x20, 0x10);

///////////////////////////////////////////////
//
// VS Enterprise guids
//
///////////////////////////////////////////////

  // VS Enterprise Cmd UIContext guid
  // {07CA8E98-FF14-4e5e-9C4D-959C081B5E47}
  DEFINE_GUID(guidTeamProjectCmdUIContext, 
      0x07CA8E98, 0xFF14, 0x4e5e, 0x9C, 0x4D, 0x95, 0x9C, 0x08, 0x1B, 0x5E, 0x47);

  // VS Enterprise Shared Commands guid
  // {3F5A3E02-AF62-4c13-8D8A-A568ECAE238B}
  DEFINE_GUID(guidTeamExplorerSharedCmdSet, 
      0x3F5A3E02, 0xAF62, 0x4c13, 0x8D, 0x8A, 0xA5, 0x68, 0xEC, 0xAE, 0x23, 0x8B);


  DEFINE_GUID (guidRefactorIcon, 
      0x5d7e7f67, 0xa63f, 0x46ee, 0x84, 0xf1, 0x99, 0xb, 0x2c, 0xab, 0x23, 0xf3);

  // {B3285A19-6471-4150-AE05-18253F95FBCC}
  DEFINE_GUID (guidGoToTypeDef, 
      0xb3285a19, 0x6471, 0x4150, 0xae, 0x5, 0x18, 0x25, 0x3f, 0x95, 0xfb, 0xcc);

  // {E6EA7925-0FE6-4867-84EA-8BA78B7FDBEE}
  DEFINE_GUID(guidGenerateMethodIcon, 
      0xe6ea7925, 0xfe6, 0x4867, 0x84, 0xea, 0x8b, 0xa7, 0x8b, 0x7f, 0xdb, 0xee);
  // {50AA77AC-6BB4-42A8-A4A2-F4CD407E80A8}
  DEFINE_GUID (guidToggleCompletionMode, 
      0x50AA77AC, 0x6BB4, 0x42A8, 0xa4, 0xa2, 0xf4, 0xcd, 0x40, 0x7e, 0x80, 0xa8);

  // Server Explorer menu group guid
  // {74D21310-2AEE-11d1-8BFB-00A0C90F26F7}
  DEFINE_GUID(guid_SE_MenuGroup, 
      0x74d21310, 0x2aee, 0x11d1, 0x8b, 0xfb, 0x0, 0xa0, 0xc9, 0xf, 0x26, 0xf7);

  // Server Explorer command ID guid
  // {74D21311-2AEE-11d1-8BFB-00A0C90F26F7}
  DEFINE_GUID(guid_SE_CommandID, 
    0x74d21311, 0x2aee, 0x11d1, 0x8b, 0xfb, 0x0, 0xa0, 0xc9, 0xf, 0x26, 0xf7);

  // UI Context GUID to enable the Tools->Connect To Server command
  // {9BF70368-F5F7-4ddf-8CD2-FB27FBE0BD9C}
  DEFINE_GUID(guidAppidSupportsConnectToServer, 
    0x9bf70368, 0xf5f7, 0x4ddf, 0x8c, 0xd2, 0xfb, 0x27, 0xfb, 0xe0, 0xbd, 0x9c);

  // SQL Server Object Explorer command ID guid
  // {03f46784-2f90-4122-91ec-72ff9e11d9a3}
  DEFINE_GUID(guidSqlObjectExplorerCmdSet,
	  0x03f46784, 0x2f90, 0x4122, 0x91, 0xec, 0x72, 0xff, 0x9e, 0x11, 0xd9, 0xa3);

///////////////////////////////////////////////
//
// Editor Shim CLSIDs from the Editor Shim Package (defined at Microsoft.VisualStudio.Editor.dll)
//
///////////////////////////////////////////////

  // CLSID for VS10 Platform Factory
  DEFINE_GUID(CLSID_PlatformFactory, 
    0x2491432F, 0x3A10, 0x4884, 0xB6, 0x28, 0x57, 0x4D, 0x57, 0xF4, 0x1E, 0x9B);

  // CLSID for VsDocDataAdapter
  DEFINE_GUID(CLSID_VsDocDataAdapter, 
    0x169F2886, 0x6566, 0x432e, 0xA9, 0x3D, 0x55, 0x88, 0xBD, 0x58, 0x32, 0x29);

  // CLSID for VsTextBufferCoordinatorAdapter
  DEFINE_GUID(CLSID_VsTextBufferCoordinatorAdapter, 
    0x5FCEEA4C, 0xD49F, 0x4acd, 0xB8, 0x16, 0x13, 0x0A, 0x5D, 0xCD, 0x4C, 0x54);

  // CLSID for VsHiddenTextManagerAdapter
  DEFINE_GUID(CLSID_VsHiddenTextManagerAdapter, 
    0x85115CFE, 0x3F29, 0x4e52, 0xAE, 0x98, 0x6F, 0xE6, 0x25, 0x73, 0xD1, 0x1C);

  // GUID to get the IVxTextBuffer from the IVsUserData
  DEFINE_GUID(GUID_VxTextBuffer,
      0xbe120c41, 0xd969, 0x42a4, 0xa4, 0xdd, 0x91, 0x26, 0x65, 0xa5, 0xbf, 0x13);

#else //!DEFINE_GUID
  // Guid if using Office provided icons
  #define guidOfficeIcon          { 0xd309f794, 0x903f, 0x11d0, { 0x9e, 0xfc, 0x00, 0xa0, 0xc9, 0x11, 0x00, 0x4f } }
  // Guid for shell icons
  #define guidVsShellIcons        { 0x9cd93c42, 0xceef, 0x45ab, { 0xb1, 0xb5, 0x60, 0x40, 0x88, 0xc, 0x95, 0x43 } } 
  // Guid for the duplicate accelerator keys
  #define guidKeyDupe             { 0xf17bdae0, 0xa16d, 0x11d0, { 0x9f, 0x4,  0x0,  0xa0, 0xc9, 0x11, 0x0,  0x4f } }
  // Guid for Shell's group and menu ids
  #define guidSHLMainMenu         { 0xd309f791, 0x903f, 0x11d0, { 0x9e, 0xfc, 0x00, 0xa0, 0xc9, 0x11, 0x00, 0x4f } }
  // Guid for ClassView menu ids
  #define guidClassViewMenu       { 0xfb61dcfe, 0xc9cb, 0x4964, { 0x84, 0x26, 0xc2, 0xd3, 0x83, 0x34, 0x07, 0x8c } }
  // Guid for CommonIDE package
  #define guidCommonIDEPackage    { 0x6E87CFAD, 0x6C05, 0x4adf, { 0x9C, 0xD7, 0x3B, 0x79, 0x43, 0x87, 0x5B, 0x7C } }
  // Guid for CommonIDE package commands
  #define guidCommonIDEPackageCmd { 0x6767e06b, 0x5789, 0x472b, { 0x8e, 0xd7, 0x1f, 0x20, 0x73, 0x71, 0x6e, 0x8c } }
  // Guid for Standard Shell Commands (97 set)
  #define CMDSETID_StandardCommandSet97  { 0x5efc7975, 0x14bc, 0x11cf, { 0x9b, 0x2b, 0x00, 0xaa, 0x00, 0x57, 0x38, 0x19 } }
  // Guid for Standard Shell Commands (2k set)
  #define CMDSETID_StandardCommandSet2K {0x1496A755, 0x94DE, 0x11D0, {0x8C, 0x3F, 0x00, 0xC0, 0x4F, 0xC2, 0xAA, 0xE2}}
  // Guid for Standard Shell Commands (Dev10 set)
  #define CMDSETID_StandardCommandSet10 {0x5dd0bb59, 0x7076, 0x4c59, {0x88, 0xd3, 0xde, 0x36, 0x93, 0x1f, 0x63, 0xf0}}
  // Guid for Standard Shell Commands (Dev11 set)
  #define CMDSETID_StandardCommandSet11 {0xd63db1f0, 0x404e, 0x4b21, {0x96, 0x48, 0xca, 0x8d, 0x99, 0x24, 0x5e, 0xc3}}
  // Guid for Standard Shell Commands (Dev12 set)
  #define CMDSETID_StandardCommandSet12 {0x2A8866DC, 0x7BDE, 0x4dc8, {0xA3, 0x60, 0xA6, 0x06, 0x79, 0x53, 0x43, 0x84}};
  // Guid for Standard Shell Commands (Dev14 set)
  #define CMDSETID_StandardCommandSet14 {0x4c7763bf, 0x5faf, 0x4264, {0xa3, 0x66, 0xb7, 0xe1, 0xf2, 0x7b, 0xa9, 0x58}};
  // Guid for Standard Shell Commands (Dev15 set)
  #define CMDSETID_StandardCommandSet15 {0x712c6c80, 0x883b, 0x4aad, {0xb4, 0x30, 0xbb, 0xca, 0x52, 0x56, 0xfa, 0x9d}};
  // Guid for the EzMDI file list menu private command set
  #define CMDSETID_EzMDI {0x489ee5bf, 0xf001, 0x41c9, {0x91, 0xc7, 0x6e, 0x89, 0xd9, 0xc1, 0x11, 0xad}}
  // Guid for the Emacs editor emulation command group
  // {9A95F3AF-F86A-4aa2-80E6-012BF65DBBC3}
  #define guidEmacsCommandGroup {0x9a95f3af, 0xf86a, 0x4aa2,{ 0x80, 0xe6, 0x1, 0x2b, 0xf6, 0x5d, 0xbb, 0xc3}}
  // Guid for the Brief editor emulation command group
  // {7A500D8A-8258-46c3-8965-6AC53ED6B4E7}
  #define guidBriefCommandGroup {0x7a500d8a, 0x8258, 0x46c3,{ 0x89, 0x65, 0x6a, 0xc5, 0x3e, 0xd6, 0xb4, 0xe7}}
  // Guid for DocOutline package commands
  #define guidDocOutlinePkg { 0x21af45b0, 0xffa5, 0x11d0, { 0xb6, 0x3f, 0x00, 0xa0, 0xc9, 0x22, 0xe8, 0x51 } }
  // Guid for TaskList package commands
  #define CLSID_VsTaskListPackage     { 0x4A9B7E50, 0xAA16, 0x11d0, { 0xA8, 0xC5, 0x00, 0xA0, 0xC9, 0x21, 0xA4, 0xD2 } }
  // Guid for find/replace bitmaps...
  #define guidFindIcon  { 0x740EEC10, 0x1A5D, 0x11D1, { 0xA0, 0x30, 0x00, 0xA0, 0xC9, 0x11, 0xE8, 0xE9} }
  // Guid for unified find bitmaps...
  #define guidUFindIcon  { 0xD7BECFE4, 0x1C1A, 0x4D32, { 0x8E, 0xD8, 0xF7, 0xDA, 0x4F, 0x89, 0x7E, 0x7B} }
  // Guid for Bookmark window bitmaps...
  #define guidBookmarkIcon  { 0x7637b0ae, 0x7d52, 0x40a1, { 0x90, 0xba, 0x51, 0x94, 0x50, 0x57, 0x97, 0x9d } }
  // Guid for Tool window goto bitmaps... {65ED2DB5-9942-4664-BA7C-CBE2B79AE7A8}
  #define guidToolWindowGotoButtons { 0x65ed2db5, 0x9942, 0x4664, { 0xba, 0x7c, 0xcb, 0xe2, 0xb7, 0x9a, 0xe7, 0xa8 } }
  // Guid for debugger bitmaps
  #define guidDebuggerIcon { 0xb7afe65e, 0x3a96, 0x11d1, { 0xb0, 0x68, 0x0, 0xc0, 0x4f, 0xb6, 0x6f, 0xa0} }
  // Guid for object browser buttons
  #define guidObjectBrowserButtons  { 0x5f810e80, 0x33ad, 0x11d1, { 0xa7, 0x96, 0x0, 0xa0, 0xc9, 0x11, 0x10, 0xc3 } }
  // Guid for Call Browser buttons {F858DE97-54BF-4929-A039-62396ACACD8E}
  #define guidCallBrowserButtons  { 0xf858de97, 0x54bf, 0x4929, { 0xa0, 0x39, 0x62, 0x39, 0x6a, 0xca, 0xcd, 0x8e } }
  // Guid for Call Hierarchy buttons {90C70706-ECC3-4d97-B80C-2CED9E7CC7EB}
  #define guidCallHierarchyButtons  { 0x90c70706, 0xecc3, 0x4d97, { 0xb8, 0xc, 0x2c, 0xed, 0x9e, 0x7c, 0xc7, 0xeb } }
  // Guid for Code Definition View buttons {88892CCC-3565-4e34-BFF3-B9B0997FC195}
  #define guidCodeDefViewButtons  { 0x88892ccc, 0x3565, 0x4e34, { 0xbf, 0xf3, 0xb9, 0xb0, 0x99, 0x7f, 0xc1, 0x95 } }
  // UIContext guid specifying that we're not in View Source mode
  #define guidNotViewSourceMode  {0x7174c6a0, 0xb93d, 0x11d1, {0x9f, 0xf4, 0x0, 0xa0, 0xc9, 0x11, 0x0, 0x4f} }
  // Guid for text editor bitmaps...
  #define guidTextEditorIcon  { 0xc40a5a10, 0x3eeb, 0x11d3, { 0xaf, 0xe5, 0x0, 0x10, 0x5a, 0x99, 0x91, 0xef } }
  // Guid for error / warning buttons...
  #define guidErrorIcon  { 0x7e65bae7, 0xd6fc, 0x4c65, { 0x89, 0x2d, 0xe2, 0xc9, 0xdc, 0xaa, 0xdd, 0xae } }
  #define guidSharedMenuGroup { 0x234a7fc1, 0xcfe9, 0x4335, { 0x9e, 0x82, 0x6, 0x1f, 0x86, 0xe4, 0x02, 0xc1 } }
  // guid for build cmd icons
  #define guidBuildCmdIcons { 0x952691c5, 0x34d6, 0x462b, {0xac, 0x56, 0x9a, 0xb0, 0x97, 0x70, 0xa3, 0x0d}}
  // {501822E1-B5AF-11d0-B4DC-00A0C91506EF} Guid for Data project commands
  #define guidDataCmdId {0x501822e1, 0xb5af, 0x11d0, {0xb4, 0xdc, 0x00, 0xa0, 0xc9, 0x15, 0x06, 0xef}}
  // {4614107F-217D-4bbf-9DFE-B9E165C65572}
  #define guidVSData    {0x4614107F, 0x217D, 0x4bbf, {0x9D, 0xFE, 0xB9, 0xE1, 0x65, 0xC6, 0x55, 0x72}}      
  //{732abe75-cd80-11d0-a2db-00aa00a3efff}
  #define CMDSETID_DaVinciDataToolsCommandSet   {0x732abe75, 0xcd80, 0x11d0, {0xa2, 0xdb, 0x00, 0xaa, 0x00, 0xa3, 0xef, 0xff} }
  // {12F1A339-02B9-46e6-BDAF-1071F76056BF}
  #define CMDSETID_WMAppCommand { 0x12f1a339, 0x02b9, 0x46e6, { 0xbd, 0xaf, 0x10, 0x71, 0xf7, 0x60, 0x56, 0xbf } }
  #define CLSID_VsCommunityPackage { 0x490508dd, 0x32ce, 0x45e8, { 0x80, 0x8c, 0xfa, 0xeb, 0xf4, 0x68, 0xb1, 0x86 } }
  // Guid for Yukon projects commands
  // {462B036F-7349-4835-9E21-BEC60E989B9C}
  #define guidVDTFlavorCmdSet { 0x462b036f, 0x7349, 0x4835, {0x9e, 0x21, 0xbe, 0xc6, 0x0e, 0x98, 0x9b, 0x9c } }
  // Error List toolwindow icon
  #define guidErrorListIcon  { 0xbffbae07, 0x4ff7, 0x45da, { 0x88, 0x3e, 0x82, 0xcc, 0xdb, 0x85, 0xf1, 0xf8 } }
  // Accessibility check button {EEF04648-250A-4360-8C2F-43CC063E198D}
  #define guidAccessibilityIcon { 0xeef04648, 0x250a, 0x4360, { 0x8c, 0x2f, 0x43, 0xcc, 0x6, 0x3e, 0x19, 0x8d } }
  // Server Explorer menu group {74D21310-2AEE-11d1-8BFB-00A0C90F26F7}
  #define guid_SE_MenuGroup { 0x74d21310, 0x2aee, 0x11d1, { 0x8b, 0xfb, 0x0, 0xa0, 0xc9, 0xf, 0x26, 0xf7 } }
  // Server Explorer command ID guid {74D21311-2AEE-11d1-8BFB-00A0C90F26F7}
  #define guid_SE_CommandID { 0x74d21311, 0x2aee, 0x11d1, { 0x8b, 0xfb, 0x0, 0xa0, 0xc9, 0xf, 0x26, 0xf7 } }
  // SQL Server Object Explorer command ID guid {03f46784-2f90-4122-91ec-72ff9e11d9a3}
  #define guidSqlObjectExplorerCmdSet { 0x03f46784, 0x2f90, 0x4122, {0x91, 0xec, 0x72, 0xff, 0x9e, 0x11, 0xd9, 0xa3 } }
  // UI Context GUID to enable the Tools->Connect To Server command {9BF70368-F5F7-4ddf-8CD2-FB27FBE0BD9C}
  #define guidAppidSupportsConnectToServer { 0x9bf70368, 0xf5f7, 0x4ddf, { 0x8c, 0xd2, 0xfb, 0x27, 0xfb, 0xe0, 0xbd, 0x9c } }
  //Guid for Extension Manager Package
  #define guidExtensionManagerPkg { 0xe7576c05, 0x1874, 0x450c, { 0x9e, 0x98, 0xcf, 0x3a, 0x08, 0x97, 0xa0, 0x69 } }
  // Guid for Extension Manager Icon
  #define guidExtensionManagerIcon { 0x12ffec2c, 0x2df7, 0x49eb, { 0xa2, 0x92, 0x05, 0xc4, 0xa5, 0xf9, 0xc3, 0x54 } }
  // Guid for puslish web Icon
  #define guidPublishWebIcon { 0x69de971c, 0x8bb7, 0x4032, { 0x9e, 0x7d, 0x3d, 0x7c, 0x11, 0x5a, 0x63, 0x29 } }

  // -------------------------------------
  // Class View Selection UIContext guids.
  // -------------------------------------

  // {48903663-A165-4e4b-867D-90622B1E6E9C}
  #define guidClassViewSelectionNamespace { 0x48903663, 0xa165, 0x4e4b, {0x86, 0x7d, 0x90, 0x62, 0x2b, 0x1e, 0x6e, 0x9c } }

  // {010FA539-D664-45c2-BD28-7C36F2AAA816}
  #define guidClassViewMultiSelectionNamespaces { 0x10fa539, 0xd664, 0x45c2, {0xbd, 0x28, 0x7c, 0x36, 0xf2, 0xaa, 0xa8, 0x16 } }

  // {C5F62498-4EEE-423b-B12E-EA6FB3217215}
  #define guidClassViewSelectionClass { 0xc5f62498, 0x4eee, 0x423b, {0xb1, 0x2e, 0xea, 0x6f, 0xb3, 0x21, 0x72, 0x15 } }

  // {767AF915-7282-49da-806E-9AC9614E78FC}
  #define guidClassViewMultiSelectionClasses { 0x767af915, 0x7282, 0x49da, {0x80, 0x6e, 0x9a, 0xc9, 0x61, 0x4e, 0x78, 0xfc } }

  // {AF5D60D7-9F6C-4824-98E6-074E258790F8}
  #define guidClassViewSelectionMember { 0xaf5d60d7, 0x9f6c, 0x4824, {0x98, 0xe6, 0x07, 0x4e, 0x25, 0x87, 0x90, 0xf8 } }

  // {C46D1701-7623-4bb2-A7E2-FB059D2B33E9}
  #define guidClassViewMultiSelectionMembers { 0xc46d1701, 0x7623, 0x4bb2, {0xa7, 0xe2, 0xfb, 0x5, 0x9d, 0x2b, 0x33, 0xe9 } } 

  // {5EE0E92B-13BD-491b-9518-40B2936F5E21}
  #define guidClassViewMultiSelectionMixed { 0x5ee0e92b, 0x13bd, 0x491b, {0x95, 0x18, 0x40, 0xb2, 0x93, 0x6f, 0x5e, 0x21 } } 

  // {57817069-31B7-4d3a-8B2C-8195EB7D216F}
  #define guidClassViewSelectionPhysicalContainer { 0x57817069, 0x31b7, 0x4d3a, {0x8b, 0x2c, 0x81, 0x95, 0xeb, 0x7d, 0x21, 0x6f } } 

  // {D584640A-388C-4e66-BB81-80969620D404}
  #define guidClassViewMultiSelectionPhysicalContainers { 0xd584640a, 0x388c, 0x4e66, {0xbb, 0x81, 0x80, 0x96, 0x96, 0x20, 0xd4, 0x4 } }

  // {F19997FD-8C6E-4972-88BC-063181D4E88C}
  #define guidClassViewSelectionHierarchy { 0xf19997fd, 0x8c6e, 0x4972, {0x88, 0xbc, 0x6, 0x31, 0x81, 0xd4, 0xe8, 0x8c } } 

  // {2D502DA9-629C-4293-8B14-1312F4EBD89A}
  #define guidClassViewSelectionMemberHierarchy { 0x2d502da9, 0x629c, 0x4293, {0x8b, 0x14, 0x13, 0x12, 0xf4, 0xeb, 0xd8, 0x9a } } 
  
  // {C53A8676-1A8F-4673-9147-09A3E7D56CDA}
  #define guidClassViewSelectionSupportsClassDesigner { 0xc53a8676, 0x1a8f, 0x4673, { 0x91, 0x47, 0x9, 0xa3, 0xe7, 0xd5, 0x6c, 0xda } }

  // -----------------------------------------
  // End Class View Selection UIContext guids.
  // -----------------------------------------

  // {84571F7F-1A90-41E0-9781-2610297FB09D}
  #define guidDExploreApplicationObject { 0x84571F7F, 0x1A90, 0x41E0, {0x97, 0x81, 0x26, 0x10, 0x29, 0x7F, 0xB0, 0x9D } }

  // {8D8529D3-625D-4496-8354-3DAD630ECC1B}
  #define guid_VSDesignerPackage { 0x8D8529D3, 0x625D, 0x4496, { 0x83, 0x54, 0x3D, 0xAD, 0x63, 0x0E, 0xCC, 0x1B } }

  // {640F725F-1B2D-4831-A9FD-874847682010}
  #define guidServerExpIcon { 0x640F725F, 0x1B2D, 0x4831, {0xA9, 0xFD, 0x87, 0x48, 0x47, 0x68, 0x20, 0x10 } }

  // Guid for the View Definition Icon
  // {5D82E0FE-9301-4B2B-8872-9E037943A681}
  #define guidViewDefinitionIcon  { 0x5d82e0fe, 0x9301, 0x4b2b, { 0x88, 0x72, 0x9e, 0x3, 0x79, 0x43, 0xa6, 0x81 } };

///////////////////////////////////////////////
//
// VS Enterprise guids
//
///////////////////////////////////////////////

  // VS Enterprise Cmd UIContext guid
  // {07CA8E98-FF14-4e5e-9C4D-959C081B5E47}
  #define guidTeamProjectCmdUIContext { 0x07CA8E98, 0xFF14, 0x4e5e, {0x9C, 0x4D, 0x95, 0x9C, 0x08, 0x1B, 0x5E, 0x47 } }

  // VS Enterprise Shared Commands guid
  // {3F5A3E02-AF62-4c13-8D8A-A568ECAE238B}
  #define guidTeamExplorerSharedCmdSet { 0x3F5A3E02, 0xAF62, 0x4c13, {0x8D, 0x8A, 0xA5, 0x68, 0xEC, 0xAE, 0x23, 0x8B } }


  #define guidRefactorIcon         { 0x5d7e7f67, 0xa63f, 0x46ee, { 0x84, 0xf1, 0x99, 0xb, 0x2c, 0xab, 0x23, 0xf3 } }
  #define guidGoToTypeDef          { 0xb3285a19, 0x6471, 0x4150, { 0xae, 0x5, 0x18, 0x25, 0x3f, 0x95, 0xfb, 0xcc } }
  #define guidGenerateMethodIcon   { 0xe6ea7925, 0x0fe6, 0x4867, { 0x84, 0xea, 0x8b, 0xa7, 0x8b, 0x7f, 0xdb, 0xee } }
  #define guidToggleCompletionMode { 0x50AA77AC, 0x6BB4, 0x42A8, { 0xa4, 0xa2, 0xf4, 0xcd, 0x40, 0x7e, 0x80, 0xa8 } };

  #define GUID_TextEditorFactory {0x8b382828, 0x6202, 0x11d1, {0x88, 0x70, 0x0, 0x0, 0xf8, 0x75, 0x79, 0xd2}}

///////////////////////////////////////////////
//
// Editor Shim CLSIDs from the Editor Shim Package (defined at Microsoft.VisualStudio.Editor.dll)
//
///////////////////////////////////////////////

  // CLSID for VS10 Editor Factory
  #define CLSID_VS10TextEditorFactory     {0xdf25faa1, 0xe891, 0x49f6, {0x98, 0x23, 0x72, 0x63, 0x4a, 0x02, 0xa4, 0x05} }

  // CLSID for VS10 Editor Factory with encoding
  #define CLSID_VS10TextEditorFactoryWithEncoding    {0xC6BE297E, 0xC907, 0x4F43, {0x91, 0x20, 0x05, 0x3C, 0x19, 0x2E, 0xF5, 0x1E} }

  // CLSID for VS10 Platform Factory
  #define CLSID_PlatformFactory           {0x2491432F, 0x3A10, 0x4884, {0xB6, 0x28, 0x57, 0x4D, 0x57, 0xF4, 0x1E, 0x9B} }

  // CLSID for VsDocDataAdapter
  #define CLSID_VsDocDataAdapter          {0x169F2886, 0x6566, 0x432e, {0xA9, 0x3D, 0x55, 0x88, 0xBD, 0x58, 0x32, 0x29} }

  // CLSID for VsTextBufferCoordinatorAdapter
  #define CLSID_VsTextBufferCoordinatorAdapter    {0x5FCEEA4C, 0xD49F, 0x4acd, {0xB8, 0x16, 0x13, 0x0A, 0x5D, 0xCD, 0x4C, 0x54} }

  // CLSID for VsHiddenTextManagerAdapter
  #define CLSID_VsHiddenTextManagerAdapter    {0x85115CFE, 0x3F29, 0x4e52, {0xAE, 0x98, 0x6F, 0xE6, 0x25, 0x73, 0xD1, 0x1C} }

  // GUID to get the IVxTextBuffer from the IVsUserData
  #define GUID_VxTextBuffer		  {0xbe120c41, 0xd969, 0x42a4, {0xa4, 0xdd, 0x91, 0x26, 0x65, 0xa5, 0xbf, 0x13} }

#endif //!DEFINE_GUID

#ifdef __CTC__
// *** UIContext Guids for use by CTC parser only...
#define UICONTEXT_SolutionBuilding      { 0xadfc4e60, 0x397, 0x11d1, { 0x9f, 0x4e, 0x0, 0xa0, 0xc9, 0x11, 0x0, 0x4f } }
#define UICONTEXT_Debugging         { 0xadfc4e61, 0x397, 0x11d1, { 0x9f, 0x4e, 0x0, 0xa0, 0xc9, 0x11, 0x0, 0x4f } }
#define UICONTEXT_FullScreenMode        { 0xadfc4e62, 0x397, 0x11d1, { 0x9f, 0x4e, 0x0, 0xa0, 0xc9, 0x11, 0x0, 0x4f } }
#define UICONTEXT_DesignMode            { 0xadfc4e63, 0x397, 0x11d1, { 0x9f, 0x4e, 0x0, 0xa0, 0xc9, 0x11, 0x0, 0x4f } }
#define UICONTEXT_NoSolution            { 0xadfc4e64, 0x397, 0x11d1, { 0x9f, 0x4e, 0x0, 0xa0, 0xc9, 0x11, 0x0, 0x4f } }
#define UICONTEXT_SolutionExists                { 0xf1536ef8, 0x92ec, 0x443c, { 0x9e, 0xd7, 0xfd, 0xad, 0xf1, 0x50, 0xda, 0x82 } }
#define UICONTEXT_EmptySolution         { 0xadfc4e65, 0x397, 0x11d1, { 0x9f, 0x4e, 0x0, 0xa0, 0xc9, 0x11, 0x0, 0x4f } }
#define UICONTEXT_SolutionHasSingleProject  { 0xadfc4e66, 0x397, 0x11d1, { 0x9f, 0x4e, 0x0, 0xa0, 0xc9, 0x11, 0x0, 0x4f } }
#define UICONTEXT_SolutionHasMultipleProjects   { 0x93694fa0, 0x397, 0x11d1, { 0x9f, 0x4e, 0x0, 0xa0, 0xc9, 0x11, 0x0, 0x4f } }
#define UICONTEXT_CodeWindow        { 0x8fe2df1d, 0xe0da, 0x4ebe, { 0x9d, 0x5c, 0x41, 0x5d, 0x40, 0xe4, 0x87, 0xb5 } }
#define UICONTEXT_NotBuildingAndNotDebugging   { 0x48ea4a80, 0xf14e, 0x4107, { 0x88, 0xfa, 0x8d, 0x0, 0x16, 0xf3, 0xb, 0x9c } } //VS 2005 Bug #35207 add new cmdUIGuid
#define UICONTEXT_SolutionExistsAndNotBuildingAndNotDebugging   { 0xd0e4deec, 0x1b53, 0x4cda, { 0x85, 0x59, 0xd4, 0x54, 0x58, 0x3a, 0xd2, 0x3b } }
#define UICONTEXT_SolutionHasAppContainerProject   { 0x7CAC4AE1, 0x2E6B, 0x4B02, { 0xA9, 0x1C, 0x71, 0x61, 0x1E, 0x86, 0xF2, 0x73 } }
// from vsshell110.h
#define UICONTEXT_OsWindows8OrHigher { 0x67CFF80C, 0x0863, 0x4202, { 0xA4, 0xE4, 0xCE, 0x80, 0xFD, 0xF8, 0x50, 0x6E } }
#endif //__CTC__

#define guidVSStd97                 CMDSETID_StandardCommandSet97
#define CLSID_StandardCommandSet97  CMDSETID_StandardCommandSet97

#define guidVSStd2K                 CMDSETID_StandardCommandSet2K
#define guidVSStd10                 CMDSETID_StandardCommandSet10
#define CLSID_StandardCommandSet10  CMDSETID_StandardCommandSet10
#define guidVSStd11                 CMDSETID_StandardCommandSet11
#define CLSID_StandardCommandSet11  CMDSETID_StandardCommandSet11
#define guidVSStd12                 CMDSETID_StandardCommandSet12
#define CLSID_StandardCommandSet12  CMDSETID_StandardCommandSet12
#define guidEzMDI                   CMDSETID_EzMDI
#define CLSID_StandardCommandSet2K  CMDSETID_StandardCommandSet2K
#define CLSID_CTextViewCommandGroup CMDSETID_StandardCommandSet2K
#define CLSID_TCG                   CMDSETID_StandardCommandSet2K
#define CLSID_ECG                   CMDSETID_StandardCommandSet2K
#define guidDavDataCmdId            CMDSETID_DaVinciDataToolsCommandSet
#define guidVSStd14                 CMDSETID_StandardCommandSet14
#define CLSID_StandardCommandSet14  CMDSETID_StandardCommandSet14
#define guidVSStd15                 CMDSETID_StandardCommandSet15
#define CLSID_StandardCommandSet15  CMDSETID_StandardCommandSet15

// Standard editor guid.
#define guidStdEditor   {0x9ADF33D0, 0x8AAD, 0x11d0, {0xB6, 0x06, 0x00, 0xA0, 0xC9, 0x22, 0xE8, 0x51} }




// Standard editor shorthand macros for a more compact and manageable table
#define guidStdEd           guidStdEditor
#define guidStdEdCmd        guidStdEditor:IDG_VS_EDITOR_CMDS
#define guidStdEdBmkFld     guidStdEditor:IDG_VS_EDITOR_BOOKMARK_FOLDER_CMDS   // Bookmark commands restricted to a folder
#define guidStdEdBmkDoc     guidStdEditor:IDG_VS_EDITOR_BOOKMARK_DOCUMENT_CMDS // Bookmark commands restricted to a document
#define guidStdEdBmkAllDocs guidStdEditor:IDG_VS_EDITOR_BOOKMARK_ALLDOCS_CMDS  // Bookmark commands that operate on all files
#define guidStdEdBmkTskLst  guidStdEditor:IDG_VS_EDITOR_BOOKMARK_TASKLIST_CMDS // Tasklist shortcut commands
#define guidStdEdAdv        guidStdEditor:IDG_VS_EDITOR_ADVANCED_CMDS
#define guidStdEdOut        guidStdEditor:IDG_VS_EDITOR_OUTLINING_CMDS
#define guidStdLang         guidStdEditor:IDG_VS_EDITOR_LANGUAGE_INFO
#define guidStdEdIntel      guidStdEditor:IDG_VS_EDITOR_INTELLISENSE_CMDS
#endif //!NOGUIDS

//////////////////////////////////////////////////////////////////////////////
//
// Toolbar Identifiers, created by Visual Studio Shell
//
//////////////////////////////////////////////////////////////////////////////
#define IDM_VS_TOOL_MAINMENU          0x0000
#define IDM_VS_TOOL_STANDARD          0x0001
#define IDM_VS_TOOL_WINDOWUI          0x0002
#define IDM_VS_TOOL_PROJWIN       0x0003
#define IDM_VS_TOOL_DEBUGGER	      0x0006
#define IDM_VS_TOOL_OBJECT_BROWSER_GO 0x0007
#define IDM_VS_TOOL_CLASSVIEW_GO      0x0008
#define IDM_VS_TOOL_OBJSEARCH         0x0009
#define IDM_VS_TOOL_FINDALLREF        0x000a
#define IDM_VS_TOOL_OPENWINDOWS       0x000b
#define IDM_VS_TOOL_VIEWBAR       0x000c
#define IDM_VS_TOOL_BUILD             0x000d
#define IDM_VS_TOOL_TEXTEDITOR        0x000e
#define IDM_VS_TOOL_OBJBROWSER        0x000f
#define IDM_VS_TOOL_CLASSVIEW         0x0010
#define IDM_VS_TOOL_PROPERTIES        0x0011
#define IDM_VS_TOOL_DATA              0x0012
#define IDM_VS_TOOL_SCHEMA            0x0013
#define IDM_VS_TOOL_OUTPUTWINDOW      0x0014
#define IDM_VS_TOOL_FINDRESULTS1      0x0015
#define IDM_VS_TOOL_FINDRESULTS2      0x0016
#define IDM_VS_TOOL_UNIFIEDFIND       0x0017
//UNUSED 0x0018
#define IDM_VS_TOOL_BOOKMARKWIND      0x0019
#define IDM_VS_TOOL_CALLBROWSER1      0x001a
#define IDM_VS_TOOL_CALLBROWSER2      0x001b
#define IDM_VS_TOOL_CALLBROWSER3      0x001c
#define IDM_VS_TOOL_CALLBROWSER4      0x001d
#define IDM_VS_TOOL_CALLBROWSER5      0x001e
#define IDM_VS_TOOL_CALLBROWSER6      0x001f
#define IDM_VS_TOOL_CALLBROWSER7      0x0020
#define IDM_VS_TOOL_CALLBROWSER8      0x0021
#define IDM_VS_TOOL_CALLBROWSER9      0x0022
#define IDM_VS_TOOL_CALLBROWSER10     0x0023
#define IDM_VS_TOOL_CALLBROWSER11     0x0024
#define IDM_VS_TOOL_CALLBROWSER12     0x0025
#define IDM_VS_TOOL_CALLBROWSER13     0x0026
#define IDM_VS_TOOL_CALLBROWSER14     0x0027
#define IDM_VS_TOOL_CALLBROWSER15     0x0028
#define IDM_VS_TOOL_CALLBROWSER16     0x0029
#define IDM_VS_TOOL_TASKLIST          0x002a
#define IDM_VS_TOOL_USERTASKS         0x002b
#define IDM_VS_TOOL_ERRORLIST         0x002c
#define IDM_VS_TOOL_SNIPPETMENUS      0x002D

#define IDM_VS_CALLBROWSER_TYPE_POPUP   0x0030

//////////////////////////////////////////////////////////////////////////////
// Toolbar ID for customize mode only 
//
// **** NOTE ****  DO NOT add any menu or toolbar that has an ID greater than
//                 IDM_VS_TOOL_ADDCOMMAND, otherwise you WILL break customize
//                 mode. IDM_VS_TOOL_UDEFINED is a very special toolbar.
//                 Do not use or place it anywhere - used by shell only.
//////////////////////////////////////////////////////////////////////////////
#define IDM_VS_TOOL_UNDEFINED         0xEDFF
#define IDM_VS_TOOL_ADDCOMMAND        0xEE00

//////////////////////////////////////////////////////////////////////////////
//
// Menu Identifiers, created by Visual Studio Shell
//
//////////////////////////////////////////////////////////////////////////////
#define IDM_VS_MENU_FILE              0x0080
#define IDM_VS_MENU_EDIT              0x0081
#define IDM_VS_MENU_VIEW              0x0082
#define IDM_VS_MENU_PROJECT           0x0083
#define IDM_VS_MENU_TOOLS             0x0085
#define IDM_VS_MENU_WINDOW            0x0086
#define IDM_VS_MENU_ADDINS            0x0087
#define IDM_VS_MENU_HELP              0x0088
#define IDM_VS_MENU_DEBUG             0x0089
#define IDM_VS_MENU_FORMAT            0x008A
#define IDM_VS_MENU_ALLMACROS         0x008B
#define IDM_VS_MENU_BUILD             0x008C
#define IDM_VS_MENU_CONTEXTMENUS      0x008D
#define IDG_VS_MENU_CONTEXTMENUS      0x008E
#define IDM_VS_MENU_REFACTORING       0x008f
#define IDM_VS_MENU_COMMUNITY         0x0090

///////////////////////////////////////////////
//
// Editor menu groups
//
///////////////////////////////////////////////
#define IDG_VS_EDITOR_CMDS                      0x3E8A

#define IDG_VS_EDITOR_BOOKMARK_FOLDER_CMDS      0x3EB0
#define IDG_VS_EDITOR_BOOKMARK_DOCUMENT_CMDS    0x3EB1
#define IDG_VS_EDITOR_BOOKMARK_ALLDOCS_CMDS     0x3EB2
#define IDG_VS_EDITOR_BOOKMARK_TASKLIST_CMDS    0x3EB3

#define IDG_VS_EDITOR_ADVANCED_CMDS             0x3E8F
#define IDG_VS_EDITOR_OUTLINING_CMDS            0x3E90
#define IDG_VS_EDITOR_LANGUAGE_INFO             0x3E93
#define IDG_VS_EDITOR_INTELLISENSE_CMDS         0x3E94
#define IDG_TOOLS_SNIPPETS                      0x3E95  //actually on the tools menu defined above

#define IDM_VS_EDITOR_BOOKMARK_MENU             0x3E9E
#define IDM_VS_EDITOR_ADVANCED_MENU             0x3EA0
#define IDM_VS_EDITOR_OUTLINING_MENU            0x3EA1
#define IDM_VS_EDITOR_INTELLISENSE_MENU         0x3EA2
#define IDM_VS_EDITOR_FIND_MENU                 0x3EA3
#define IDM_VS_EDITOR_PASTE_MENU                0x3EA4
#define IDM_VS_EDITOR_GOTO_MENU                 0x3EA5

//////////////////////////////////////////////////////////////////////////////
//
// Group Identifiers, created by Visual Studio Shell
//
//////////////////////////////////////////////////////////////////////////////

// Main Menu Bar Groups
#define IDG_VS_MM_FILEEDITVIEW        0x0101    // File/Edit/View menus go here
#define IDG_VS_MM_PROJECT             0x0102    // Project menu go here
#define IDG_VS_MM_BUILDDEBUGRUN       0x0103    // Build/Debug/Run menus go here
#define IDG_VS_MM_TOOLSADDINS         0x0104    // Tools/Addins menu goes here
#define IDG_VS_MM_WINDOWHELP          0x0105    // Window/Help menus go here
#define IDG_VS_MM_FULLSCREENBAR       0x0106    // Full Screen group
//VS 2005 Bug #58088   Put refactorings on top-level menu 
#define IDG_VS_MM_REFACTORING         0x0107    // Refactorings go here
#define IDG_VS_MM_REFACTORING_JS      0x0108    // VS 2005 bug #275998

// All Macros Groups
#define IDG_VS_MM_MACROS              0x010A

// File Menu Groups
#define IDG_VS_FILE_NEW_PROJ_CSCD     0x010E
#define IDG_VS_FILE_ITEM              0x010F
#define IDG_VS_FILE_FILE              0x0110
#define IDG_VS_FILE_ADD               0x0111
#define IDG_VS_FILE_SAVE              0x0112
#define IDG_VS_FILE_RENAME            0x0113
#define IDG_VS_FILE_PRINT             0x0114
#define IDG_VS_FILE_ACCOUNTSETTINGS   0x0711
#define IDG_VS_FILE_MRU               0x0115
#define IDG_VS_FILE_EXIT              0x0116
#define IDG_VS_FILE_DELETE            0x0117
#define IDG_VS_FILE_SOLUTION          0x0118
#define IDG_VS_FILE_NEW_CASCADE       0x0119
#define IDG_VS_FILE_OPENP_CASCADE     0x011A
#define IDG_VS_FILE_OPENF_CASCADE     0x011B
#define IDG_VS_FILE_ADD_PROJECT_NEW   0x011C
#define IDG_VS_FILE_ADD_PROJECT_EXI   0x011D
#define IDG_VS_FILE_FMRU_CASCADE      0x011E
#define IDG_VS_FILE_PMRU_CASCADE      0x011F
#define IDG_VS_FILE_BROWSER           0x0120
#define IDG_VS_FILE_MOVE              0x0121
#define IDG_VS_FILE_MOVE_CASCADE      0x0122
#define IDG_VS_FILE_MOVE_PICKER       0x0123
#define IDG_VS_FILE_MISC              0x0124
#define IDG_VS_FILE_MISC_CASCADE      0x0125
#define IDG_VS_FILE_MAKE_EXE          0x0126
#define IDG_VS_FILE_OPENSCC_CASCADE   0x0127

// Edit Menu Groups
#define IDG_VS_EDIT_OBJECTS           0x0128
#define IDG_VS_EDIT_UNDOREDO          0x0129
#define IDG_VS_EDIT_CUTCOPY           0x012A
#define IDG_VS_EDIT_SELECT            0x012B
#define IDG_VS_EDIT_FIND              0x012C
#define IDG_VS_EDIT_GOTO              0x012D
#define IDG_VS_EDIT_COMMANDWELL       0x012E
#define IDG_VS_EDIT_PASTE             0x012F

// View Menu Groups
#define IDG_VS_VIEW_BROWSER           0x0130
#define IDG_VS_VIEW_PROPPAGES         0x0131
#define IDG_VS_VIEW_TOOLBARS          0x0132
#define IDG_VS_VIEW_FORMCODE          0x0133
#define IDG_VS_VIEW_DEFINEVIEWS       0x0134
#define IDG_VS_VIEW_WINDOWS           0x0135
#define IDG_VS_VIEW_ARCH_WINDOWS      0x0720
#define IDG_VS_VIEW_ORG_WINDOWS       0x0721
#define IDG_VS_VIEW_CODEBROWSENAV_WINDOWS 0x0722
#define IDG_VS_VIEW_DEV_WINDOWS       0x0723
#define IDG_VS_WNDO_FINDRESULTS       0x0724
#define IDG_VS_VIEW_REFRESH           0x0136
#define IDG_VS_VIEW_NAVIGATE          0x0137
#define IDG_VS_VIEW_SYMBOLNAVIGATE    0x0138
#define IDG_VS_VIEW_SMALLNAVIGATE     0x0139
#define IDG_VS_VIEW_OBJBRWSR          0x013A
#define IDG_VS_VIEW_LINKS             0x013B
#define IDG_VS_VIEW_COMMANDWELL       0x013C
#define IDG_VS_VIEW_SYMBOLNAVIGATE_JS 0x013D // VS 2005 bug #303148

// Project Menu Groups
#define IDG_VS_PROJ_ADD               0x0140
#define IDG_VS_PROJ_OPTIONS           0x0141
#define IDG_VS_PROJ_REFERENCE         0x0142
#define IDG_VS_PROJ_FOLDER            0x0143
#define IDG_VS_PROJ_UNLOADRELOAD      0x0144
#define IDG_VS_PROJ_ADDCODE           0x0145
#define IDG_VS_PROJ_PROJECT           0x0146
#define IDG_VS_PROJ_ADDREMOVE         0x0147
#define IDG_VS_PROJ_WEB1              0x0148
#define IDG_VS_PROJ_WEB2              0x0149
#define IDG_VS_PROJ_TOOLBAR1          0x014A
#define IDG_VS_PROJ_TOOLBAR2          0x014B
#define IDG_VS_PROJ_MISCADD           0x014C
#define IDG_VS_PROJ_SETTINGS          0x014D
#define IDG_VS_PROJ_ADMIN             0x014E

// Run Menu Groups
#define IDG_VS_RUN_START          0x0150
#define IDG_VS_DBG_STEP           0x0151
#define IDG_VS_DBG_WATCH          0x0152
#define IDG_VS_DBG_BRKPTS         0x0153
#define IDG_VS_DBG_STATEMENT          0x0154
#define IDG_VS_DBG_ATTACH         0x0155
#define IDG_VS_DBG_TBBRKPTS       0x0156
#define IDG_VS_DBG_DBGWINDOWS         0x0157   // this actually resides on the debugger toolbar

//Tools->External Tools Groups
#define IDG_VS_TOOLS_EXT_CUST         0x0158
#define IDG_VS_TOOLS_EXT_TOOLS        0x0159

// Tools Menu Groups
#define IDG_VS_TOOLS_OPTIONS          0x015A
#define IDG_VS_TOOLS_OTHER2       0x015B
#define IDG_VS_TOOLS_OBJSUBSET        0x015C
#define IDG_VS_TOOLS_EXTENSIBILITY    0x015F

// Addins Menu Groups
#define IDG_VS_ADDIN_BUILTIN          0x015D
#define IDG_VS_ADDIN_MANAGER          0x015E

// Window Menu Groups
#define IDG_VS_WINDOW_NEW             0x0160
#define IDG_VS_WINDOW_ARRANGE         0x0161
#define IDG_VS_WINDOW_LIST            0x0162
#define IDG_VS_WINDOW_NAVIGATION      0x0163
#define IDG_VS_WINDOW_LAYOUT          0x0164
#define IDG_VS_WINDOW_LAYOUT_LIST     0x0165

// Help Menu Groups
#define IDG_VS_HELP_SUPPORT       0x016A
#define IDG_VS_HELP_ABOUT         0x016B
#define IDG_VS_HELP_ACCESSIBILITY 0x016D
//#define IDG_VS_HELP_SAMPLES       0x016C


// Standard Toolbar Groups
#define IDG_VS_TOOLSB_NEWADD          0x0170
#define IDG_VS_TOOLSB_SAVEOPEN        0x0171
#define IDG_VS_TOOLSB_CUTCOPY         0x0172
#define IDG_VS_TOOLSB_UNDOREDO        0x0173
#define IDG_VS_TOOLSB_RUNBUILD        0x0174
#define IDG_VS_TOOLSB_WINDOWS         0x0175  // don't use
#define IDG_VS_TOOLSB_GAUGE       0x0176
#define IDG_VS_TOOLSB_SEARCH          0x0177
#define IDG_VS_TOOLSB_NEWWINDOWS      0x0178
#define IDG_VS_TOOLSB_NAVIGATE        0x0179
#define IDG_VS_FINDTAB            0x017D
#define IDG_VS_REPLACETAB         0x017E

// Window UI Toolbar Groups
#define IDG_VS_WINDOWUI_LOADSAVE      0x017A

// Open Windows Toolbar Groups
#define IDG_VS_OPENWIN_WINDOWS        0x017B

// View Bar Toolbar Groups
#define IDG_VS_VIEWBAR_VIEWS          0x017C

// Watch context menu groups
#define IDG_VS_WATCH_EDITADDDEL       0x0180
//#define IDG_VS_WATCH_COLLAPSE       0x0181
#define IDG_VS_WATCH_PROCDEFN         0x0182
#define IDG_VS_WATCH_STARTEND         0x0183

// Thread context menu groups
#define IDG_VS_THREAD_SUSPENDRESUME   0x0184

// Hexadecimal group
#define IDG_VS_DEBUG_DISPLAYRADIX     0x0185

// Treegrid context menu
#define IDG_VS_TREEGRID           0x0186

// Immediate context menu groups
#define IDG_VS_IMMD_OBPROCDEFN        0x0188

// Docking / Hide Pane Group
#define IDG_VS_DOCKCLOSE          0x0189
#define IDG_VS_DOCKHIDE           0x0190
#define IDG_VS_DOCUMENTDOCKHIDE   0x0192

// Thread context menu groups
#define IDG_VS_CALLST_RUNTOCURSOR     0x0191
// 0x0192 is used above in IDG_VS_DOCUMENTDOCKHIDE

// MenuDesigner Context Menu Groups
#define IDG_VS_MNUDES_CUTCOPY         0x0195
#define IDG_VS_MNUDES_INSERT          0x0196
#define IDG_VS_MNUDES_EDITNAMES       0x0197
#define IDG_VS_MNUDES_VIEWCODE        0x0198
#define IDG_VS_MNUDES_PROPERTIES      0x0199

#define IDG_VS_MNUDES_UNDOREDO        0x019A

// Window Menu Cascade groups
#define IDG_VS_WNDO_OTRWNDWS0         0x019E
#define IDG_VS_WNDO_OTRWNDWS1         0x019F
#define IDG_VS_WNDO_OTRWNDWS2         0x01A0
#define IDG_VS_WNDO_OTRWNDWS3         0x01A1
#define IDG_VS_WNDO_OTRWNDWS4         0x01A2
#define IDG_VS_WNDO_OTRWNDWS5         0x01A3
#define IDG_VS_WNDO_OTRWNDWS6         0x01A4
#define IDG_VS_WNDO_WINDOWS1          0x01A5
#define IDG_VS_WNDO_WINDOWS2          0x01A6
#define IDG_VS_WNDO_DBGWINDOWS        IDG_VS_WNDO_WINDOWS1
#define IDG_VS_WNDO_INTERACTIVEWNDWS  0x01A7

// OLE Verbs Menu Cascade groups
#define IDG_VS_EDIT_OLEVERBS          0x01A8

// PropBrs Context menu groups
#define IDG_VS_PROPBRS_MISC       0x01AA

// Output Window Pane Context menu groups
#define IDG_VS_RESULTSLISTCOPY        0x01AC
#define IDG_VS_RESULTSLISTCLEAR       0x01AD
#define IDG_VS_RESULTSLISTGOTO        0x01AE
#define IDG_VS_RESULTSLISTOUTLINE     0x01AF

// New Toolbox Context Menu groups   
#define IDG_VS_TOOLBOX_ACTIONS          0x01B0
#define IDG_VS_TOOLBOX_ITEM             0x01B1
#define IDG_VS_TOOLBOX_TAB              0x01B2
#define IDG_VS_TOOLBOX_MOVE             0x01B3
#define IDG_VS_TOOLBOX_VIEW             0x01B4

// Miscellaneous Files project context menu groups
#define IDG_VS_MISCFILES_PROJ         0x01B8

// Miscellaneous Files project item context menu groups
#define IDG_VS_MISCFILES_PROJITEM     0x01BA

// Solution Items project item context menu groups
#define IDG_VS_SOLNITEMS_PROJ         0x01BC
#define IDG_VS_SOLNITEMS_PROJITEM     0x01BD

// Stub (unloaded/placeholder) project context menu groups
#define IDG_VS_STUB_PROJECT           0x01BE

// Code Window context menu groups
#define IDG_VS_CODEWIN_TEXTEDIT     0x01C0
//#define unused menu ID            0x01C1
#define IDG_VS_CODEWIN_DEBUG_WATCH  0x01C2
#define IDG_VS_CODEWIN_DEBUG_STEP   0x01C3
#define IDG_VS_CODEWIN_MARKER       0x01C4
#define IDG_VS_CODEWIN_OPENURL      0x01C5
#define IDG_VS_CODEWIN_SHORTCUT     0x01C6

#define IDG_VS_CODEWIN_INTELLISENSE     0x02B0
#define IDG_VS_CODEWIN_NAVIGATETOLOCATION         0x02B1
#define IDG_VS_CODEWIN_NAVIGATETOFILE 0x02B2
#define IDG_VS_CODEWIN_OUTLINING    0x02B3
#define IDG_VS_CODEWIN_CTXT_OUTLINING 0x02B4
#define IDG_VS_CODEWIN_REFACTORING  0x02b5
// 0x02B6 used below in IDG_VS_FINDRESULTS1_STOPFIND
// 0x02B7 used below in IDG_VS_FINDRESULTS2_STOPFIND
#define IDG_VS_CODEWIN_REFACTORING_JS 0x02b8 // VS 2005 bug #275998
#define IDG_VS_CODEWIN_LANGUAGE     0x02D0
#define IDG_VS_CODEWIN_ADVANCED		0x02D1

// Snippet flyout menu and groups
#define IDG_VS_CODEWIN_SNIPPETS     0x02D2
#define IDM_VS_CODEWIN_SNIPPET_ROOT 0x02D3
#define IDG_VS_CODEWIN_SNIPPET_ROOT 0x02D4


// Annotation flyout menu and groups
#define IDG_VS_CODEWIN_ANNOTATION      0x02D5
#define IDM_VS_CODEWIN_ANNOTATION_ROOT 0x02D6
#define IDG_VS_CODEWIN_ANNOTATION_ROOT 0x02D7

// IntelliTrace step menu groups.
#define IDG_VS_CODEWIN_INTELLITRACE_STEP 0x02D8

// Task List context menu groups
#define IDG_VS_TASKLIST           0x01C7
#define IDG_VS_ERRORLIST          0x01CB

// cascading Task list menu groups
#define IDG_VS_TASKLIST_SORT              0x01C8
#define IDG_VS_TASKLIST_NEXTPREV_ERR	  0x01C9
#define IDG_VS_TASKLIST_CLIENT            0x01CA
// 0x01CB used above (IDG_VS_ERRORLIST)
#define IDG_VS_ERRORLIST_CLIENT           0x01CC
#define IDG_VS_ERRORLIST_NEXTPREV_ERR     0x01CD
#define IDG_VS_TASKLIST_GROUPS            0x01CE
#define IDG_VS_TASKLIST_COLUMNS           0x01CF

#define IDG_VS_TASKLIST_SORT_COLUMN       0x01D0

// Tasklist toolbar provider list group
#define IDG_VS_TASKLIST_PROVIDERLIST    0x01D1

// Build toolbar group
#define IDG_VS_BUILDBAR                 0x01D2

// User Tasks toolbar group
#define IDG_VS_USERTASKS_EDIT           0x01D3

// Error List toolbar group
#define IDG_VS_ERRORLIST_ERRORGROUP     0x01D4

// Project Window Toolbar group
#define IDG_VS_PROJ_TOOLBAR3            0x01D5
#define IDG_VS_PROJ_TOOLBAR4            0x01D6
#define IDG_VS_PROJ_TOOLBAR5            0x01D7

// More error list toolbar groups -- see IDG_VS_ERRORLIST_ERRORGROUP above
#define IDG_VS_ERRORLIST_WARNINGGROUP          0x01D8
#define IDG_VS_ERRORLIST_MESSAGEGROUP          0x01D9
#define IDG_VS_ERRORLIST_FILTERLISTTOGROUP     0x01DA
#define IDG_VS_ERRORLIST_FILTERCATEGORIESGROUP 0x01DB
#define IDG_VS_ERRORLIST_BUILDGROUP            0x01DC
#define IDG_VS_ERRORLIST_CLEARFILTERGROUP      0x01DD

// Solution Node ctxt menu groups     
#define IDG_VS_SOLNNODE_CTXT_TOP    0x01E0
#define IDG_VS_SOLNNODE_CTXT_BOTTOM 0x01E1

// Project Window Default group
#define IDG_VS_PROJWIN_NODE_CTXT_TOP    0x01E2
#define IDG_VS_PROJWIN_NODE_CTXT_BOTTOM 0x01E3
#define IDG_VS_PROJWIN_ITEM_CTXT_TOP    0x01E4
#define IDG_VS_PROJWIN_ITEM_CTXT_BOTTOM 0x01E5

// Document Window Default groups   
#define IDG_VS_DOCWINDOW_CTXT_TOP   0x01E6
#define IDG_VS_DOCWINDOW_CTXT_BOTTOM    0x01E7

// Tool Window Default groups
#define IDG_VS_TOOLWINDOW_CTXT_TOP  0x01E8
#define IDG_VS_TOOLWINDOW_CTXT_BOTTOM   0x01E9

// EZ MDI groups
#define IDG_VS_EZ_TILE                  0x01EA
#define IDG_VS_EZ_CANCEL                0x01EB
#define IDG_VS_EZ_DOCWINDOWOPS          0x01EC
#define IDG_VS_EZ_DOCWINDOWPATHOPS      0x01ED

// Pinned Tabs
#define IDG_VS_PINNEDTABS               0x01EE

// Debugger Group
#define IDG_VS_TOOL_DEBUGGER            0x0200

// Shell defined context menu groups
#define IDG_VS_CTXT_MULTIPROJ_BUILD     0x0201
#define IDG_VS_CTXT_PROJECT_ADD         0x0202
#define IDG_VS_CTXT_PROJECT_ADD_ITEMS   0x0203
#define IDG_VS_CTXT_PROJECT_DEBUG   0x0204
#define IDG_VS_CTXT_PROJECT_START   0x0205
#define IDG_VS_CTXT_PROJECT_BUILD   0x0206
#define IDG_VS_CTXT_PROJECT_TRANSFER    0x0207
#define IDG_VS_CTXT_ITEM_VIEWOBJECT 0x0208
#define IDG_VS_CTXT_ITEM_OPEN       0x0209
#define IDG_VS_CTXT_ITEM_TRANSFER   0x020A
#define IDG_VS_CTXT_ITEM_VIEWBROWSER    0x020B
#define IDG_VS_CTXT_SAVE        0x020C
#define IDG_VS_CTXT_ITEM_PRINT      0x020D
#define IDG_VS_CTXT_ITEM_PROPERTIES 0x020E
#define IDG_VS_CTXT_SCC         0x020F 
#define IDG_VS_CTXT_ITEM_RENAME     0x0210
#define IDG_VS_CTXT_PROJECT_RENAME  0x0211
#define IDG_VS_CTXT_SOLUTION_RENAME 0x0212
#define IDG_VS_CTXT_ITEM_SAVE       IDG_VS_CTXT_SAVE
#define IDG_VS_CTXT_PROJECT_SAVE    0x0213
#define IDG_VS_CTXT_PROJECT_PROPERTIES  0x0214
#define IDG_VS_CTXT_SOLUTION_PROPERTIES 0x0215
#define IDG_VS_CTXT_ITEM_SCC        IDG_VS_CTXT_SCC
#define IDG_VS_CTXT_PROJECT_SCC     0x0216
#define IDG_VS_CTXT_SOLUTION_SCC    0x0217

#define IDG_VS_CTXT_SOLUTION_SAVE   0x0218
#define IDG_VS_CTXT_SOLUTION_BUILD  0x0219
#define IDG_VS_UNUSED           0x021A  // unused group for hidden cmds
#define IDG_VS_CTXT_SOLUTION_START      0x021B
#define IDG_VS_CTXT_SOLUTION_TRANSFER   0x021C
#define IDG_VS_CTXT_SOLUTION_ADD_PROJ   0x021D
#define IDG_VS_CTXT_SOLUTION_ADD_ITEM   0x021E
#define IDG_VS_CTXT_SOLUTION_DEBUG      0x021F

#define IDG_VS_CTXT_DOCOUTLINE      0x0220
#define IDG_VS_CTXT_NOCOMMANDS          0x0221

#define IDG_VS_TOOLS_CMDLINE        0x0222
#define IDG_VS_TOOLS_SNIPPETS       IDG_TOOLS_SNIPPETS
#define IDG_VS_CTXT_CMDWIN_MARK     0x0223

#define IDG_VS_CTXT_AUTOHIDE        0x0224

//External tools context menu groups
#define IDG_VS_EXTTOOLS_CURARGS         0x0225
#define IDG_VS_EXTTOOLS_PROJARGS        0x0226
#define IDG_VS_EXTTOOLS_SLNARGS         0x0227
#define IDG_VS_EXTTOOLS_CURDIRS         0x0228
#define IDG_VS_EXTTOOLS_PROJDIRS        0x0229
#define IDG_VS_EXTTOOLS_SLNDIRS         0x022A
#define IDG_VS_EXTTOOLS_TARGETARGS      0x022B
#define IDG_VS_EXTTOOLS_EDITORARGS      0x022C
#define IDG_VS_EXTTOOLS_TARGETDIRS      0x022D

#define IDG_VS_CTXT_ITEM_VIEW           0x022E
#define IDG_VS_CTXT_DELETE              0x022F
#define IDG_VS_CTXT_FOLDER_TRANSFER     0x0230
#define IDG_VS_CTXT_MULTISELECT_TRANSFER  0x0231
#define IDG_VS_CTXT_PROJECT_DEPS        0x0232
#define IDG_VS_CTXT_SOLUTION_ADD        0x0233
#define IDG_VS_CTXT_PROJECT_CONFIG      0x0234

// New File/Add New Item Open button drop-down menu
#define IDG_VS_OPENDROPDOWN_MENU        0x0235

// Unhide group on solution context menu
#define IDG_VS_CTXT_SOLUTION_UNHIDE     0x0236

// Context menu group for editing a project file
#define IDG_VS_CTXT_PROJECT_EDITFILE  0x0237

// Object search menu groups
#define IDG_VS_OBJSEARCH_NAVIGATE     0x0238
#define IDG_VS_OBJSEARCH_EDIT         0x0239
//#define IDG_VS_OBJSEARCH_SORTING      0x0268

// Context menu group for unloading/reloading a project
#define IDG_VS_CTXT_PROJECT_UNLOADRELOAD 0x023A

// Classview menu groups

#define IDG_VS_CLASSVIEW_BASE_DERIVED_GRP   0x023B
#define IDG_VS_CLASSVIEW_DISPLAY2           0x023C
#define IDG_VS_CLASSVIEW_MEMACCESSGRP       0x023D
#define IDG_VS_CLASSVIEW_SEARCH2            0x023E

#define IDG_VS_CLASSVIEW_MEMGRP       0x023F
#define IDG_VS_CLASSVIEW_FOLDERS      0x0240  // Used in toolbar
#define IDG_VS_CLASSVIEW_FOLDERS2     0x0241  // Used in context menu
#define IDG_VS_CLASSVIEW_DISPLAY      0x0242
#define IDG_VS_CLASSVIEW_SEARCH       0x0243
#define IDG_VS_CLASSVIEW_EDIT         0x0244
#define IDG_VS_CLASSVIEW_NAVIGATION   0x0245
#define IDG_VS_CLASSVIEW_SHOWINFO     0x0247
#define IDG_VS_CLASSVIEW_PROJADD      0x0248
#define IDG_VS_CLASSVIEW_ITEMADD      0x0249
#define IDG_VS_CLASSVIEW_GROUPING     0x024a
#define IDG_VS_CLASSVIEW_PROJWIZARDS  0x024b
#define IDG_VS_CLASSVIEW_ITEMWIZARDS  0x024c
#define IDG_VS_CLASSVIEW_PROJADDITEMS 0x024d
#define IDG_VS_CLASSVIEW_FOLDERS_EDIT 0x024e



// Regular Expression Context menu groups
#define IDG_VS_FINDREGEXNORM0         0x024f
#define IDG_VS_FINDREGEXNORM1         0x0250
#define IDG_VS_FINDREGEXHELP          0x0251
#define IDG_VS_REPLACEREGEXNORM       0x0252
#define IDG_VS_REPLACEREGEXHELP       0x0253
#define IDG_VS_FINDWILDNORM       0x0254
#define IDG_VS_FINDWILDHELP       0x0255
#define IDG_VS_REPLACEWILDNORM        0x0256
#define IDG_VS_REPLACEWILDHELP        0x0257
#define IDG_VS_FINDREGEXNORM2         0x0258
#define IDG_VS_FINDREGEXNORM3         0x0259
#define IDG_VS_FINDREGEXNORM4         0x5300

#define IDG_VS_EXTTOOLS_BINARGS       0x025A
#define IDG_VS_EXTTOOLS_BINDIRS       0x025B

// Solution Folders context menu groups
#define IDG_VS_CTXT_SLNFLDR_ADD_PROJ  0x0261
#define IDG_VS_CTXT_SLNFLDR_ADD_ITEM  0x0262
#define IDG_VS_CTXT_SLNFLDR_BUILD     0x0263
#define IDG_VS_CTXT_SLNFLDR_ADD       0x0264

#define IDG_VS_CTXT_SOLUTION_EXPLORE  0x0265
#define IDG_VS_CTXT_PROJECT_EXPLORE   0x0266
#define IDG_VS_CTXT_FOLDER_EXPLORE    0x0267

// object search (find symbol results) context menu group
#define IDG_VS_OBJSEARCH_SORTING      0x0268
#define IDG_VS_OBJSEARCH_NAVIGATE2    0x0269 // Used in toolbar
#define IDG_VS_OBJSEARCH_BROWSE       0x026a
#define IDG_VS_OBJSEARCH_COMMON       0x026b

// Find All References toolbar groups
#define IDG_VS_FINDALLREF_COMMON           0x026c
#define IDG_VS_FINDALLREF_PRESETGROUPINGS  0x026d
#define IDG_VS_FINDALLREF_LOCKWINGROUP     0x026e
#define IDG_VS_FINDALLREF_PRESERVED        0x026f

// Object Browser menu groups
#define IDG_VS_OBJBROWSER_SUBSETS     0x0270
#define IDG_VS_OBJBROWSER_DISPLAY     0x0271
#define IDG_VS_OBJBROWSER_DISPLAY2    0x0272
#define IDG_VS_OBJBROWSER_SEARCH      0x0273
#define IDG_VS_OBJBROWSER_SEARCH2     0x0274
#define IDG_VS_OBJBROWSER_NAVIGATION  0x0275
#define IDG_VS_OBJBROWSER_EDIT        0x0276
#define IDG_VS_OBJBROWSER_OBJGRP      0x0277
#define IDG_VS_OBJBROWSER_MEMGRP      0x0278
#define IDG_VS_OBJBROWSER_GROUPINGS   0x0279
#define IDG_VS_OBJBROWSER_VIEWGRP     0x027A
#define IDG_VS_OBJBROWSER_MEMACCESSGRP  0x027B
#define IDG_VS_OBJBROWSER_BROWSERSETTINGSBTN 0x027C
#define IDG_VS_OBJBROWSER_BASE_DERIVED_GRP  0x027D
#define IDG_VS_OBJBROWSER_BROWSERSETTINGS 0x027E
#define IDG_VS_OBJBROWSER_MEMBERSETTINGS  0x027F


// Build Menu groups
#define IDG_VS_BUILD_SOLUTION         0x0280
#define IDG_VS_BUILD_SELECTION        0x0281
#define IDG_VS_BUILD_MISC             0x0282
#define IDG_VS_BUILD_CANCEL           0x0283
#define IDG_VS_BUILD_CASCADE          0x0284
#define IDG_VS_REBUILD_CASCADE        0x0285
#define IDG_VS_CLEAN_CASCADE          0x0286
#define IDG_VS_DEPLOY_CASCADE         0x0287
#define IDG_VS_BUILD_PROJPICKER       0x0288
#define IDG_VS_REBUILD_PROJPICKER     0x0289
#define IDG_VS_PGO_SELECTION          0x028A
// 0x028B is used below (IDG_VS_PGO_BUILD_CASCADE_RUN)
#define IDG_VS_BUILD_COMPILE          0x028C
#define IDG_VS_CLEAN_PROJPICKER       0x0290
#define IDG_VS_DEPLOY_PROJPICKER      0x0291

#define IDG_VS_CTXT_CMDWIN_CUTCOPY    0x0292

// Output Window menu groups
#define IDG_VS_OUTPUTWINDOW_SELECT    0x0293
#define IDG_VS_OUTPUTWINDOW_GOTO      0x0294
#define IDG_VS_OUTPUTWINDOW_NEXTPREV  0x0295
#define IDG_VS_OUTPUTWINDOW_CLEAR     0x0296
#define IDG_VS_OUTPUTWINDOW_WORDWRAP  0x029F

// Find Results 1 menu groups
#define IDG_VS_FINDRESULTS1_GOTO      0x0297
#define IDG_VS_FINDRESULTS1_NEXTPREV  0x0298
#define IDG_VS_FINDRESULTS1_CLEAR     0x0299
#define IDG_VS_FINDRESULTS1_STOPFIND  0x02B6

// Find Results 2 menu groups
#define IDG_VS_FINDRESULTS2_GOTO      0x029A
#define IDG_VS_FINDRESULTS2_NEXTPREV  0x029B
#define IDG_VS_FINDRESULTS2_CLEAR     0x029C
#define IDG_VS_FINDRESULTS2_STOPFIND  0x02B7

#define IDG_VS_PROJONLY_CASCADE       0x029D
#define IDG_VS_PGO_BUILD_CASCADE_BUILD 0x029E
#define IDG_VS_PGO_BUILD_CASCADE_RUN   0x028B

// 0x029F used above (IDG_VS_OUTPUTWINDOW_WORDWRAP)

// Additional Shell defined context menu groups
#define IDG_VS_CTXT_PROJECT_ADD_FORMS 0x02A0
#define IDG_VS_CTXT_PROJECT_ADD_MISC  0x02A1
#define IDG_VS_CTXT_ITEM_INCLUDEEXCLUDE 0x02A2
#define IDG_VS_CTXT_FOLDER_ADD        0x02A3
#define IDG_VS_CTXT_REFROOT_ADD       0x02A4
#define IDG_VS_CTXT_REFROOT_TRANSFER  0x02A5
#define IDG_VS_CTXT_WEBREFFOLDER_ADD  0x02A6
#define IDG_VS_CTXT_COMPILELINK       0x02A7
#define IDG_VS_CTXT_REFERENCE         0x02A8
#define IDG_VS_CTXT_APPDESIGNERFOLDER_OPEN  0x02A9

#define IDG_VS_OBJSEARCH_CLEAR        0x02AA
#define IDG_VS_CTXT_CMDWIN_CLEAR      0x02AB

#define IDG_VS_UFINDQUICK             0x02AD
#define IDG_VS_UFINDFIF               0x02AE
#define IDG_VS_FFINDSYMBOL            0x02AF
#define IDG_VS_CTXT_PROJECT_CLASSDIAGRAM 0x02B8
#define IDG_VS_CTXT_PROJECT_ADD_REFERENCES 0x02B9

//0x02B0 used for Codewindow context menu
//0x02B1 used for Codewindow context menu
//0x02B2 used for Codewindow context menu
#define IDG_VS_BWNEXTBM             0x01F0
#define IDG_VS_BWPREVBM             0x01F1
#define IDG_VS_BWNEXTBMF            0x01F2
#define IDG_VS_BWPREVBMF            0x01F3
#define IDG_VS_BWNEWFOLDER          0x01F4
#define IDG_VS_BWENABLE             0x01F5
#define IDG_VS_BWDISABLE            0x01F6
#define IDG_VS_CTXT_BW1             0x01F7
#define IDG_VS_CTXT_BW2             0x01F8
#define IDG_VS_BWDELETE             0x01F9

// Properties panel groups
#define IDG_VS_PROPERTIES_SORT      0x02BA
#define IDG_VS_PROPERTIES_PAGES     0x02BB

#define IDG_VS_CLASSVIEW_SETTINGS   0x02BC  // Used in toolbar
#define IDG_VS_CLASSVIEW_BROWSERSETTINGSBTN 0x02BD
#define IDG_VS_CLASSVIEW_SHOW_INHERITED     0x02BE


//////////////////////////////////////////////////////////////////////////////
//
// Groups for Menu Controllers
//
//////////////////////////////////////////////////////////////////////////////
#define IDG_VS_MNUCTRL_NEWITM                 0x02C0
#define IDG_VS_MNUCTRL_NEWITM_BOTTOM          0x02C1
#define IDG_VS_MNUCTRL_NEWPRJ                 0x02C2
#define IDG_VS_MNUCTRL_NEWPRJ_BOTTOM          0x02C3
#define IDG_VS_MNUCTRL_NAVBACK                0x02C4
#define IDG_VS_MNUCTRL_OBSEARCHOPTIONS        0x02C5
#define IDG_VS_MNUCTRL_FIND                   0x02C6
#define IDG_VS_MNUCTRL_REPLACE                0x02C7

#define IDG_VS_SNIPPET_PROP                   0x02C8
#define IDG_VS_SNIPPET_REF                    0x02C9
#define IDG_VS_SNIPPET_REPL                   0x02CA

#define IDG_VS_CTXT_PROJECT_BUILDDEPENDENCIES 0x02E0
#define IDG_VS_CTXT_PROJECT_SCC_CONTAINER     0x02E1

#define IDG_VS_CTXT_PROJECT_ANALYZE_GENERAL   0x02E2
#define IDG_VS_CTXT_PROJECT_VIEW_GENERAL      0x02E3
#define IDG_VS_CTXT_PROJECT_CONVERT_GENERAL   0x02E4


//////////////////////////////////////////////////////////////////////////////
//
// Cascading Menu Identifiers, created by Visual Studio Shell
//
//////////////////////////////////////////////////////////////////////////////
#define IDM_VS_CSCD_WINDOWS           0x0300
#define IDM_VS_CSCD_TASKLIST_SORT         0x0301
#define IDM_VS_CSCD_TASKLIST_FILTER       0x0302
#define IDM_VS_CSCD_TASKLIST_VIEWMENU_FILTER  0x0303
#define IDM_VS_CSCD_DEBUGWINDOWS          0x0304
#define IDM_VS_EDITOR_CSCD_OUTLINING_MENU 0x0305
#define IDM_VS_CSCD_COMMANDBARS               0x0306
#define IDM_VS_CSCD_OLEVERBS                  0x0307
#define IDM_VS_CSCD_NEW                       0x0308
#define IDM_VS_CSCD_OPEN                      0x0309
#define IDM_VS_CSCD_ADD                       0x030A
#define IDM_VS_CSCD_MNUDES                    0x030B
#define IDM_VS_CSCD_FILEMRU                   0x030C
#define IDM_VS_CSCD_PROJMRU                   0x030D
#define IDM_VS_CSCD_NEW_PROJ                  0x030E
#define IDM_VS_CSCD_MOVETOPRJ                 0x030F
#define IDM_VS_CSCD_INTERACTIVEWNDWS          0x0310

#define IDM_VS_CSCD_BUILD                     0x0330
#define IDM_VS_CSCD_REBUILD                   0x0331
#define IDM_VS_CSCD_CLEAN                     0x0332
#define IDM_VS_CSCD_DEPLOY                    0x0333
#define IDM_VS_CSCD_MISCFILES                 0x0334
#define IDM_VS_CSCD_PROJONLY                  0x0335
#define IDM_VS_CSCD_PGO_BUILD                 0x0336

#define IDM_VS_CSCD_EXTTOOLS                  0x0340

#define IDM_VS_CSCD_SOLUTION_ADD              0x0350
#define IDM_VS_CSCD_SOLUTION_DEBUG            0x0351
#define IDM_VS_CSCD_PROJECT_ADD               0x0352
#define IDM_VS_CSCD_PROJECT_DEBUG             0x0353

// ClassView cascades
#define IDM_VS_CSCD_CV_PROJADD                0x0354
#define IDM_VS_CSCD_CV_ITEMADD                0x0355

#define IDM_VS_CSCD_SLNFLDR_ADD               0x0357

#define IDM_VS_CSCD_TASKLIST_COLUMNS          0x0358

#define IDM_VS_CSCD_CALLBROWSER               0x0359
#define IDG_VS_VIEW_CALLBROWSER               0x035A
#define IDG_VS_VIEW_CALLBROWSER_CASCADE       0x035B
#define IDG_VS_VIEW_CALLBROWSER_SHOW          0x035C

#define IDM_VS_CSCD_FINDRESULTS               0x035D

#define IDM_VS_CSCD_PROJECT_ANALYZE           0x035E
#define IDM_VS_CSCD_PROJECT_VIEW              0x035F
#define IDM_VS_CSCD_PROJECT_CONVERT           0x0360
#define IDM_VS_CSCD_PROJECT_BUILDDEPENDENCIES 0x0361
#define IDM_VS_CSCD_PROJECT_SCC               0x0362
#define IDM_VS_CSCD_WINDOW_LAYOUTS            0x0363

#define IDM_VS_CSCD_TASKLIST_GROUPS           0x0364

//////////////////////////////////////////////////////////////////////////////
//
// Context Menu Identifiers, created by Visual Studio Shell
//
//////////////////////////////////////////////////////////////////////////////
#define IDM_VS_CTXT_PROJNODE          0x0402
#define IDM_VS_CTXT_PROJWIN           0x0403
#define IDM_VS_CTXT_PROJWINBREAK      0x0404
#define IDM_VS_CTXT_ERRORLIST         0x0405
#define IDM_VS_CTXT_DOCKEDWINDOW      0x0406
#define IDM_VS_CTXT_MENUDES           0x0407
#define IDM_VS_CTXT_PROPBRS           0x0408
#define IDM_VS_CTXT_TOOLBOX           0x0409
// UNUSED: 0x040A - 0x040C
#define IDM_VS_CTXT_CODEWIN           0x040D
#define IDM_VS_CTXT_TASKLIST          0x040E
#define IDM_VS_CTXT_RESULTSLIST       0x0411
#define IDM_VS_CTXT_STUBPROJECT       0x0412
#define IDM_VS_CTXT_SOLNNODE          0x0413
#define IDM_VS_CTXT_SOLNFOLDER        0x0414

// Slctn of one or more ProjNodes & SolnNode (doesn't involve ProjItem nodes)
#define IDM_VS_CTXT_XPROJ_SLNPROJ     0x0415
// Slctn of one or more ProjItems & SolnNode (min 1 ProjItem & may involve ProjNodes too)
#define IDM_VS_CTXT_XPROJ_SLNITEM     0x0416
// Selection of one more Project Nodes and one or more Project Items across projects (does not involve Solution Node) 
#define IDM_VS_CTXT_XPROJ_PROJITEM    0x0417
// Selection of two or more Project Nodes (does not involve the Solution Node or Project Item Nodes)
#define IDM_VS_CTXT_XPROJ_MULTIPROJ   0x0418
// Selection of one more Project Items across projects (does not involve Project Nodes or Solution Node)
#define IDM_VS_CTXT_XPROJ_MULTIITEM   0x0419

#define IDM_VS_CTXT_NOCOMMANDS        0x041A

// Miscellaneous Files project and item context menus
#define IDM_VS_CTXT_MISCFILESPROJ     0x041B

// Selection of two or more solution folders
#define IDM_VS_CTXT_XPROJ_MULTIFOLDER 0x041C
// Selection of combination of projects and solution folders
#define IDM_VS_CTXT_XPROJ_MULTIPROJFLDR 0x041D

// Command Window context menu
#define IDM_VS_CTXT_COMMANDWINDOW     0x041F

// AutoHide context menu on channel
#define IDM_VS_CTXT_AUTOHIDE          0x0420

// Expansion Manager description pane context menu
#define IDM_VS_CTXT_EXPANSION_DESC    0x0421

// Expansion Manager description pane context menu commands
#define IDG_VS_CTXT_EXPANSION_DESC_COPY 0x0422     
#define IDG_VS_CTXT_EXPANSION_DESC_SELECTALL 0x0423

#define IDM_VS_CTXT_FIND_REGEX        0x0424
#define IDM_VS_CTXT_REPLACE_REGEX     0x0425
#define IDM_VS_CTXT_FIND_WILD         0x0426
#define IDM_VS_CTXT_REPLACE_WILD      0x0427
#define IDM_VS_CTXT_EXTTOOLSARGS      0x0428
#define IDM_VS_CTXT_EXTTOOLSDIRS      0x0429

// EZMdi context menus
#define IDM_VS_CTXT_EZTOOLWINTAB      0x042A
#define IDM_VS_CTXT_EZDOCWINTAB       0x042B
#define IDM_VS_CTXT_EZDRAGGING        0x042C
#define IDM_VS_CTXT_EZCHANNEL         0x042D

// New File/Add New Item Open button drop-down menu
#define IDM_VS_CTXT_OPENDROPDOWN      0x042E

// Framework Version drop-down menu
#define IDM_VS_CTXT_FRAMEWORKVERSION  0x042F


// Common Item Node context menu
#define IDM_VS_CTXT_ITEMNODE          0x0430

// Folder Node context menu
#define IDM_VS_CTXT_FOLDERNODE        0x0431

//////////////////////////////////////////////////////////////////////////////
// ClassView context menus
#define IDM_VS_CTXT_CV_PROJECT        0x0432
#define IDM_VS_CTXT_CV_ITEM           0x0433
#define IDM_VS_CTXT_CV_FOLDER         0x0434
#define IDM_VS_CTXT_CV_GROUPINGFOLDER 0x0435
#define IDM_VS_CTXT_CV_MULTIPLE       0x0436
#define IDM_VS_CTXT_CV_MULTIPLE_MEMBERS 0x0437
#define IDM_VS_CTXT_CV_MEMBER          0x0438
#define IDM_VS_CTXT_CV_NON_SYMBOL_MEMBERS   0x0439
#define IDM_VS_CTXT_CV_PROJECT_REFS_FOLDER  0x0440
#define IDM_VS_CTXT_CV_PROJECT_REFERENCE    0x0441

#define IDM_VS_CTXT_CV_NO_SOURCE_ITEM       0x0442
#define IDM_VS_CTXT_CV_NO_SOURCE_MEMBER     0x0443

#define IDM_VS_CTXT_CV_MULTIPLE_NO_SOURCE          0x049
#define IDM_VS_CTXT_CV_MULTIPLE_MEMBERS_NO_SOURCE  0x04A

// Object Browsing tools context menus
#define IDM_VS_SYMBOLS_DUMMY           0x0444
#define IDM_VS_CTXT_OBJBROWSER_OBJECTS 0x0445
#define IDM_VS_CTXT_OBJBROWSER_MEMBERS 0x0446
#define IDM_VS_CTXT_OBJBROWSER_DESC   0x0447
#define IDM_VS_CTXT_OBJSEARCH         0x0448

//#define IDM_VS_CTXT_CV_MULTIPLE_NO_SOURCE          0x049
//#define IDM_VS_CTXT_CV_MULTIPLE_MEMBERS_NO_SOURCE  0x04A

#define IDG_VS_FRAMEWORKVERSIONDROPDOWN_MENU        0x0449

//////////////////////////////////////////////////////////////////////////////
// Reference context menus
// Reference Root Node context menu
#define IDM_VS_CTXT_REFERENCEROOT     0x0450
// Reference Item context menu
#define IDM_VS_CTXT_REFERENCE         0x0451
// Web Reference Folder context menu
#define IDM_VS_CTXT_WEBREFFOLDER      0x0452
// App Designer Folder context menu
#define IDM_VS_CTXT_APPDESIGNERFOLDER 0x0453
// Find All References context menu
#define IDM_VS_CTXT_FINDALLREF        0x0454
//////////////////////////////////////////////////////////////////////////////
// Right drag menu group
#define IDM_VS_CTXT_RIGHT_DRAG        0x0460
#define IDG_VS_CTXT_RIGHT_DRAG1       0x0461
#define IDG_VS_CTXT_RIGHT_DRAG2       0x0462

//////////////////////////////////////////////////////////////////////////////
// Web context menus
#define IDM_VS_CTXT_WEBPROJECT        0x0470
#define IDM_VS_CTXT_WEBFOLDER         0x0471
#define IDM_VS_CTXT_WEBITEMNODE       0x0472
// BEWARE!!!: IDM_VS_CTXT_BOOKMARK is defined as 0x0473
#define IDM_VS_CTXT_WEBSUBWEBNODE     0x0474

//////////////////////////////////////////////////////////////////////////////
// Error correction context menu and group
#define IDM_VS_CTXT_ERROR_CORRECTION  0x0480
#define IDG_VS_CTXT_ERROR_CORRECTION  0x0481

//////////////////////////////////////////////////////////////////////////////
// Context menu organizers

//No group for the Misc menu, since nobody purposely adds menus to this group,
//it's used as a catchall to which we programmatically assign unparented context menus
#define IDM_VS_CTXT_MISC                0x0490

#define IDM_VS_CTXT_CV_ALL              0x0491
#define IDG_VS_CTXT_CV_ALL              0x0492
#define IDM_VS_CTXT_OBJBROWSER_ALL      0x0493
#define IDG_VS_CTXT_OBJBROWSER_ALL      0x0494
#define IDM_VS_CTXT_SOLNEXPL_ALL        0x0495
#define IDG_VS_CTXT_SOLNEXPL_ALL        0x0496

// CSHARP REFACTORING Context menu
#define IDM_VS_CTX_REFACTORING          0x0497

#define IDM_VS_CTXT_EDITOR_ALL          0x0498
#define IDG_VS_CTXT_EDITOR_ALL          0x0499

//////////////////////////////////////////////////////////////////////////////
// Bookmark window context menu
#define IDM_VS_CTXT_BOOKMARK          0x0473

//////////////////////////////////////////////////////////////////////////////
//
// Menu Controller dentifiers, created by Visual Studio Shell
//
//////////////////////////////////////////////////////////////////////////////
#define IDM_VS_MNUCTRL_NEWITM                   0x0500
#define IDM_VS_MNUCTRL_NEWPRJ                   0x0501
#define IDM_VS_MNUCTRL_OTRWNDWS                 0x0502
#define IDM_VS_MNUCTRL_NAVBACK                  0x0503
#define IDM_VS_MNUCTRL_OBSEARCHOPTS             0x0504
#define IDM_VS_MNUCTRL_CVGROUPING               0x0505
#define IDM_VS_MNUCTRL_OBGRPOBJS                0x0506
#define IDM_VS_MNUCTRL_OBGRPMEMS                0x0507
#define IDM_VS_MNUCTRL_OBGRPVIEWS               0x0509
#define IDM_VS_MNUCTRL_OBGRPMEMSACCESS          0x050A
#define IDM_VS_MNUCTRL_CALLBROWSER1_SETTINGS    0x050B
#define IDM_VS_MNUCTRL_CALLBROWSER2_SETTINGS    0x050C
#define IDM_VS_MNUCTRL_CALLBROWSER3_SETTINGS    0x050D
#define IDM_VS_MNUCTRL_CALLBROWSER4_SETTINGS    0x050E
#define IDM_VS_MNUCTRL_CALLBROWSER5_SETTINGS    0x050F
#define IDM_VS_MNUCTRL_CALLBROWSER6_SETTINGS    0x0510
#define IDM_VS_MNUCTRL_CALLBROWSER7_SETTINGS    0x0511
#define IDM_VS_MNUCTRL_CALLBROWSER8_SETTINGS    0x0512
#define IDM_VS_MNUCTRL_CALLBROWSER9_SETTINGS    0x0513
#define IDM_VS_MNUCTRL_CALLBROWSER10_SETTINGS   0x0514
#define IDM_VS_MNUCTRL_CALLBROWSER11_SETTINGS   0x0515
#define IDM_VS_MNUCTRL_CALLBROWSER12_SETTINGS   0x0516
#define IDM_VS_MNUCTRL_CALLBROWSER13_SETTINGS   0x0517
#define IDM_VS_MNUCTRL_CALLBROWSER14_SETTINGS   0x0518
#define IDM_VS_MNUCTRL_CALLBROWSER15_SETTINGS   0x0519
#define IDM_VS_MNUCTRL_CALLBROWSER16_SETTINGS   0x051A
#define IDM_VS_MNUCTRL_FIND                     0x051B
#define IDM_VS_MNUCTRL_REPLACE                  0x051C
#define IDM_VS_MNUCTRL_FILTERERRORLIST          0x051D
#define IDM_VS_MNUCTRL_FILTERSOLUTIONEXPLORER   0x051E

// Text editor toolbar groups
#define IDG_VS_EDITTOOLBAR_COMPLETION 0x0550
#define IDG_VS_EDITTOOLBAR_INDENT     0x0551
#define IDG_VS_EDITTOOLBAR_COMMENT    0x0552
#define IDG_VS_EDITTOOLBAR_TEMPBOOKMARKS    0x0553

// Edit menu groups (HTML Editor Edit | Advanced)
#define IDG_TAG_OUTLINING             0x5554  

// Format Menu groups
#define IDG_VS_FORMAT_STYLE     0x0569
#define IDG_VS_FORMAT_COLOR     0x056A
#define IDG_VS_FORMAT_PARAGRAPH 0x056B
#define IDG_VS_FORMAT_INDENT    0x056C
#define IDG_VS_FORMAT_GRID      0x0554
#define IDG_VS_FORMAT_SPACE     0x0555
#define IDG_VS_FORMAT_CENTER    0x0556
#define IDG_VS_FORMAT_ORDER     0x0557
#define IDG_VS_FORMAT_ALIGN     0x0567
#define IDG_VS_FORMAT_LOCK      0x0558
#define IDG_VS_FORMAT_ELEMENT   0x056D
// skip down to 0x0590 for IDG_VS_FORMAT_ANCHORS

// Format Align menu groups
#define IDG_VS_FORMAT_ALIGN_X     0x0559
#define IDG_VS_FORMAT_ALIGN_Y     0x055A
#define IDG_VS_FORMAT_ALIGN_GRID  0x055B

// Format Size menu groups
#define IDG_VS_FORMAT_SIZE        0x055C

// Format Space menu groups
#define IDG_VS_FORMAT_SPACE_X     0x055D
#define IDG_VS_FORMAT_SPACE_Y     0x055E

// Format Center menu groups
#define IDG_VS_FORMAT_CENTER_CMDS 0x055F

// Format Order menu groups
#define IDG_VS_FORMAT_ORDER_CMDS  0x0560

// Format Grid menu group
#define IDG_VS_FORMAT_GRID_CMDS   0x0570

// Layout Position menu group
#define IDG_VS_LAYOUT_POSITION_CMDS 0x592
#define IDG_VS_LAYOUT_POSITION_OPTIONS 0x596

// Format cascaded menus
#define IDM_VS_CSCD_FORMAT_FONT      0x056F
#define IDM_VS_CSCD_FORMAT_JUSTIFY   0x0570
#define IDM_VS_CSCD_FORMAT_ALIGN     0x0561
#define IDM_VS_CSCD_FORMAT_SIZE      0x0562
#define IDM_VS_CSCD_FORMAT_SPACE_X   0x0563
#define IDM_VS_CSCD_FORMAT_SPACE_Y   0x0564
#define IDM_VS_CSCD_FORMAT_CENTER    0x0565
#define IDM_VS_CSCD_FORMAT_ORDER     0x0566

// View menu groups
#define IDG_VS_VIEW_TABORDER         0x0568
#define IDG_VS_VIEW_OPTIONS          0x0571


// 0x0569 used in IDG_VS_FORMAT_STYLE above
// 0x056A used in IDG_VS_FORMAT_COLOR above
// 0x056B used in IDG_VS_FORMAT_PARAGRAPH above
// 0x056C used in IDG_VS_FORMAT_INDENT above
// 0x056D used in IDG_VS_FORMAT_ELEMENT above
// 0x056F used in IDM_VS_CSCD_FORMAT_FONT above
// 0x0570 used in IDM_VS_CSCD_FORMAT_JUSTIFY above
// 0x0571 used in IDG_VS_VIEW_OPTIONS above


// Format Paragraph menu groups
#define IDG_VS_FORMAT_FONTFACE       0x0572
#define IDG_VS_FORMAT_FONTSCRIPT     0x0573
#define IDG_VS_FORMAT_JUSTIFY        0x0574

// Layout menu
#define IDM_VS_LAYOUT_MENU            0x0575

// Table cascaded menus
#define IDM_VS_CSCD_TABLE_INSERT     0x0576
#define IDM_VS_CSCD_TABLE_DELETE     0x0577
#define IDM_VS_CSCD_TABLE_SELECT     0x0578
#define IDM_VS_CSCD_TABLE_RESIZE     0x0596

// Table menu groups
#define IDG_VS_TABLE_MAIN            0x0579
#define IDG_VS_TABLE_INSERT_1        0x057A
#define IDG_VS_TABLE_INSERT_2        0x057B
#define IDG_VS_TABLE_INSERT_3        0x057C
#define IDG_VS_TABLE_INSERT_4        0x057D
#define IDG_VS_TABLE_DELETE          0x057E
#define IDG_VS_TABLE_SELECT          0x057F
#define IDM_VS_CSCD_LAYOUT_POSITION  0x0593
#define IDG_VS_LAYOUT_INSERT	     0x0594
#define IDG_VS_TABLE_RESIZE          0x0595

// Frame Set Menu
#define IDM_VS_FRAMESET_MENU         0x0580

// Frame Set Menu groups
#define IDG_VS_FRAME_WHOLE           0x0581
#define IDG_VS_FRAME_INDV            0x0582
#define IDG_VS_FRAME_NEW             0x0583

// Tools Menu groups
#define IDG_VS_TOOLS_EDITOPT         0x0584

// Insert Menu
#define IDM_VS_INSERT_MENU           0x0585

// Insert Menu groups
#define IDG_VS_INSERT_TAGS           0x0586
#define IDG_VS_INSERT_TAGS2          0x0587

// Continuation of Format Menu groups
#define IDG_VS_FORMAT_ANCHORS        0x0590
#define IDG_VS_LAYOUT_POSITION       0x0591

// Project cascaded menus
#define IDM_VS_CSCD_PROJECT_WEB      0x0600

// More Object browser groups
#define IDG_VS_OBJBROWSER_ADDREFERENCE       0x0610
#define IDG_VS_OBJBROWSER_ADDTOFAVOURITES    0x0611
#define IDG_VS_OBJBROWSER_SHOW_INHERITED     0x0612

// Code Definition View groups
#define IDG_VS_CODEDEFVIEW                   0x0617

// Project menu groups
#define IDG_VS_CTXT_PROJECT_BUILD_ORDER      0x0620
#define IDG_VS_CTXT_PROJECT_BUILD_PGO        0x0621

// Goto menu
#define IDG_VS_GOTO                  0x0622


///////////////////////////////////////////////
//
// EzMDI files command group
//
///////////////////////////////////////////////

#define IDM_EZMDI_FILELIST                   0x0650
#define IDG_EZMDI_FILELIST                   0x0651

///////////////////////
// Calls Browser groups
///////////////////////

#define IDG_VS_CALLBROWSER_TYPE                 0x0660
#define IDG_VS_CALLBROWSER_EDIT                 0x0661
#define IDG_VS_CALLBROWSER_NAVIGATION           0x0662
#define IDM_VS_CTXT_CALLBROWSER                 0x0663

#define IDG_VS_TOOLBAR_CALLBROWSER1_CBSETTINGS  0x0670
#define IDG_VS_TOOLBAR_CALLBROWSER2_CBSETTINGS  0x0671
#define IDG_VS_TOOLBAR_CALLBROWSER3_CBSETTINGS  0x0672
#define IDG_VS_TOOLBAR_CALLBROWSER4_CBSETTINGS  0x0673
#define IDG_VS_TOOLBAR_CALLBROWSER5_CBSETTINGS  0x0674
#define IDG_VS_TOOLBAR_CALLBROWSER6_CBSETTINGS  0x0675
#define IDG_VS_TOOLBAR_CALLBROWSER7_CBSETTINGS  0x0676
#define IDG_VS_TOOLBAR_CALLBROWSER8_CBSETTINGS  0x0677
#define IDG_VS_TOOLBAR_CALLBROWSER9_CBSETTINGS  0x0678
#define IDG_VS_TOOLBAR_CALLBROWSER10_CBSETTINGS 0x0679
#define IDG_VS_TOOLBAR_CALLBROWSER11_CBSETTINGS 0x067A
#define IDG_VS_TOOLBAR_CALLBROWSER12_CBSETTINGS 0x067B
#define IDG_VS_TOOLBAR_CALLBROWSER13_CBSETTINGS 0x067C
#define IDG_VS_TOOLBAR_CALLBROWSER14_CBSETTINGS 0x067D
#define IDG_VS_TOOLBAR_CALLBROWSER15_CBSETTINGS 0x067E
#define IDG_VS_TOOLBAR_CALLBROWSER16_CBSETTINGS 0x067F

#define IDG_VS_CALLBROWSER1_SETTINGSBTN         0x0680
#define IDG_VS_CALLBROWSER2_SETTINGSBTN         0x0681
#define IDG_VS_CALLBROWSER3_SETTINGSBTN         0x0682
#define IDG_VS_CALLBROWSER4_SETTINGSBTN         0x0683
#define IDG_VS_CALLBROWSER5_SETTINGSBTN         0x0684
#define IDG_VS_CALLBROWSER6_SETTINGSBTN         0x0685
#define IDG_VS_CALLBROWSER7_SETTINGSBTN         0x0686
#define IDG_VS_CALLBROWSER8_SETTINGSBTN         0x0687
#define IDG_VS_CALLBROWSER9_SETTINGSBTN         0x0688
#define IDG_VS_CALLBROWSER10_SETTINGSBTN        0x0689
#define IDG_VS_CALLBROWSER11_SETTINGSBTN        0x068A
#define IDG_VS_CALLBROWSER12_SETTINGSBTN        0x068B
#define IDG_VS_CALLBROWSER13_SETTINGSBTN        0x068C
#define IDG_VS_CALLBROWSER14_SETTINGSBTN        0x068D
#define IDG_VS_CALLBROWSER15_SETTINGSBTN        0x068E
#define IDG_VS_CALLBROWSER16_SETTINGSBTN        0x068F

#define IDG_VS_CALLBROWSER1_SORTING             0x0690
#define IDG_VS_CALLBROWSER2_SORTING             0x0691
#define IDG_VS_CALLBROWSER3_SORTING             0x0692
#define IDG_VS_CALLBROWSER4_SORTING             0x0693
#define IDG_VS_CALLBROWSER5_SORTING             0x0694
#define IDG_VS_CALLBROWSER6_SORTING             0x0695
#define IDG_VS_CALLBROWSER7_SORTING             0x0696
#define IDG_VS_CALLBROWSER8_SORTING             0x0697
#define IDG_VS_CALLBROWSER9_SORTING             0x0698
#define IDG_VS_CALLBROWSER10_SORTING            0x0699
#define IDG_VS_CALLBROWSER11_SORTING            0x069A
#define IDG_VS_CALLBROWSER12_SORTING            0x069B
#define IDG_VS_CALLBROWSER13_SORTING            0x069C
#define IDG_VS_CALLBROWSER14_SORTING            0x069D
#define IDG_VS_CALLBROWSER15_SORTING            0x069E
#define IDG_VS_CALLBROWSER16_SORTING            0x069F

#define IDG_VS_CALLBROWSER1_SETTINGS            0x06A0
#define IDG_VS_CALLBROWSER2_SETTINGS            0x06A1
#define IDG_VS_CALLBROWSER3_SETTINGS            0x06A2
#define IDG_VS_CALLBROWSER4_SETTINGS            0x06A3
#define IDG_VS_CALLBROWSER5_SETTINGS            0x06A4
#define IDG_VS_CALLBROWSER6_SETTINGS            0x06A5
#define IDG_VS_CALLBROWSER7_SETTINGS            0x06A6
#define IDG_VS_CALLBROWSER8_SETTINGS            0x06A7
#define IDG_VS_CALLBROWSER9_SETTINGS            0x06A8
#define IDG_VS_CALLBROWSER10_SETTINGS           0x06A9
#define IDG_VS_CALLBROWSER11_SETTINGS           0x06AA
#define IDG_VS_CALLBROWSER12_SETTINGS           0x06AB
#define IDG_VS_CALLBROWSER13_SETTINGS           0x06AC
#define IDG_VS_CALLBROWSER14_SETTINGS           0x06AD
#define IDG_VS_CALLBROWSER15_SETTINGS           0x06AE
#define IDG_VS_CALLBROWSER16_SETTINGS           0x06AF

#define IDG_VS_TOOLBAR_CALLBROWSER1_TYPE        0x06B0
#define IDG_VS_TOOLBAR_CALLBROWSER2_TYPE        0x06B1
#define IDG_VS_TOOLBAR_CALLBROWSER3_TYPE        0x06B2
#define IDG_VS_TOOLBAR_CALLBROWSER4_TYPE        0x06B3
#define IDG_VS_TOOLBAR_CALLBROWSER5_TYPE        0x06B4
#define IDG_VS_TOOLBAR_CALLBROWSER6_TYPE        0x06B5
#define IDG_VS_TOOLBAR_CALLBROWSER7_TYPE        0x06B6
#define IDG_VS_TOOLBAR_CALLBROWSER8_TYPE        0x06B7
#define IDG_VS_TOOLBAR_CALLBROWSER9_TYPE        0x06B8
#define IDG_VS_TOOLBAR_CALLBROWSER10_TYPE       0x06B9
#define IDG_VS_TOOLBAR_CALLBROWSER11_TYPE       0x06BA
#define IDG_VS_TOOLBAR_CALLBROWSER12_TYPE       0x06BB
#define IDG_VS_TOOLBAR_CALLBROWSER13_TYPE       0x06BC
#define IDG_VS_TOOLBAR_CALLBROWSER14_TYPE       0x06BD
#define IDG_VS_TOOLBAR_CALLBROWSER15_TYPE       0x06BE
#define IDG_VS_TOOLBAR_CALLBROWSER16_TYPE       0x06BF

#define IDG_VS_CALLBROWSER_TYPE_POPUP           0x06C0


///////////////////////
// Preview Changes groups
///////////////////////

#define IDG_VS_PREVIEWCHANGES_EDIT              0x06D0
#define IDM_VS_CTXT_PREVIEWCHANGES              0x06D1

#define IDG_VS_TOOLBAR_CALLBROWSER1_ACTIONS     0x06E0
#define IDG_VS_TOOLBAR_CALLBROWSER2_ACTIONS     0x06E1
#define IDG_VS_TOOLBAR_CALLBROWSER3_ACTIONS     0x06E2
#define IDG_VS_TOOLBAR_CALLBROWSER4_ACTIONS     0x06E3
#define IDG_VS_TOOLBAR_CALLBROWSER5_ACTIONS     0x06E4
#define IDG_VS_TOOLBAR_CALLBROWSER6_ACTIONS     0x06E5
#define IDG_VS_TOOLBAR_CALLBROWSER7_ACTIONS     0x06E6
#define IDG_VS_TOOLBAR_CALLBROWSER8_ACTIONS     0x06E7
#define IDG_VS_TOOLBAR_CALLBROWSER9_ACTIONS     0x06E8
#define IDG_VS_TOOLBAR_CALLBROWSER10_ACTIONS    0x06E9
#define IDG_VS_TOOLBAR_CALLBROWSER11_ACTIONS    0x06EA
#define IDG_VS_TOOLBAR_CALLBROWSER12_ACTIONS    0x06EB
#define IDG_VS_TOOLBAR_CALLBROWSER13_ACTIONS    0x06EC
#define IDG_VS_TOOLBAR_CALLBROWSER14_ACTIONS    0x06ED
#define IDG_VS_TOOLBAR_CALLBROWSER15_ACTIONS    0x06EE
#define IDG_VS_TOOLBAR_CALLBROWSER16_ACTIONS    0x06EF


///////////////////////////////////////////////
//
// VS Enterprise menu and menu groups
//
///////////////////////////////////////////////

// Team Foundation Client standard menu
#define IDM_MENU_TEAM_FOUNDATION_CLIENT     0x700
#define IDM_MENU_PROJECT_CONTEXT_MENU       0x707
#define IDM_TEAM_PROJECT_SETTINGS_MENU      0x708
#define IDM_TEAM_SERVER_SETTINGS_MENU       0x709

// Team Foundation Client Toolbar Group
#define IDG_TEAM_FOUNDATION_CLIENT_TOOLBAR  0x701

// Commands on the Team menu for projects that require project context
#define IDG_MENU_PROJECT_CONTEXT            0x702
// Commands on the Team menu shared by tools (e.g. Properties)
#define IDG_SHARED_COMMANDS                 0x703
// Tool-specific commands on the Team menu
#define IDG_TOOL_COMMANDS                   0x704

// Team Project Settings Cascade Menu Group
#define IDG_TEAM_PROJECT_SETTINGS_COMMANDS  0x705
// Team Server Settings Cascade Menu Group
#define IDG_TEAM_SERVER_SETTINGS_COMMANDS   0x706

// Commands on the TE context menu for projects that require project context
#define IDG_CTXT_PROJECT_CONTEXT            0x710


// Thes are defined up and copies here to ensure accident reuse does not occur
// #define IDG_VS_FILE_ACCOUNTSETTINGS        0x0711
//
// 
// #define IDG_VS_VIEW_ARCH_WINDOWS      0x0720
// #define IDG_VS_VIEW_ORG_WINDOWS       0x0721
// #define IDG_VS_VIEW_CODEBROWSENAV_WINDOWS 0x0722
// #define IDG_VS_VIEW_DEV_WINDOWS       0x0723
// #define IDG_VS_WNDO_FINDRESULTS       0x0724


#define IDG_VS_TOOLBAR_PROJWIN_NAVIGATION           0x730  // Contains navigation commands for the Solution Explorer (back, forward, home)
#define IDG_VS_TOOLBAR_PROJWIN_NEWVIEW              0x731  // Contains the New View toolbar command
#define IDM_VS_CTXT_PROJWIN_FILECONTENTS            0x732  // Context menu for GraphNode items in the Solution Explorer
#define IDM_VS_CSCD_PROJWIN_FILECONTENTS_SCOPELIST  0x733  // Flyout menu for changing the view to a different aspect
#define IDG_VS_CTXT_PROJWIN_SCOPE                   0x734  // Group for containing view-scoping commands (Scope View To This, New View, Change View To)
#define IDG_VS_CTXT_PROJWIN_SCOPELIST               0x735  // Group for containing the dynamic list of scopes the Solution Explorer can be changed to
#define IDG_VS_CTXT_PROJWIN_FILECONTENTS_SCOPE      0x736  // Group specifically for the Item Contents context menu, containing the Scope View To This command
#define IDG_VS_CTXT_PROJWIN_FILECONTENTS_NEWVIEW    0x737  // Group specifically for the Item Contents context menu, containing the New View command
#define IDG_VS_TOOLBAR_PROJWIN_FILTERS              0x738  // Group containing common filters for the Solution Explorer (e.g. Opened, Pending Changes)
#define IDM_VS_CTXT_PEEKRESULT                      0x739  // Context menu for results in Peek
#define IDG_VS_CTXT_PEEKRESULTGROUP                 0x73A  // Contains commands that can run on peek results (Copy Full Path, Promote to Document, Open Containing Folder)

////////////////////////////////////////////////
//
// Refactor menu groups
//
////////////////////////////////////////////////

#define IDG_REFACTORING_COMMON              0x1801
#define IDG_REFACTORING_ADVANCED            0x1802

#define IDBI_ExtractMethod      1
#define IDBI_EncapsulateField   2
#define IDBI_ExtractInterface   3
#define IDBI_Rename             4
#define IDBI_ReorderParameters  5
#define IDBI_RemoveParameters   6
#define IDBI_AddUsing           7
#define IDBI_GenerateMethod     8
#define IDBI_PromoteLocal       9
#define IDBI_Snippet            10


///////////////////////////////////////////////
//
// Server Explorer menu groups
//
///////////////////////////////////////////////

// Groups
#define IDG_SE_CONTEXT_GENERAL                  0x0312
#define IDG_SE_CONTEXT_DELETE                   0x0313
#define IDG_SE_CONTEXT_DATA                     0x0314
#define IDG_SE_CONTEXT_NODE                     0x0315
#define IDG_SE_CONTEXT_DATAPROP                 0x0316
#define IDG_SE_CONTEXT_SQLINSTANCE              0x0317

#define IDG_SE_TOOLBAR_REFRESH                  0X0403
#define IDG_SE_TOOLBAR_VIEW                     0x0404
#define IDG_SE_TOOLBAR_VIEW_LIST                0x0405
#define IDG_SE_TOOLBAR_VIEW_SAVE                0x0406

// A new group under the Tools menu.  It's for <Add..> nodes.
#define IDG_SE_TOOLS_ADD                        0x0408

// Menus
#define IDM_SE_CONTEXT_STANDARD                 0x0503
#define IDM_SE_TOOLBAR_VIEW                     0x0504
#define IDM_SE_TOOLBAR_SERVEREXPLORER           0x0600

///////////////////////////////////////////////
//
// SQL Server Object Explorer menu groups
//
///////////////////////////////////////////////

//Context Menu
#define mnuIdSqlServerObjectExplorerContextMenu           0x2003

///////////////////////////////////////////////
//
// Data Explorer menu groups
//
///////////////////////////////////////////////

#define IDG_DV_GLOBAL1                   0x4001
#define IDG_DV_GLOBAL2                   0x4002
#define IDG_DV_GLOBAL3                   0x4003
#define IDG_DV_CONNECTION                0x4101

// These are the old (DDEX 1.0) names
#define IDG_DV_STATIC                    0x4401
#define IDG_DV_OBJECT                    0x4301
#define IDG_DV_STATICS                   0x4701
#define IDG_DV_OBJECTS                   0x4501
#define IDG_DV_MIXED_OBJECTS             0x4601
#define IDG_DV_MIXED                     0x4801

// These are the new (DDEX 2.0+) names
#define IDG_DV_STATIC_NODE               0x4401
#define IDG_DV_OBJECT_NODE               0x4301
#define IDG_DV_STATIC_NODES              0x4701
#define IDG_DV_HOMOGENOUS_OBJECT_NODES   0x4501
#define IDG_DV_HETEROGENOUS_OBJECT_NODES 0x4601
#define IDG_DV_HETEROGENOUS_NODES        0x4801

//IDG_VS_TOOLBAR is a group that simply contains IDG_VS_TOOLBAR_LIST (the dynamic toolbar list) and IDG_VS_TOOLBAR_CUSTOMIZE (the customize
//command).  This group is placed both on IDM_VS_CSCD_COMMANDBARS and the toolbar tray context menu (IDM_VS_CTXT_TOOLBARS)
#define IDG_VS_TOOLBAR                   0x4802
#define IDG_VS_TOOLBAR_LIST              0x4803
#define IDG_VS_TOOLBAR_CUSTOMIZE         0x4804
#define IDM_VS_CTXT_TOOLBARS             0x4805

// Group containing explorer windows in other windows menu
#define IDG_VS_WNDO_OTRWNDWSEXPLORERS 0x5200

// groups for upgrade commands
#define IDG_VS_ALL_PROJ_UPGRADE          0x5030
#define IDG_VS_PROJ_UPGRADE			     0x5031

// Solution/project fault resolution
#define IDG_VS_CTXT_SOLUTION_RESOLVE     0x5032


#endif // _VSSHLIDS_H_
