//------------------------------------------------------------------------------
// <copyright from='1997' to='2007' company='Microsoft Corporation'>           
//    Copyright (c) Microsoft Corporation. All Rights Reserved.                
//    Information Contained Herein is Proprietary and Confidential.            
// </copyright>                                                                
//------------------------------------------------------------------------------
//
// Definition of the numeric part of the IDs for the VSCT elements of this
// package.
//
// NOTE: if you make any changes here, make sure to make the same changes in
// PkgCmdId.cs.
//

/////////////////////////////////////////////////////////////////////
// Menus
//
#define IDM_DEBUGGER_PROJECT_CONTEXT_MENU               0x0201
#define IDG_DEBUGGER_PROJECT_CONTEXT_MENU_MAIN_GROUP    0x0202
#define IDG_DATA_TIPS_ON_DEBUG                          0x0205
#define IDM_THREAD_WINDOW_TOOLBAR						0x0206
#define IDG_THREAD_WINDOW_TOOLBAR_FLAG					0x0207
#define IDG_THREAD_WINDOW_TOOLBAR_STACKS				0x0208
#define IDG_THREAD_WINDOW_TOOLBAR_GROUPS				0x0209
#define IDG_THREAD_WINDOW_TOOLBAR_SEARCH				0x0210
#define IDG_THREAD_WINDOW_TOOLBAR_FLAG_MENU				0x0211
#define IDG_THREAD_WINDOW_TOOLBAR_FLAG_MENU_GROUP		0x0212
#define IDG_THREAD_WINDOW_TOOLBAR_ARRANGE				0x0213
#define IDG_THREAD_WINDOW_TOOLBAR_TOGGLE_SUSPENDED	    0x0214
#define IDG_DATA_TIPS_CONTEXT                           0x0215
#define IDM_DATA_TIPS_CONTEXT                           0x0216
#define IDG_DATA_TIPS_CONTEXT_CLEAR                     0x0217
#define IDG_DATA_TIPS_MENU_CLEAR                        0x0218
#define IDG_THREAD_WINDOW_SELECT_COLUMNS                0x0219
#define IDM_DATA_TIPS_WATCH_ITEM_CONTEXT				0x021A
#define IDM_DATA_TIPS_TEXT_BOX_CONTEXT					0x021B
#define IDG_DATATIP_TEXTBOX_CLIPBOARD                   0x021C
#define IDG_DATATIP_RADIX                               0x021D
#define IDG_DATATIP_EXPRESSIONS                         0x021E
#define IDM_DISASSEMBLY_WINDOW_TOOLBAR					0x0220
#define IDG_DISASSEMBLY_WINDOW_TOOLBAR_ADDRESS			0x0221
#define IDM_MANAGEDMEMORYANALYSIS_SUBMENU               0x0222
#define IDG_MANAGEDMEMORYANALYSIS_SUBMENU               0x0223

// These values must be synced with intellitrace\Includes\PackageCommandIds.h
#define IDM_IntelliTraceHubDetailsViewFilterContextMenu		0x0225
#define IDM_IntelliTraceHubDetailsViewFilterCategorySubMenu	0x0226
#define IDG_IntelliTraceHubDetailsViewFilterCategoryEventsGroup 0x0227
#define IDG_IntelliTraceHubDetailsViewFilterCategorySubMenuGroup       0x0228


// TODO: re-enabled this constant
#define cmdidClearAllTips                               0x00000101
#define cmdidRazorThreadWindowToolbarExpandStacks		0x00000103
#define cmdidRazorThreadWindowToolbarCollapseStacks		0x00000104
#define cmdidRazorThreadWindowToolbarExpandGroups		0x00000105
#define cmdidRazorThreadWindowToolbarCollapseGroups		0x00000106
#define cmdidRazorThreadWindowToolbarSearchCombo		0x00000107
#define cmdidRazorThreadWindowToolbarSearchHandler		0x00000108
#define cmdidRazorThreadWindowToolbarClearSearch		0x00000109
#define cmdidRazorThreadWindowToolbarSearchCallStack	0x00000110
#define cmdidRazorThreadWindowToolbarFlagJustMyCode		0x00000111
#define cmdidRazorThreadWindowToolbarFlagCustomModules  0x00000112
#define cmdidRazorThreadWindowToolbarFlag				0x00000113
#define cmdidToolsProgramToDebug						0x00000114
#define cmdidDebugProgramToDebug                        0x00000115
#define cmdidInstallJitDebugger                         0x00000116
#define cmdidClearDataTipsSubMenu                       0x00000119
#define cmdidClearDataTipsContextRoot                   0x0000011A
#define cmdidClearDataTipsContextSingle                 0x0000011B
#define cmdidClearDataTipsContextFirst                  0x0000011C
#define cmdidClearDataTipsContextLast                   0x0000021C
#define cmdidClearDataTipsMenuFirst                     0x0000021D
#define cmdidClearDataTipsMenuLast                      0x0000031D
#define cmdidClearActivePinnedTips                      0x0000031E
#define cmdidArrangePinnedTipsOnLine                    0x0000031F
#define cmdidExportDataTips                             0x00000320
#define cmdidImportDataTips                             0x00000321
#define cmdidRazorThreadWindowToolbarGroupCombo         0x00000322
#define cmdidRazorThreadWindowToolbarGroupHandler       0x00000323
#define cmdidRazorThreadWindowToolbarColumnsMenu        0x00000324
#define cmdidThreadWindowToolbarSelectColumnFirst       0x00000325
#define cmdidThreadWindowToolbarSelectColumnLast        0x00000345
#define cmdidRazorThreadWindowToolbarFreezeThreads      0x00000346
#define cmdidRazorThreadWindowToolbarThawThreads        0x00000347
#define cmdidPinExpression                              0x00000348
#define cmdidAddExpression								0x00000349
#define cmdidRemoveExpression							0x0000034A
#define cmdidRazorThreadWindowToolbarShowFlaggedOnly    0x0000034B
#define cmdidRazorThreadWindowToolbarShowCurProcOnly    0x0000034C
#define cmdidRazorDisassemblyWindowToolbarAddressCombo  0x00000360
#define cmdidLaunchManagedMemoryAnalysis                0x00000600

// This must match values in HubExtensions/UIConstants.cs
#define cmdidIntelliTraceHubDetailsViewFilterCategoryTopLevelBase      0x00000700
#define cmdidIntelliTraceHubDetailsViewFilterCategoryTopLevelLast      0x0000072A // excluded
#define cmdidIntelliTraceHubDetailsViewFilterCategorySecondLevelBase   0x0000072A
#define cmdidIntelliTraceHubDetailsViewFilterCategorySecondLevelLast   0x00000750 // excluded

// Bitmaps
#define bmpShieldIcon 1

// Thread window icon strip (image well)
#define imgThreadsExpandCallstack						1
#define imgThreadsCollapseCallstack						2
#define imgThreadsExpandGroups							3
#define imgThreadsCollapseGroups						4
#define imgThreadsFreezeThreads							5
#define imgThreadsThawThreads   						6
