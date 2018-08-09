 // PkgCmdID.h
// Command IDs used in defining command bars
//

#ifndef _VSDBGCMD_H_INCLUDED
#define _VSDBGCMD_H_INCLUDED

#define MULTIPLE_WATCH_WINDOWS 1

///////////////////////////////////////////////////////////////////////////////
// Menu IDs

// menus
#define IDM_DEBUG_MENU                     0x0401
#define IDM_DEBUG_WINDOWS                  0x0402
#define IDM_STEPINTOSPECIFIC               0x0403
#define IDM_STEP_UNIT                      0x0404
#define IDM_MEMORY_WINDOWS                 0x0405
#define IDM_BREAKPOINTS_WINDOW_COLUMN_LIST 0x0406
#define IDM_HIDDEN_COMMANDS                0x0407
#ifdef MULTIPLE_WATCH_WINDOWS
#define IDM_WATCH_WINDOWS                  0x0408
#endif
#define IDM_DEBUG_TOOLBAR_WINDOWS          0x0409
#define IDM_DEBUGGER_CONTEXT_MENUS         0x0410
//#define unused menu ID                   0x0411
#define IDM_BREAKPOINT_SUBMENU             0x0412
#define IDM_DISASM_BREAKPOINT_SUBMENU      0x0413
#define IDM_CALLSTACK_BREAKPOINT_SUBMENU   0x0414
#define IDM_BREAKPOINTS_WINDOW_NEW_LIST    0x0415
#define IDM_NEW_BREAKPOINT_SUBMENU         0x0416
#define IDM_MODULES_LOADSYMBOLS_SUBMENU    0x0417
#define IDM_CALLSTACK_LOADSYMBOLS_SUBMENU  0x0418
#define IDM_STEPINTOSPECIFIC_CONTEXT       0x0419
#define IDM_OTHER_DEBUG_TARGETS_SUBMENU    0x041A

// toolbars
// NOTE: IDM_DEBUG_CONTEXT_TOOLBAR comes before IDM_DEBUG_TOOLBAR
// so that the Debug toolbar will appear to the left of the Debug Location toolbar.
// This uses the fact that the toolbar defined earlier go to the right when on the same line
// (see VS7 bug 295621)
#define IDM_DEBUG_CONTEXT_TOOLBAR       0x0420
#define IDM_DEBUG_TOOLBAR               0x0421
#define IDM_BREAKPOINTS_WINDOW_TOOLBAR  0x0422
#define IDM_DISASM_WINDOW_TOOLBAR       0x0423
#define IDM_ATTACHED_PROCS_TOOLBAR      0x0424
#define IDM_MODULES_WINDOW_TOOLBAR      0x0425

#define IDM_MEMORY_WINDOW_TOOLBAR       0x0430
#define IDM_MEMORY_WINDOW_TOOLBAR1      0x0431
#define IDM_MEMORY_WINDOW_TOOLBAR2      0x0432
#define IDM_MEMORY_WINDOW_TOOLBAR3      0x0433
#define IDM_MEMORY_WINDOW_TOOLBAR4      0x0434
#define IDM_BREAKPOINTS_WINDOW_SORT_LIST            0x0435

#define IDM_WATCH_WINDOW_TOOLBAR            0x0440
#define IDM_WATCH_WINDOW_TOOLBAR1           0x0441
#define IDM_WATCH_WINDOW_TOOLBAR2           0x0442
#define IDM_WATCH_WINDOW_TOOLBAR3           0x0443
#define IDM_WATCH_WINDOW_TOOLBAR4           0x0444
#define IDM_LOCALS_WINDOW_TOOLBAR           0x0445
#define IDM_AUTOS_WINDOW_TOOLBAR            0x0446
#define IDM_WATCH_LOADSYMBOLS_SUBMENU       0x0447

// context menus
#define IDM_WATCH_CONTEXT               0x0500
#define IDM_LOCALS_CONTEXT              0x0501
#define IDM_AUTOS_CONTEXT               0x0502
#define IDM_THREADS_CONTEXT             0x0503
#define IDM_CALLSTACK_CONTEXT           0x0504
#define IDM_DISASM_CONTEXT              0x0505
#define IDM_BREAKPOINTS_WINDOW_CONTEXT  0x0506
#define IDM_MEMORY_CONTEXT              0x0507
#define IDM_REGISTERS_CONTEXT           0x0508
#define IDM_MODULES_CONTEXT             0x0509
#define IDM_DATATIP_CONTEXT             0x050A
#define IDM_ATTACHED_PROCS_CONTEXT      0x050B
#define IDM_BREAKPOINT_CONTEXT          0x050C
#define IDM_CONSOLE_CONTEXT             0x050D
#define IDM_OUTPUT_CONTEXT              0x050E
#define IDM_SCRIPT_PROJECT_CONTEXT      0x050F
#define IDM_THREADTIP_CONTEXT           0x0510
#define IDM_THREAD_IP_MARKER_CONTEXT    0x0511
#define IDM_THREAD_IP_MARKERS_CONTEXT   0x0512
#define IDM_LOADSYMBOLS_CONTEXT         0x0513
#define IDM_SYMBOLINCLUDELIST_CONTEXT   0x0514
#define IDM_SYMBOLEXCLUDELIST_CONTEXT   0x0515
#define IDM_DEBUG_VS_CODEWIN_ADD_WATCH_MENU 0x0516

///////////////////////////////////////////////////////////////////////////////
// Menu Group IDs
#define IDG_DEBUG_MENU                      0x0001
#define IDG_DEBUG_WINDOWS_GENERAL           0x0002
#define IDG_DEBUG_TOOLBAR                   0x0003
#define IDG_EXECUTION                       0x0004
#define IDG_STEPPING                        0x0005
#define IDG_WATCH                           0x0006
#define IDG_BREAKPOINTS                     0x0007
#define IDG_HIDDEN_COMMANDS                 0x0008
#define IDG_WINDOWS                         0x0009
#define IDG_STEPINTOSPECIFIC                0x000A
#define IDG_VARIABLES                       0x000B
#define IDG_WATCH_EDIT                      0x000C
#define IDG_THREAD_CONTROL                  0x000F
#define IDG_DEBUG_DISPLAY                   0x0010
#define IDG_DEBUG_TOOLBAR_STEPPING          0x0011
#define IDG_DEBUGGER_CONTEXT_MENUS          0x0012
#define IDG_MEMORY_WINDOWS                  0x0013
#define IDG_DISASM_NAVIGATION               0x0014
#define IDG_DISASM_BREAKPOINTS              0x0015
#define IDG_DISASM_EXECUTION                0x0016
#define IDG_DISASM_DISPLAY                  0x0017
#define IDG_BREAKPOINTS_WINDOW_NEW          0x0018
#define IDG_BREAKPOINTS_WINDOW_DELETE       0x0019
#define IDG_BREAKPOINTS_WINDOW_ALL          0x001A
#define IDG_BREAKPOINTS_WINDOW_VIEW         0x001B
#define IDG_BREAKPOINTS_WINDOW_EDIT         0x001C
#define IDG_BREAKPOINTS_WINDOW_COLUMNS      0x001D
#define IDG_BREAKPOINTS_WINDOW_COLUMN_LIST  0x001E
#define IDG_BREAKPOINTS_WINDOW_NEW_LIST     0x001F
#define IDG_BREAKPOINTS_WINDOW_PROPERTIES_LIST 0x0020
#define IDG_NEW_BREAKPOINT_SUBMENU          0x0021
#define IDG_PROGRAM_TO_DEBUG                0x0024
#define IDG_DEBUG_TOOLBAR_WATCH             0x0025
#define IDG_DEBUG_VS_CODEWIN_ADD_WATCH_GROUP                0x0026
#define IDG_DEBUG_CONTEXT_TOOLBAR           0x0027
#define IDG_DISASM_WINDOW_TOOLBAR           0x0028
#define IDG_MEMORY_FORMAT                   0x0100
#define IDG_MEMORY_INT_FORMAT               0x0101
#define IDG_MEMORY_FLT_FORMAT               0x0102
#define IDG_MEMORY_TEXT                     0x0103
#define IDG_MEMORY_MISC                     0x0104
#define IDG_MEMORY_TOOLBAR                  0x0105
#define IDG_REGISTERS_GROUPS                0x0108
#define IDG_REGISTERS_EDIT                  0x0109
#define IDG_CUSTOMIZABLE_CONTEXT_MENU_COMMANDS 0x0110
#define IDG_CALLSTACK_COPY                  0x0111
#define IDG_CALLSTACK_NAVIGATE              0x0112
#define IDG_CALLSTACK_BREAKPOINTS           0x0113
#define IDG_CALLSTACK_DISPLAY               0x0114
#define IDG_CALLSTACK_OPTIONS               0x0115
#define IDG_DEBUG_WINDOWS_INSPECT           0x0116
#define IDG_DEBUG_WINDOWS_CONTEXT           0x0117
#define IDG_DEBUG_WINDOWS_DISASM            0x0118
#define IDG_CRASHDUMP                       0x0119
#define IDG_DEBUG_TOOLBAR_WINDOWS           0x011A
#define IDG_DEBUG_TOOLBAR_EXECUTION         0x011B
#define IDG_THREAD_COPY                     0x011C
#define IDG_TOOLS_DEBUG                     0x011D
#define IDG_STEP_UNIT                       0x011E
#ifdef MULTIPLE_WATCH_WINDOWS
#define IDG_WATCH_WINDOWS                   0x011F
#endif
#define IDG_CALLSTACK_SYMBOLS               0x0120
#define IDG_MODULES_COPY                    0x0121
#define IDG_MODULES_SYMBOLS                 0x0122
#define IDG_MODULES_DISPLAY                 0x0123
#define IDG_DATATIP_WATCH                   0x0124
#define IDG_DATATIP_VISIBILITY              0x0125
#define IDG_ATTACHED_PROCS_COPY             0x0126
#define IDG_ATTACHED_PROCS_EXECCNTRL        0x0127
#define IDG_ATTACHED_PROCS_ADDBP            0x0128
#define IDG_ATTACHED_PROCS_ATTACH           0x0129
#define IDG_ATTACHED_PROCS_COLUMNS          0x0130
#define IDG_ATTACHED_PROCS_DETACHONSTOP     0x0131
#define IDG_DEBUG_CONSOLE                   0x0132
#define IDG_MODULES_JMC                     0x0133
//#define unused group ID                   0x0134
//#define unused group ID                   0x0135
#define IDG_BREAKPOINT_CONTEXT_ADD_DELETE   0x0136
#define IDG_BREAKPOINT_CONTEXT_MODIFY       0x0137
#define IDG_ATTACHED_PROCS_STEPPING         0x0138
#define IDG_CONSOLE_CONTEXT                 0x0139
#define IDG_DATATIP_CLIPBOARD               0x013A
#define IDG_ATTACHED_PROCS_EXECCNTRL2       0x013B
#define IDG_OUTPUT_CONTEXT                  0x013C
#define IDG_SINGLEPROC_EXECUTION            0x013D
#define IDG_THREAD_FLAGGING	                0x013E
#define IDG_THREADTIP_WATCH                 0x013f
#define IDG_THREADTIP_CLIPBOARD             0x0140
#define IDG_THREAD_IP_MARKER_CONTEXT        0x0141
#define IDG_THREAD_IP_MARKERS_CONTEXT       0x0142
#define IDG_THREAD_IP_MARKERS_SWITCHCONTEXT 0x0143
#define IDG_THREAD_IP_MARKERS_FLAG          0x0144
#define IDG_THREAD_IP_MARKERS_UNFLAG        0x0145
#define IDG_DEBUG_CONTEXT_TOGGLE_SUBGROUP   0x0146
#define IDG_DEBUG_CONTEXT_STACKFRAME_SUBGROUP 0x0147
#define IDG_LOAD_SYMBOLS_DEFAULTS           0x0149
#define IDG_BREAKPOINTS_WINDOW_SET_FILTER   0x0151
#define IDG_BREAKPOINTS_WINDOW_SORT         0x0152
#define IDG_BREAKPOINTS_WINDOW_SORT_LIST    0x0153
#define IDG_BREAKPOINTS_WINDOW_SORT_DIRECTION 0x0154
#define IDG_BREAKPOINTS_WINDOW_SORT_NONE    0x0155
#define IDG_BREAKPOINTS_WINDOW_UNDO_REDO    0x0156
#define IDG_BREAKPOINTS_WINDOW_IMPORT_EXPORT 0x0157
#define IDG_BREAKPOINTS_WINDOW_EXPORT       0x0158
#define IDG_BREAKPOINT_EXPORT               0x0159
#define IDG_AUTOLOAD_SYMBOLS_DEFAULTS       0x0160
#define IDG_SYMBOLS_INCLUDELIST_DEFAULTS    0x0161
#define IDG_SYMBOLS_EXCLUDELIST_DEFAULTS    0x0162
#define IDG_DEBUGGER_OPTIONS                0x0163

#define IDG_WATCH_WINDOW_REAL_FUNC_EVAL    0x0164
#define IDG_WATCH_WINDOW_REAL_FUNC_EVAL1   0x0165
#define IDG_WATCH_WINDOW_REAL_FUNC_EVAL2   0x0166
#define IDG_WATCH_WINDOW_REAL_FUNC_EVAL3   0x0167
#define IDG_WATCH_WINDOW_REAL_FUNC_EVAL4   0x0168
#define IDG_LOCALS_WINDOW_REAL_FUNC_EVAL   0x0169
#define IDG_AUTOS_WINDOW_REAL_FUNC_EVAL    0x0170
#define IDG_WATCH_SYMBOLS                  0x0171
#define IDG_MODULES_WINDOW_TOOLBAR_FILTER  0x0172
#define IDG_VARIABLE_NAVIGATION            0x0173
#define IDG_OTHER_DEBUG_TARGETS_SUBMENU    0x0174
#define IDG_DATATIP_SYMBOLS                0x0175  

// Call out memory window instances separately for multiple toolbar support
#define IDG_MEMORY_EXPRESSION1          0x0201
#define IDG_MEMORY_EXPRESSION2          0x0202
#define IDG_MEMORY_EXPRESSION3          0x0203
#define IDG_MEMORY_EXPRESSION4          0x0204
#define IDG_MEMORY_COLUMNS1             0x0211
#define IDG_MEMORY_COLUMNS2             0x0212
#define IDG_MEMORY_COLUMNS3             0x0213
#define IDG_MEMORY_COLUMNS4             0x0214

#define IDG_MODULES_SYMBOLS_SETTINGS    0x0215
#define IDG_CALLSTACK_SYMBOLS_SETTINGS  0x0216
#define IDG_WATCH_SYMBOLS_SETTINGS      0x0217
#define IDG_DATATIP_SYMBOLS_SETTINGS    0x0218

#define IDG_VS_CODEWIN_DEBUG_BP         0x0230

// this group is for commands that are available in the command window,
// but are not on any menus by default
#define IDG_COMMAND_WINDOW              0x0300

#define IDG_INTELLITRACE_STEP           0x0400
#define IDG_INTELLITRACE_TOOLBAR_STEP   0x0401

///////////////////////////////////////////////////////////////////////////////
// Indexes into bitmaps (image wells)

//Instructions for adding new icons:
// First, see if the icon is already in the VS Image Catalog.
// If so, use it. If not, view the readme.txt file for vsimage
// service to find out how to add new images.

// DbgToolwindowIcons.bmp
#define IDBI_TW_THREADS                 1
#define IDBI_TW_RUNNING_DOCS            2
#define IDBI_TW_INSERT_BREAKPOINT       3
#define IDBI_TW_STACK                   4
#define IDBI_TW_LOCALS                  5
#define IDBI_TW_AUTOS                   6
#define IDBI_TW_DISASM                  7
#define IDBI_TW_MEMORY                  8
#define IDBI_TW_REGISTERS               9
#define IDBI_TW_WATCH                  10
#define IDBI_TW_MODULES                11
#define IDBI_TW_CONSOLE_WINDOW         12
#define IDBI_TW_PROCESSES              13

#define IDBI_MAX                       13


///////////////////////////////////////////////////////////////////////////////
// Command IDs
// Unfortunately several debugger cmdid's found in vs\src\common\inc\stdidcmd.h
// cannot be moved into this file for backward compatibility reasons.
// Otherwise, all V7 debugger cmdid's should be defined in here.

///////////////////////////////////////////////////////////////////////////////
// IMPORTANT: Our implementation of multiple-instance toolwindows make use of
// the high-end byte of the cmdid's to store instance information.  Do not use
// this byte unless you are implementing a multiple-instance toolwindow.
///////////////////////////////////////////////////////////////////////////////
#ifdef __cplusplus
inline DWORD DBGCMDID_STRIP(DWORD cmdid)
{
    return cmdid & 0x00ffffff;
}
inline long DBGCMDID_TOOLWINDOW_ID(DWORD cmdid)
{
    return (cmdid >> 24) & 0x000000ff;
}
#endif

// General debugger commands
#define cmdidDebuggerFirst                  0x00000000
#define cmdidDebuggerLast                   0x00000fff

#define cmdidBreakpointsWindowShow          0x00000100
#define cmdidDisasmWindowShow               0x00000101
#define cmdidRegisterWindowShow             0x00000103
#define cmdidModulesWindowShow              0x00000104
#define cmdidApplyCodeChanges               0x00000105
#define cmdidStopApplyCodeChanges           0x00000106
#define cmdidGoToDisassembly                0x00000107
#define cmdidShowDebugOutput                0x00000108
#define cmdidStepUnitLine                   0x00000110
#define cmdidStepUnitStatement              0x00000111
#define cmdidStepUnitInstruction            0x00000112
#define cmdidStepUnitList                   0x00000113
#define cmdidStepUnitListEnum               0x00000114
#define cmdidWriteCrashDump                 0x00000115
#define cmdidProcessList                    0x00000116
#define cmdidProcessListEnum                0x00000117
#define cmdidThreadList                     0x00000118
#define cmdidThreadListEnum                 0x00000119
#define cmdidStackFrameList                 0x00000120
#define cmdidStackFrameListEnum             0x00000121
#define cmdidDisableAllBreakpoints          0x00000122
#define cmdidEnableAllBreakpoints           0x00000123
#define cmdidToggleAllBreakpoints           0x00000124
#define cmdidTerminateAll                   0x00000125
#define cmdidSymbolOptions                  0x00000126
#define cmdidLoadSymbolsFromCurrentPath     0x00000127
#define cmdidSymbolLoadInfo                 0x00000128
#define cmdidStopEvaluatingExpression       0x00000129
#define cmdidAttachedProcsWindowShow        0x00000131 
#define cmdidToggleFlaggedThreads           0x00000132
#define cmdidThreadFlag                     0x00000133
#define cmdidThreadUnflag                   0x00000134
#define cmdidJustMyCode                     0x00000135
#define cmdidNewFunctionBreakpoint          0x00000137
#define cmdidNewAddressBreakpoint           0x00000138
#define cmdidNewDataBreakpoint              0x00000139 
#define cmdidProcessRefreshRequest          0x0000013A
#define cmdidThreadUnflagAll                0x00000040
#define cmdidInsertTracepoint               0x00000041
#define cmdidBreakpointSettings             0x0000013B
#define cmdidBreakpointSettingsCondition    0x00000140
#define cmdidBreakpointSettingsAction       0x00000141
#define cmdidBreakpointConstraints          0x00000145
#define cmdidCreateObjectID                 0x00000147
//#define cmdid is available                0x00000148
#define cmdidCopyExpression                 0x00000149
#define cmdidCopyValue                      0x0000014A
#define cmdidDestroyObjectID                0x0000014B
#define cmdidOutputOnException              0x00000150
#define cmdidOutputOnModuleLoad             0x00000151
#define cmdidOutputOnModuleUnload           0x00000152
#define cmdidOutputOnProcessDestroy         0x00000153
#define cmdidOutputOnThreadDestroy          0x00000154
#define cmdidOutputOnOutputDebugString      0x00000155
#define cmdidSingleProcStepInto             0x00000156
#define cmdidSingleProcStepOver             0x00000157
#define cmdidSingleProcStepOut              0x00000158
#define cmdidToggleCurrentThreadFlag        0x00000159
#define cmdidShowThreadIpIndicators         0x0000015A
#define cmdidLoadSymbolsFromPublic          0x0000015B
#define cmdidOutputOnStepFilter             0x0000015D
#define cmdidStepFilterToggle               0x0000015E
#define cmdidShowStepIntoSpecificMenu       0x0000015F
#define cmdidBreakpointEditLabels           0x00000160
#define cmdidBreakpointExport               0x00000161
#define cmdidAutoLoadSymbolsEnabled         0x00000163
#define cmdidAutoLoadSymbolsDisabled        0x00000164
#define cmdidAddWatchExpression             0x00000171
#define cmdidQuickWatchExpression           0x00000172
#define cmdidDebuggerOptions                0x00000173
#define cmdidRunThreadsToCursor             0x00000174
#define cmdidToggleShowCurrentProcessOnly   0x00000175
#define cmdidRunCurrentTileToCursor         0x00000176
#define cmdidAddParallelWatch	            0x00000179
#define cmdidExitDebuggerDeploymentBuild    0x0000017A
#define cmdidLaunchAppx                     0x0000017B
#define cmdidSymbolsIncludeExclude          0x0000017C
#define cmdidTriggerAppPrefetch             0x0000017D

// App package menu control
#define cmdidAppPackageAppsControl          0x0000017E
#define cmdidAppPackageAppsMenuGroupTargetAnchor 0x0000017F
#define cmdidAppPackageAppsMenuGroup        0x00000180
#define cmdidAppPackageAppsMenuGroupTarget  0x00000181
#define cmdidAppPackageAppsMenuGroupTargetLast   0x0000019F

// See above for explanation of these constants...
#define cmdidMemoryWindowShow               0x00000200
#define cmdidMemoryWindowShow1              0x01000200
#define cmdidMemoryWindowShow2              0x02000200
#define cmdidMemoryWindowShow3              0x03000200
#define cmdidMemoryWindowShow4              0x04000200

#ifdef MULTIPLE_WATCH_WINDOWS
#define cmdidWatchWindowShow                0x00000300
#define cmdidWatchWindowShow1               0x01000300
#define cmdidWatchWindowShow2               0x02000300
#define cmdidWatchWindowShow3               0x03000300
#define cmdidWatchWindowShow4               0x04000300
#define cmdidWatchWindowShowSingle          0x05000300
#endif

// Breakpoint Window commands
#define cmdidBreakpointsWindowFirst         0x00001000
#define cmdidBreakpointsWindowLast          0x00001fff

#define cmdidBreakpointsWindowNewBreakpoint   0x00001001 // deprecated
#define cmdidBreakpointsWindowNewGroup        0x00001002
#define cmdidBreakpointsWindowDelete          0x00001003
#define cmdidBreakpointsWindowProperties      0x00001004 // deprecated
#define cmdidBreakpointsWindowDefaultGroup    0x00001005
#define cmdidBreakpointsWindowGoToSource      0x00001006
#define cmdidBreakpointsWindowGoToDisassembly 0x00001007
#define cmdidBreakpointsWindowGoToBreakpoint  0x00001008
#define cmdidBreakpointsWindowSetFilter             0x00001009
#define cmdidBreakpointsWindowSetFilterList         0x0000100A
#define cmdidBreakpointsWindowSetFilterDropDown     0x0000100B
#define cmdidBreakpointsWindowSetFilterDropDownList 0x0000100C
#define cmdidBreakpointsWindowImport            0x0000100D
#define cmdidBreakpointsWindowUndo              0x0000100E
#define cmdidBreakpointsWindowRedo              0x0000100F
#define cmdidBreakpointsWindowExport            0x00001010
#define cmdidBreakpointsWindowExportSelected    0x00001011
#define cmdidBreakpointsWindowClearSearchFilter       0x00001013
#define cmdidBreakpointsWindowDeleteAllMatching        0x00001014
#define cmdidBreakpointsWindowToggleAllMatching        0x00001015
#define cmdidBreakpointsWindowSortByColumnName  0x00001200
#define cmdidBreakpointsWindowSortByColumnCondition   0x00001201
#define cmdidBreakpointsWindowSortByColumnHitCount    0x00001202
#define cmdidBreakpointsWindowSortByColumnLanguage    0x00001203
#define cmdidBreakpointsWindowSortByColumnFunction    0x00001204
#define cmdidBreakpointsWindowSortByColumnFile        0x00001205
#define cmdidBreakpointsWindowSortByColumnAddress     0x00001206
#define cmdidBreakpointsWindowSortByColumnData        0x00001207
#define cmdidBreakpointsWindowSortByColumnProcess     0x00001208
#define cmdidBreakpointsWindowSortByColumnConstraints 0x00001209
#define cmdidBreakpointsWindowSortByColumnAction      0x0000120A
#define cmdidBreakpointsWindowSortByColumnLabel       0x0000120B
#define cmdidBreakpointsWindowSortByNone              0x0000120C
#define cmdidBreakpointsWindowSortAscending           0x0000120D
#define cmdidBreakpointsWindowSortDescending          0x0000120E


#define cmdidBreakpointsWindowColumnName        0x00001100
#define cmdidBreakpointsWindowColumnCondition   0x00001101
#define cmdidBreakpointsWindowColumnHitCount    0x00001102
#define cmdidBreakpointsWindowColumnLanguage    0x00001103
#define cmdidBreakpointsWindowColumnFunction    0x00001104
#define cmdidBreakpointsWindowColumnFile        0x00001105
#define cmdidBreakpointsWindowColumnAddress     0x00001106
#define cmdidBreakpointsWindowColumnData        0x00001107
#define cmdidBreakpointsWindowColumnProcess     0x00001108
#define cmdidBreakpointsWindowColumnConstraints 0x00001109
#define cmdidBreakpointsWindowColumnAction      0x0000110A
#define cmdidBreakpointsWindowColumnLabel       0x0000110B



// Disassembly Window commands
#define cmdidDisasmWindowFirst              0x00002000
#define cmdidDisasmWindowLast               0x00002fff

#define cmdidGoToSource                     0x00002001
#define cmdidShowDisasmAddress              0x00002002
#define cmdidShowDisasmSource               0x00002003
#define cmdidShowDisasmCodeBytes            0x00002004
#define cmdidShowDisasmSymbolNames          0x00002005
#define cmdidShowDisasmLineNumbers          0x00002006
#define cmdidShowDisasmToolbar              0x00002007
#define cmdidDisasmExpression               0x00002008
#define cmdidToggleDisassembly              0x00002009

// Memory Window commands
#define cmdidMemoryWindowFirst              0x00003000
#define cmdidMemoryWindowLast               0x00003fff

// The following are specific to each instance of the memory window.  The high-end
// byte is critical for proper operation of these commands.  The high-byte indicates
// the particular toolwindow that this cmdid applies to.  You can change the
// lowest 3 bytes to be whatever you want.

// The first constant in each group marks a cmdid representing the entire group.
// We use this constant inside our switch statements so as to not have to list
// out each separate instance of cmdid.
#define cmdidMemoryExpression               0x00003001
#define cmdidMemoryExpression1              0x01003001
#define cmdidMemoryExpression2              0x02003001
#define cmdidMemoryExpression3              0x03003001
#define cmdidMemoryExpression4              0x04003001

#define cmdidAutoReevaluate                 0x00003002
#define cmdidAutoReevaluate1                0x01003002
#define cmdidAutoReevaluate2                0x02003002
#define cmdidAutoReevaluate3                0x03003002
#define cmdidAutoReevaluate4                0x04003002

#define cmdidMemoryColumns                  0x00003003
#define cmdidMemoryColumns1                 0x01003003
#define cmdidMemoryColumns2                 0x02003003
#define cmdidMemoryColumns3                 0x03003003
#define cmdidMemoryColumns4                 0x04003003

#define cmdidColCountList                   0x00003004
#define cmdidColCountList1                  0x01003004
#define cmdidColCountList2                  0x02003004
#define cmdidColCountList3                  0x03003004
#define cmdidColCountList4                  0x04003004

#define cmdidWatchRealFuncEvalFirst         0x0000e001
#define cmdidWatchRealFuncEvalLast          0x0000e001

#define cmdidWatchRealFuncEval              0x0000e001
#define cmdidWatchRealFuncEval1             0x0100e001
#define cmdidWatchRealFuncEval2             0x0200e001
#define cmdidWatchRealFuncEval3             0x0300e001
#define cmdidWatchRealFuncEval4             0x0400e001

#define cmdidAutosRealFuncEvalFirst         0x0000e005
#define cmdidAutosRealFuncEvalLast          0x0000e005

#define cmdidAutosRealFuncEval              0x0000e005

#define cmdidLocalsRealFuncEvalFirst        0x0000e006
#define cmdidLocalsRealFuncEvalLast         0x0000e006

#define cmdidLocalsRealFuncEval             0x0000e006


// The following apply to all instances of the memory windows.  If any of these
// are added to the toolbar, they must be made per-instance!
#define cmdidShowNoData                     0x00003011
#define cmdidOneByteInt                     0x00003012
#define cmdidTwoByteInt                     0x00003013
#define cmdidFourByteInt                    0x00003014
#define cmdidEightByteInt                   0x00003015
#define cmdidFloat                          0x00003020
#define cmdidDouble                         0x00003021
#define cmdidFormatHex                      0x00003030
#define cmdidFormatSigned                   0x00003031
#define cmdidFormatUnsigned                 0x00003032
#define cmdidFormatBigEndian                0x00003033
#define cmdidShowNoText                     0x00003040
#define cmdidShowAnsiText                   0x00003041
#define cmdidShowUnicodeText                0x00003042
#define cmdidEditValue                      0x00003050
#define cmdidShowToolbar                    0x00003062

// MemoryView-specific commands.  These are used internally by the MemoryView implementation.
#define cmdidStopInPlaceEdit                0x00003100

// Registers Window commands
#define cmdidRegisterWindowFirst            0x00004000
#define cmdidRegWinGroupFirst               0x00004001
#define cmdidRegWinGroupLast                0x00004100

#define cmdidRegisterWindowLast             0x00004fff

// QuickWatch commands
#define cmdidQuickWatchFirst                0x00005000
#define cmdidQuickWatchLast                 0x00005fff


// Debug Context toolbar commands
//#define cmdidDebugContextFirst              0x00006000
//#define cmdidDebugContextLast               0x00006fff


// Modules Window commands
#define cmdidModulesWindowFirst             0x00007000
#define cmdidModulesWindowLast              0x00007100

#define cmdidReloadSymbols                  0x00007001 // deprecated
#define cmdidShowAllModules                 0x00007002
#define cmdidToggleUserCode                 0x00007003

#define cmdidModulesWindowFilter            0x00007004
#define cmdidModulesWindowFilterList        0x00007005
#define cmdidModulesWindowClearSearchFilter 0x00007006

// step into specific
#define cmdidStepIntoSpecificFirst          0x00007200
#define cmdidStepIntoSpecificMaxDisplay     0x00007231
// This is currently unused, but the entire range was previously
// used for step into specific, so leaving it in to maintain that range.
#define cmdidStepIntoSpecificLast           0x00007FFF

// Call Stack commands
#define cmdidCallStackWindowFirst           0x00008000
#define cmdidCallStackWindowLast            0x00008fff

#define cmdidSetCurrentFrame                0x00008001
#define cmdidCallStackValues                0x00008002
#define cmdidCallStackTypes                 0x00008003
#define cmdidCallStackNames                 0x00008004
#define cmdidCallStackModules               0x00008005
#define cmdidCallStackLineOffset            0x00008006
#define cmdidCallStackByteOffset            0x00008007
#define cmdidCrossThreadCallStack           0x00008008
#define cmdidShowExternalCode               0x00008009
#define cmdidUnwindFromException            0x0000800a
#define cmdidCallstackShowFrameType         0x0000800b

// Datatip commands
#define cmdidDatatipFirst                   0x00009000
#define cmdidDatatipLast                    0x00009fff

#define cmdidDatatipNoTransparency          0x00009010
#define cmdidDatatipLowTransparency         0x00009011
#define cmdidDatatipMedTransparency         0x00009012
#define cmdidDatatipHighTransparency        0x00009013

// Attached Processes Window commands
#define cmdidAttachedProcsWindowFirst               0x0000a000
#define cmdidAttachedProcsWindowLast                0x0000a100

#define cmdidAttachedProcsStartProcess              0x0000a001
#define cmdidAttachedProcsPauseProcess              0x0000a002
#define cmdidAttachedProcsStepIntoProcess           0x0000a003
#define cmdidAttachedProcsStepOverProcess           0x0000a004
#define cmdidAttachedProcsStepOutProcess            0x0000a005
#define cmdidAttachedProcsDetachProcess             0x0000a006
#define cmdidAttachedProcsTerminateProcess          0x0000a007
#define cmdidAttachedProcsDetachOnStop              0x0000a008
#define cmdidAttachedProcsColumnName                0x0000a010
#define cmdidAttachedProcsColumnID                  0x0000a011
#define cmdidAttachedProcsColumnPath                0x0000a012
#define cmdidAttachedProcsColumnTitle               0x0000a013
#define cmdidAttachedProcsColumnMachine             0x0000a014
#define cmdidAttachedProcsColumnState               0x0000a015
#define cmdidAttachedProcsColumnTransport           0x0000a016
#define cmdidAttachedProcsColumnTransportQualifier  0x0000a017

#define cmdidThreadIpMarkerSwitchContext            0x0000a018
#define cmdidThreadIpMarkerFlagUnflag               0x0000a019
#define cmdidThreadIpMarkersSwitchContext           0x0000b000
#define cmdidThreadIpMarkersSwitchContextFirst      0x0000b001
#define cmdidThreadIpMarkersSwitchContextLast       0x0000bfff
#define cmdidThreadIpMarkersFlag                    0x0000c000
#define cmdidThreadIpMarkersFlagFirst               0x0000c001
#define cmdidThreadIpMarkersFlagLast                0x0000cfff
#define cmdidThreadIpMarkersUnflag                  0x0000d000
#define cmdidThreadIpMarkersUnflagFirst             0x0000d001
#define cmdidThreadIpMarkersUnflagLast              0x0000dfff
#define cmdidAppPrelaunch                           0x00000219
#define cmdidDebugForAccessibility                  0x00000220


// Command Window commands
// while all commands are available in the command window,
// these are not on any menus by default
//
#define cmdidCommandWindowFirst             0x0000f000
#define cmdidCommandWindowLast              0x0000ffff

#define cmdidListMemory                     0x0000f001
#define cmdidListCallStack                  0x0000f002
#define cmdidListDisassembly                0x0000f003
#define cmdidListRegisters                  0x0000f004 
// unused                                   0x0000f005
#define cmdidListThreads                    0x0000f006
#define cmdidSetRadix                       0x0000f007 
// unused                                   0x0000f008
#define cmdidSetCurrentThread               0x0000f009
#define cmdidSetCurrentStackFrame           0x0000f00a
#define cmdidListSource                     0x0000f00b
#define cmdidSymbolPath                     0x0000f00c
#define cmdidListModules                    0x0000f00d
#define cmdidListProcesses                  0x0000f00e
#define cmdidSetCurrentProcess              0x0000f00f

#define guidSuspendAppPackageAppIcon  { 0xb203ce85, 0x9889, 0x4b2e, { 0x81, 0xea, 0x18, 0xec, 0x9a, 0xd0, 0x85, 0xa2 } }
#define guidResumeAppPackageAppIcon   { 0xfa813ed0, 0xbb98, 0x4a0e, { 0x9c, 0x27, 0x31, 0xc1, 0xab, 0xd7, 0xa7, 0x97 } }
#define guidShutDownAppPackageAppIcon { 0x6edd202e, 0x1c6, 0x4a4a, { 0xab, 0x1a, 0x48, 0x56, 0xff, 0xc4, 0x9a, 0x3e } }

#define cmdidReattach                       0x0000f010

#endif // _VSDBGCMD_H_INCLUDED
