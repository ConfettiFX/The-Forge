//-----------------------------------------------------------------------------
// Microsoft Visual Studio
//
// Copyright 1995-2003 Microsoft Corporation.  All Rights Reserved.
//
// File: stdidcmd.h
// Area: IOleCommandTarget and IOleComponentUIManager
//
// Contents:
//   Contains ids used for commands used in StandardCommandSet97.
//   StandardCommandSet97 is defined by the following guid:
//
//   {5efc7975-14bc-11cf-9b2b-00aa00573819}
//   DEFINE_GUID(CLSID_StandardCommandSet97,
//               0x5efc7975, 0x14bc, 0x11cf, 0x9b, 0x2b, 0x00, 0xaa, 0x00,
//               0x57, 0x38, 0x19);
//
//   Contains ids used for commands used in StandardCommandSet2K.
//   StandardCommandSet2K is defined by the following guid:
//
//   {1496A755-94DE-11D0-8C3F-00C04FC2AAE2}
//  DEFINE_GUID(CMDSETID_StandardCommandSet2K,
//    0x1496A755, 0x94DE, 0x11D0, 0x8C, 0x3F, 0x00, 0xC0, 0x4F, 0xC2, 0xAA, 0xE2);
//
//
//   Contains ids used for commands used in StandardCommandSet10.
//   StandardCommandSet10 is defined by the following guid:
//
//   {5DD0BB59-7076-4c59-88D3-DE36931F63F0}
//  DEFINE_GUID(CMDSETID_StandardCommandSet10, 
//    0x5dd0bb59, 0x7076, 0x4c59, 0x88, 0xd3, 0xde, 0x36, 0x93, 0x1f, 0x63, 0xf0);
//
//
//   Contains ids used for commands used in StandardCommandSet11.
//   StandardCommandSet11 is defined by the following guid:
//
//   {D63DB1F0-404E-4B21-9648-CA8D99245EC3}
//  DEFINE_GUID(CMDSETID_StandardCommandSet11, 
//    0xd63db1f0, 0x404e, 0x4b21, 0x96, 0x48, 0xca, 0x8d, 0x99, 0x24, 0x5e, 0xc3);
//
//   Contains ids used for commands used in StandardCommandSet12.
//   StandardCommandSet12 is defined by the following guid:
//
//   {2A8866DC-7BDE-4dc8-A360-A60679534384}
//  DEFINE_GUID(CMDSETID_StandardCommandSet12,
//    0x2A8866DC, 0x7BDE, 0x4dc8, 0xA3, 0x60, 0xA6, 0x06, 0x79, 0x53, 0x43, 0x84);
//
//   Contains ids used for commands used in StandardCommandSet14.
//   StandardCommandSet14 is defined by the following guid:
//
//   {4C7763BF-5FAF-4264-A366-B7E1F27BA958}
//  DEFINE_GUID(CMDSETID_StandardCommandSet14,
//    0x4c7763bf, 0x5faf, 0x4264, 0xa3, 0x66, 0xb7, 0xe1, 0xf2, 0x7b, 0xa9, 0x58);
//
//   {712C6C80-883B-4AAD-B430-BBCA5256FA9D}
//  DEFINE_GUID(CMDSETID_StandardCommandSet15,
//    0x712c6c80, 0x883b, 0x4aad, 0xb4, 0x30, 0xbb, 0xca, 0x52, 0x56, 0xfa, 0x9d);
//
//  NOTE that new commands should be added to the end of StandardCommandSet2K
//  and that CLSID_StandardCommandSet97 should not be further added to.
//  NOTE also that in StandardCommandSet2K all commands up to ECMD_FINAL are
//  standard editor commands and have been moved here from editcmd.h.
//-----------------------------------------------------------------------------

#ifndef _STDIDCMD_H_
#define _STDIDCMD_H_

#ifndef __CTC__
#ifdef __cplusplus

// for specialized contracts
enum
{
    CMD_ZOOM_PAGEWIDTH     = -1,
    CMD_ZOOM_ONEPAGE       = -2,
    CMD_ZOOM_TWOPAGES      = -3,
    CMD_ZOOM_SELECTION     = -4,
    CMD_ZOOM_FIT           = -5
};

#endif //__cplusplus
#endif //__CTC__

#define cmdidAlignBottom        1
#define cmdidAlignHorizontalCenters     2
#define cmdidAlignLeft          3
#define cmdidAlignRight         4
#define cmdidAlignToGrid        5
#define cmdidAlignTop           6
#define cmdidAlignVerticalCenters       7
#define cmdidArrangeBottom      8
#define cmdidArrangeRight       9
#define cmdidBringForward       10
#define cmdidBringToFront       11
#define cmdidCenterHorizontally         12
#define cmdidCenterVertically           13
#define cmdidCode           14
#define cmdidCopy           15
#define cmdidCut            16
#define cmdidDelete         17
#define cmdidFontName           18
#define cmdidFontNameGetList            500
#define cmdidFontSize           19
#define cmdidFontSizeGetList            501
#define cmdidGroup          20
#define cmdidHorizSpaceConcatenate      21
#define cmdidHorizSpaceDecrease         22
#define cmdidHorizSpaceIncrease         23
#define cmdidHorizSpaceMakeEqual        24
#define cmdidLockControls               369
#define cmdidInsertObject       25
#define cmdidPaste          26
#define cmdidPrint          27
#define cmdidProperties         28
#define cmdidRedo           29
#define cmdidMultiLevelRedo     30
#define cmdidSelectAll          31
#define cmdidSendBackward       32
#define cmdidSendToBack         33
#define cmdidShowTable          34
#define cmdidSizeToControl      35
#define cmdidSizeToControlHeight        36
#define cmdidSizeToControlWidth         37
#define cmdidSizeToFit          38
#define cmdidSizeToGrid         39
#define cmdidSnapToGrid         40
#define cmdidTabOrder           41
#define cmdidToolbox            42
#define cmdidUndo           43
#define cmdidMultiLevelUndo     44
#define cmdidUngroup            45
#define cmdidVertSpaceConcatenate       46
#define cmdidVertSpaceDecrease          47
#define cmdidVertSpaceIncrease          48
#define cmdidVertSpaceMakeEqual         49
#define cmdidZoomPercent        50
#define cmdidBackColor          51
#define cmdidBold           52
#define cmdidBorderColor        53
#define cmdidBorderDashDot      54
#define cmdidBorderDashDotDot           55
#define cmdidBorderDashes       56
#define cmdidBorderDots         57
#define cmdidBorderShortDashes          58
#define cmdidBorderSolid        59
#define cmdidBorderSparseDots           60
#define cmdidBorderWidth1       61
#define cmdidBorderWidth2       62
#define cmdidBorderWidth3       63
#define cmdidBorderWidth4       64
#define cmdidBorderWidth5       65
#define cmdidBorderWidth6       66
#define cmdidBorderWidthHairline        67
#define cmdidFlat           68
#define cmdidForeColor          69
#define cmdidItalic         70
#define cmdidJustifyCenter      71
#define cmdidJustifyGeneral     72
#define cmdidJustifyLeft        73
#define cmdidJustifyRight       74
#define cmdidRaised         75
#define cmdidSunken         76
#define cmdidUnderline          77
#define cmdidChiseled           78
#define cmdidEtched         79
#define cmdidShadowed           80
#define cmdidCompDebug1         81
#define cmdidCompDebug2         82
#define cmdidCompDebug3         83
#define cmdidCompDebug4         84
#define cmdidCompDebug5         85
#define cmdidCompDebug6         86
#define cmdidCompDebug7         87
#define cmdidCompDebug8         88
#define cmdidCompDebug9         89
#define cmdidCompDebug10        90
#define cmdidCompDebug11        91
#define cmdidCompDebug12        92
#define cmdidCompDebug13        93
#define cmdidCompDebug14        94
#define cmdidCompDebug15        95
#define cmdidExistingSchemaEdit         96
#define cmdidFind           97
#define cmdidGetZoom            98
#define cmdidQueryOpenDesign        99
#define cmdidQueryOpenNew       100
#define cmdidSingleTableDesign          101
#define cmdidSingleTableNew     102
#define cmdidShowGrid           103
#define cmdidNewTable           104
#define cmdidCollapsedView      105
#define cmdidFieldView          106
#define cmdidVerifySQL          107
#define cmdidHideTable          108

#define cmdidPrimaryKey         109
#define cmdidSave           110
#define cmdidSaveAs         111
#define cmdidSortAscending      112

#define cmdidSortDescending     113
#define cmdidAppendQuery        114
#define cmdidCrosstabQuery      115
#define cmdidDeleteQuery        116
#define cmdidMakeTableQuery     117

#define cmdidSelectQuery        118
#define cmdidUpdateQuery        119
#define cmdidParameters         120
#define cmdidTotals         121
#define cmdidViewCollapsed      122

#define cmdidViewFieldList      123


#define cmdidViewKeys           124
#define cmdidViewGrid           125
#define cmdidInnerJoin          126

#define cmdidRightOuterJoin     127
#define cmdidLeftOuterJoin      128
#define cmdidFullOuterJoin      129
#define cmdidUnionJoin          130
#define cmdidShowSQLPane        131

#define cmdidShowGraphicalPane          132
#define cmdidShowDataPane       133
#define cmdidShowQBEPane        134
#define cmdidSelectAllFields        135

#define cmdidOLEObjectMenuButton        136

// ids on the ole verbs menu - these must be sequential ie verblist0-verblist9
#define cmdidObjectVerbList0        137
#define cmdidObjectVerbList1        138
#define cmdidObjectVerbList2        139
#define cmdidObjectVerbList3        140
#define cmdidObjectVerbList4        141
#define cmdidObjectVerbList5        142
#define cmdidObjectVerbList6        143
#define cmdidObjectVerbList7        144
#define cmdidObjectVerbList8        145
#define cmdidObjectVerbList9        146 // Unused on purpose!

#define cmdidConvertObject      147
#define cmdidCustomControl      148
#define cmdidCustomizeItem      149
#define cmdidRename         150

#define cmdidImport         151
#define cmdidNewPage            152
#define cmdidMove           153
#define cmdidCancel         154

#define cmdidFont           155

#define cmdidExpandLinks        156
#define cmdidExpandImages       157
#define cmdidExpandPages        158
#define cmdidRefocusDiagram         159
#define cmdidTransitiveClosure      160
#define cmdidCenterDiagram      161
#define cmdidZoomIn             162
#define cmdidZoomOut            163
#define cmdidRemoveFilter       164
#define cmdidHidePane           165
#define cmdidDeleteTable        166
#define cmdidDeleteRelationship     167
#define cmdidRemove             168
#define cmdidJoinLeftAll        169
#define cmdidJoinRightAll       170
#define cmdidAddToOutput        171     // Add selected fields to query output
#define cmdidOtherQuery         172     // change query type to 'other'
#define cmdidGenerateChangeScript   173
#define cmdidSaveSelection      174 // Save current selection
#define cmdidAutojoinCurrent        175 // Autojoin current tables
#define cmdidAutojoinAlways         176 // Toggle Autojoin state
#define cmdidEditPage           177 // Launch editor for url
#define cmdidViewLinks          178 // Launch new webscope for url
#define cmdidStop           179 // Stope webscope rendering
#define cmdidPause          180 // Pause webscope rendering
#define cmdidResume         181 // Resume webscope rendering
#define cmdidFilterDiagram      182 // Filter webscope diagram
#define cmdidShowAllObjects     183 // Show All objects in webscope diagram
#define cmdidShowApplications       184 // Show Application objects in webscope diagram
#define cmdidShowOtherObjects       185 // Show other objects in webscope diagram
#define cmdidShowPrimRelationships  186 // Show primary relationships
#define cmdidExpand         187 // Expand links
#define cmdidCollapse           188 // Collapse links
#define cmdidRefresh            189 // Refresh Webscope diagram
#define cmdidLayout         190 // Layout websope diagram
#define cmdidShowResources      191 // Show resouce objects in webscope diagram
#define cmdidInsertHTMLWizard       192 // Insert HTML using a Wizard
#define cmdidShowDownloads      193 // Show download objects in webscope diagram
#define cmdidShowExternals      194 // Show external objects in webscope diagram
#define cmdidShowInBoundLinks       195 // Show inbound links in webscope diagram
#define cmdidShowOutBoundLinks      196 // Show out bound links in webscope diagram
#define cmdidShowInAndOutBoundLinks 197 // Show in and out bound links in webscope diagram
#define cmdidPreview            198 // Preview page
#define cmdidOpen           261 // Open
#define cmdidOpenWith           199 // Open with
#define cmdidShowPages          200 // Show HTML pages
#define cmdidRunQuery           201     // Runs a query
#define cmdidClearQuery         202     // Clears the query's associated cursor
#define cmdidRecordFirst        203     // Go to first record in set
#define cmdidRecordLast         204     // Go to last record in set
#define cmdidRecordNext         205     // Go to next record in set
#define cmdidRecordPrevious         206     // Go to previous record in set
#define cmdidRecordGoto         207     // Go to record via dialog
#define cmdidRecordNew          208     // Add a record to set

#define cmdidInsertNewMenu      209 // menu designer
#define cmdidInsertSeparator        210 // menu designer
#define cmdidEditMenuNames      211 // menu designer

#define cmdidDebugExplorer      212
#define cmdidDebugProcesses     213
#define cmdidViewThreadsWindow      214
#define cmdidWindowUIList       215

// ids on the file menu
#define cmdidNewProject         216
#define cmdidOpenProject        217
#define cmdidOpenSolution       218
#define cmdidCloseSolution      219
#define cmdidFileNew            221
#define cmdidNewProjectFromExisting 385
#define cmdidFileOpen           222
#define cmdidFileOpenFromWeb        451
#define cmdidFileClose          223
#define cmdidSaveSolution       224
#define cmdidSaveSolutionAs     225
#define cmdidSaveProjectItemAs      226
#define cmdidPageSetup          227
#define cmdidPrintPreview       228
#define cmdidExit           229

// ids on the edit menu
#define cmdidReplace            230
#define cmdidGoto           231

// ids on the view menu
#define cmdidPropertyPages      232
#define cmdidFullScreen         233
#define cmdidProjectExplorer        234
#define cmdidPropertiesWindow       235
#define cmdidTaskListWindow     236
//#define cmdidErrorListWindow    320 // defined below
#define cmdidOutputWindow       237
#define cmdidObjectBrowser      238
#define cmdidDocOutlineWindow   239
#define cmdidImmediateWindow    240
#define cmdidWatchWindow        241
#define cmdidLocalsWindow       242
#define cmdidCallStack          243
// moved below definition
//#define cmdidAutosWindow        cmdidDebugReserved1
//#define cmdidThisWindow         cmdidDebugReserved2

// ids on project menu
#define cmdidAddNewItem         220
#define cmdidAddExistingItem        244
#define cmdidNewFolder          245
#define cmdidSetStartupProject      246
#define cmdidProjectSettings        247
#define cmdidProjectReferences          367

// ids on the debug menu
#define cmdidStepInto           248
#define cmdidStepOver           249
#define cmdidStepOut            250
#define cmdidRunToCursor        251
#define cmdidAddWatch           252
#define cmdidEditWatch          253
#define cmdidQuickWatch                 254

#define cmdidToggleBreakpoint       255
#define cmdidClearBreakpoints       256
#define cmdidShowBreakpoints        257
#define cmdidSetNextStatement       258
#define cmdidShowNextStatement      259
#define cmdidEditBreakpoint             260
#define cmdidDetachDebugger             262

// ids on the tools menu
#define cmdidCustomizeKeyboard      263
#define cmdidToolsOptions       264

// ids on the windows menu
#define cmdidNewWindow          265
#define cmdidSplit          266
#define cmdidCascade            267
#define cmdidTileHorz           268
#define cmdidTileVert           269

// ids on the help menu
#define cmdidTechSupport        270
// #define cmdidDebugContextWindow 327 // defined below

// NOTE cmdidAbout and cmdidDebugOptions must be consecutive
//      cmd after cmdidDebugOptions (ie 273) must not be used
#define cmdidAbout          271
#define cmdidDebugOptions       272

// ids on the watch context menu
// CollapseWatch appears as 'Collapse Parent', on any
// non-top-level item
#define cmdidDeleteWatch        274
#define cmdidCollapseWatch      275
// ids 276, 277, 278, 279, 280 are in use
// below
// ids on the property browser context menu
#define cmdidPbrsToggleStatus       282
#define cmdidPropbrsHide        283

// ids on the docking context menu
#define cmdidDockingView        284
#define cmdidHideActivePane     285
// ids for window selection via keyboard
//#define cmdidPaneNextPane     316 (listed below in order)
//#define cmdidPanePrevPane     317 (listed below in order)
#define cmdidPaneNextTab        286
#define cmdidPanePrevTab        287
#define cmdidPaneCloseToolWindow    288
#define cmdidPaneActivateDocWindow  289
#define cmdidDockingViewDocument    290
#define cmdidDockingViewFloater     291
#define cmdidAutoHideWindow     292
#define cmdidMoveToDropdownBar          293
#define cmdidFindCmd                    294 // internal Find commands
#define cmdidStart          295
#define cmdidRestart            296

#define cmdidMultiLevelUndoList     298
#define cmdidMultiLevelRedoList     299

#define cmdidToolboxAddTab      300
#define cmdidToolboxDeleteTab       301
#define cmdidToolboxRenameTab       302
#define cmdidToolboxTabMoveUp       303
#define cmdidToolboxTabMoveDown     304
#define cmdidToolboxRenameItem      305
#define cmdidToolboxListView        306
//(below) cmdidSearchSetCombo       307

#define cmdidWindowUIGetList        308
#define cmdidInsertValuesQuery      309

#define cmdidShowProperties     310

#define cmdidThreadSuspend      311
#define cmdidThreadResume       312
#define cmdidThreadSetFocus     313
#define cmdidDisplayRadix       314

#define cmdidOpenProjectItem        315

#define cmdidPaneNextPane       316
#define cmdidPanePrevPane       317

#define cmdidClearPane          318
#define cmdidGotoErrorTag       319

#define cmdidErrorListWindow    320

#define cmdidCancelEZDrag               326

#define cmdidDebugContextWindow 327

// Samples are no longer supported post d15
// #define cmdidHelpSamples        328

#define cmdidToolboxAddItem     329
#define cmdidToolboxReset       330

#define cmdidSaveProjectItem        331
#define cmdidSaveOptions                959
#define cmdidViewForm           332
#define cmdidViewCode           333
#define cmdidPreviewInBrowser       334
#define cmdidBrowseWith         336
#define cmdidSearchSetCombo     307
#define cmdidSearchCombo        337
#define cmdidEditLabel          338
#define cmdidExceptions         339
// UNUSED 340

#define cmdidToggleSelMode      341
#define cmdidToggleInsMode      342

#define cmdidLoadUnloadedProject    343
#define cmdidUnloadLoadedProject    344

// ids on the treegrids (watch/local/threads/stack)
#define cmdidElasticColumn  345
#define cmdidHideColumn         346

#define cmdidToggleDesigner 347

#define cmdidZoomDialog         348

// find/replace options
#define cmdidFindInSelection            354
#define cmdidFindStop                   355
#define cmdidFindInFiles                277
#define cmdidReplaceInFiles             278
#define cmdidNextLocation               279 // next item in task list, results lists, etc.
#define cmdidPreviousLocation           280 // prev item "
#define cmdidGotoQuick                  281
#define cmdidGotoFIF                    282
#define cmdidGotoSymbol                 283
#define cmdidGotoQuickReplace           285
#define cmdidGotoReplaceInFiles         286

// UNUSED: 356 - 366

// 367 is used above in cmdidProjectReferences
#define cmdidStartNoDebug       368
// 369 is used above in cmdidLockControls

#define cmdidFindNext                   370
#define cmdidFindPrev                   371
#define cmdidFindSelectedNext           372
#define cmdidFindSelectedPrev           373
#define cmdidSearchGetList              374
#define cmdidInsertBreakpoint       375
#define cmdidEnableBreakpoint       376
#define cmdidF1Help         377

// UNUSED: 378 - 383

#define cmdidMoveToNextEZCntr           384
// 385 is used above in cmdidNewProjectFromExisting
#define cmdidUpdateMarkerSpans          386

//UNUSED 387-392


#define cmdidMoveToPreviousEZCntr       393

//UNUSED 394-395

#define cmdidProjectProperties          396
#define cmdidPropSheetOrProperties  397

// NOTE - the next items are debug only !!
#define cmdidTshellStep                 398
#define cmdidTshellRun                  399

// marker commands on the codewin menu
#define cmdidMarkerCmd0                 400
#define cmdidMarkerCmd1                 401
#define cmdidMarkerCmd2                 402
#define cmdidMarkerCmd3                 403
#define cmdidMarkerCmd4                 404
#define cmdidMarkerCmd5                 405
#define cmdidMarkerCmd6                 406
#define cmdidMarkerCmd7                 407
#define cmdidMarkerCmd8                 408
#define cmdidMarkerCmd9                 409
#define cmdidMarkerLast                 409
#define cmdidMarkerEnd                  410 // list terminator reserved

// user-invoked project reload and unload
#define cmdidReloadProject              412
#define cmdidUnloadProject              413

#define cmdidNewBlankSolution           414
#define cmdidSelectProjectTemplate      415

// document outline commands
#define cmdidDetachAttachOutline        420
#define cmdidShowHideOutline            421
#define cmdidSyncOutline                422

#define cmdidRunToCallstCursor          423
#define cmdidNoCmdsAvailable        424

#define cmdidContextWindow              427
#define cmdidAlias          428
#define cmdidGotoCommandLine        429
#define cmdidEvaluateExpression     430
#define cmdidImmediateMode      431
#define cmdidEvaluateStatement      432

#define cmdidFindResultWindow1  433
#define cmdidFindResultWindow2  434

#define cmdidRenameBookmark 559
#define cmdidToggleBookmark 560
#define cmdidDeleteBookmark 561
#define cmdidBookmarkWindowGoToBookmark 562
//563 unused
#define cmdidEnableBookmark 564
#define cmdidNewBookmarkFolder 565
//566 unused
//567 unused
#define cmdidNextBookmarkFolder 568
#define cmdidPrevBookmarkFolder 569

// 500 is used above in cmdidFontNameGetList
// 501 is used above in cmdidFontSizeGetList

// ids on the window menu - these must be sequential ie window1-morewind
#define cmdidWindow1            570
#define cmdidWindow2            571
#define cmdidWindow3            572
#define cmdidWindow4            573
#define cmdidWindow5            574
#define cmdidWindow6            575
#define cmdidWindow7            576
#define cmdidWindow8            577
#define cmdidWindow9            578
#define cmdidWindow10           579
#define cmdidWindow11           580
#define cmdidWindow12           581
#define cmdidWindow13           582
#define cmdidWindow14           583
#define cmdidWindow15           584
#define cmdidWindow16           585
#define cmdidWindow17           586
#define cmdidWindow18           587
#define cmdidWindow19           588
#define cmdidWindow20           589
#define cmdidWindow21           590
#define cmdidWindow22           591
#define cmdidWindow23           592
#define cmdidWindow24           593
#define cmdidWindow25           594   // note cmdidWindow25 is unused on purpose!
#define cmdidMoreWindows        595

#define cmdidAutoHideAllWindows     597

// UNUSED: 598

#define cmdidClassView          599

#define cmdidMRUProj1           600
#define cmdidMRUProj2           601
#define cmdidMRUProj3           602
#define cmdidMRUProj4           603
#define cmdidMRUProj5           604
#define cmdidMRUProj6           605
#define cmdidMRUProj7           606
#define cmdidMRUProj8           607
#define cmdidMRUProj9           608
#define cmdidMRUProj10          609
#define cmdidMRUProj11          610
#define cmdidMRUProj12          611
#define cmdidMRUProj13          612
#define cmdidMRUProj14          613
#define cmdidMRUProj15          614
#define cmdidMRUProj16          615
#define cmdidMRUProj17          616
#define cmdidMRUProj18          617
#define cmdidMRUProj19          618
#define cmdidMRUProj20          619
#define cmdidMRUProj21          620
#define cmdidMRUProj22          621
#define cmdidMRUProj23          622
#define cmdidMRUProj24          623
#define cmdidMRUProj25          624  // note cmdidMRUProj25 is unused on purpose!

#define cmdidSplitNext          625
#define cmdidSplitPrev          626

#define cmdidCloseAllDocuments      627
#define cmdidNextDocument       628
#define cmdidPrevDocument       629

#define cmdidTool1          630  // note cmdidTool1 - cmdidTool24 must be
#define cmdidTool2          631  // consecutive
#define cmdidTool3          632
#define cmdidTool4          633
#define cmdidTool5          634
#define cmdidTool6          635
#define cmdidTool7          636
#define cmdidTool8          637
#define cmdidTool9          638
#define cmdidTool10         639
#define cmdidTool11         640
#define cmdidTool12         641
#define cmdidTool13         642
#define cmdidTool14         643
#define cmdidTool15         644
#define cmdidTool16         645
#define cmdidTool17         646
#define cmdidTool18         647
#define cmdidTool19         648
#define cmdidTool20         649
#define cmdidTool21         650
#define cmdidTool22         651
#define cmdidTool23         652
#define cmdidTool24         653
#define cmdidExternalCommands       654

#define cmdidPasteNextTBXCBItem     655
#define cmdidToolboxShowAllTabs     656
#define cmdidProjectDependencies    657
#define cmdidCloseDocument      658
#define cmdidToolboxSortItems       659

#define cmdidViewBarView1       660   //UNUSED
#define cmdidViewBarView2       661   //UNUSED
#define cmdidViewBarView3       662   //UNUSED
#define cmdidViewBarView4       663   //UNUSED
#define cmdidViewBarView5       664   //UNUSED
#define cmdidViewBarView6       665   //UNUSED
#define cmdidViewBarView7       666   //UNUSED
#define cmdidViewBarView8       667   //UNUSED
#define cmdidViewBarView9       668   //UNUSED
#define cmdidViewBarView10      669   //UNUSED
#define cmdidViewBarView11      670   //UNUSED
#define cmdidViewBarView12      671   //UNUSED
#define cmdidViewBarView13      672   //UNUSED
#define cmdidViewBarView14      673   //UNUSED
#define cmdidViewBarView15      674   //UNUSED
#define cmdidViewBarView16      675   //UNUSED
#define cmdidViewBarView17      676   //UNUSED
#define cmdidViewBarView18      677   //UNUSED
#define cmdidViewBarView19      678   //UNUSED
#define cmdidViewBarView20      679   //UNUSED
#define cmdidViewBarView21      680   //UNUSED
#define cmdidViewBarView22      681   //UNUSED
#define cmdidViewBarView23      682   //UNUSED
#define cmdidViewBarView24      683   //UNUSED

#define cmdidSolutionCfg        684
#define cmdidSolutionCfgGetList     685

//
// Schema table commands:
// All invoke table property dialog and select appropriate page.
//
#define cmdidManageIndexes          675
#define cmdidManageRelationships    676
#define cmdidManageConstraints      677

// UNUSED: 678 - 727

#define cmdidWhiteSpace                 728

#define cmdidCommandWindow      729
#define cmdidCommandWindowMarkMode  730
#define cmdidLogCommandWindow       731

#define cmdidShell          732

#define cmdidSingleChar           733
#define cmdidZeroOrMore           734
#define cmdidOneOrMore            735
#define cmdidBeginLine            736
#define cmdidEndLine              737
#define cmdidBeginWord            738
#define cmdidEndWord              739
#define cmdidCharInSet            740
#define cmdidCharNotInSet         741
#define cmdidOr                   742
#define cmdidEscape               743
#define cmdidTagExp               744

// See more commands in StandardCommandSet2K, IDs 2509 through 2516

// Regex builder context help menu commands
#define cmdidPatternMatchHelp       745
#define cmdidRegExList              746

#define cmdidDebugReserved1         747
#define cmdidDebugReserved2         748
#define cmdidDebugReserved3         749
//USED ABOVE                        750
//USED ABOVE                        751
//USED ABOVE                        752
//USED ABOVE                        753

#define cmdidAutosWindow        cmdidDebugReserved1
#define cmdidThisWindow         cmdidDebugReserved2

//Regex builder wildcard menu commands
#define cmdidWildZeroOrMore         754
#define cmdidWildSingleChar         755
#define cmdidWildSingleDigit        756
#define cmdidWildCharInSet          757
#define cmdidWildCharNotInSet       758
#define cmdidWildEscape             774

#define cmdidFindWhatText           759
#define cmdidTaggedExp1             760
#define cmdidTaggedExp2             761
#define cmdidTaggedExp3             762
#define cmdidTaggedExp4             763
#define cmdidTaggedExp5             764
#define cmdidTaggedExp6             765
#define cmdidTaggedExp7             766
#define cmdidTaggedExp8             767
#define cmdidTaggedExp9             768

// See more commands in StandardCommandSet2K, IDs 2517 through 2527

#define cmdidEditorWidgetClick      769 // param 0 is the moniker as VT_BSTR, param 1 is the buffer line as VT_I4, and param 2 is the buffer index as VT_I4
#define cmdidCmdWinUpdateAC         770

#define cmdidSlnCfgMgr                  771

#define cmdidAddNewProject      772
#define cmdidAddExistingProject     773
// Used above in cmdidWildEscape    774
#define cmdidAutoHideContext1       776
#define cmdidAutoHideContext2       777
#define cmdidAutoHideContext3       778
#define cmdidAutoHideContext4       779
#define cmdidAutoHideContext5       780
#define cmdidAutoHideContext6       781
#define cmdidAutoHideContext7       782
#define cmdidAutoHideContext8       783
#define cmdidAutoHideContext9       784
#define cmdidAutoHideContext10      785
#define cmdidAutoHideContext11      786
#define cmdidAutoHideContext12      787
#define cmdidAutoHideContext13      788
#define cmdidAutoHideContext14      789
#define cmdidAutoHideContext15      790
#define cmdidAutoHideContext16      791
#define cmdidAutoHideContext17      792
#define cmdidAutoHideContext18      793
#define cmdidAutoHideContext19      794
#define cmdidAutoHideContext20      795
#define cmdidAutoHideContext21      796
#define cmdidAutoHideContext22      797
#define cmdidAutoHideContext23      798
#define cmdidAutoHideContext24      799
#define cmdidAutoHideContext25      800
#define cmdidAutoHideContext26      801
#define cmdidAutoHideContext27      802
#define cmdidAutoHideContext28      803
#define cmdidAutoHideContext29      804
#define cmdidAutoHideContext30      805
#define cmdidAutoHideContext31      806
#define cmdidAutoHideContext32      807
#define cmdidAutoHideContext33      808  // must remain unused

#define cmdidShellNavBackward           809
#define cmdidShellNavForward            810


#define cmdidShellWindowNavigate1   844
#define cmdidShellWindowNavigate2   845
#define cmdidShellWindowNavigate3   846
#define cmdidShellWindowNavigate4   847
#define cmdidShellWindowNavigate5   848
#define cmdidShellWindowNavigate6   849
#define cmdidShellWindowNavigate7   850
#define cmdidShellWindowNavigate8   851
#define cmdidShellWindowNavigate9   852
#define cmdidShellWindowNavigate10  853
#define cmdidShellWindowNavigate11  854
#define cmdidShellWindowNavigate12  855
#define cmdidShellWindowNavigate13  856
#define cmdidShellWindowNavigate14  857
#define cmdidShellWindowNavigate15  858
#define cmdidShellWindowNavigate16  859
#define cmdidShellWindowNavigate17  860
#define cmdidShellWindowNavigate18  861
#define cmdidShellWindowNavigate19  862
#define cmdidShellWindowNavigate20  863
#define cmdidShellWindowNavigate21  864
#define cmdidShellWindowNavigate22  865
#define cmdidShellWindowNavigate23  866
#define cmdidShellWindowNavigate24  867
#define cmdidShellWindowNavigate25  868
#define cmdidShellWindowNavigate26  869
#define cmdidShellWindowNavigate27  870
#define cmdidShellWindowNavigate28  871
#define cmdidShellWindowNavigate29  872
#define cmdidShellWindowNavigate30  873
#define cmdidShellWindowNavigate31  874
#define cmdidShellWindowNavigate32  875
#define cmdidShellWindowNavigate33  876  // must remain unused

// ObjectSearch cmds
#define cmdidOBSDoFind                  877
#define cmdidOBSMatchCase               878
#define cmdidOBSMatchSubString          879
#define cmdidOBSMatchWholeWord          880
#define cmdidOBSMatchPrefix             881

// build cmds
#define cmdidBuildSln                   882
#define cmdidRebuildSln                 883
#define cmdidDeploySln                  884
#define cmdidCleanSln                   885

#define cmdidBuildSel                   886
#define cmdidRebuildSel                 887
#define cmdidDeploySel                  888
#define cmdidCleanSel                   889


#define cmdidCancelBuild                890
#define cmdidBatchBuildDlg              891

#define cmdidBuildCtx                   892
#define cmdidRebuildCtx                 893
#define cmdidDeployCtx                  894
#define cmdidCleanCtx                   895

#define cmdidQryManageIndexes       896
#define cmdidPrintDefault               897     // quick print
//            Unused                898
#define cmdidShowStartPage          899

#define cmdidMRUFile1           900
#define cmdidMRUFile2           901
#define cmdidMRUFile3           902
#define cmdidMRUFile4           903
#define cmdidMRUFile5           904
#define cmdidMRUFile6           905
#define cmdidMRUFile7           906
#define cmdidMRUFile8           907
#define cmdidMRUFile9           908
#define cmdidMRUFile10          909
#define cmdidMRUFile11          910
#define cmdidMRUFile12          911
#define cmdidMRUFile13          912
#define cmdidMRUFile14          913
#define cmdidMRUFile15          914
#define cmdidMRUFile16          915
#define cmdidMRUFile17          916
#define cmdidMRUFile18          917
#define cmdidMRUFile19          918
#define cmdidMRUFile20          919
#define cmdidMRUFile21          920
#define cmdidMRUFile22          921
#define cmdidMRUFile23          922
#define cmdidMRUFile24          923
#define cmdidMRUFile25          924  // note cmdidMRUFile25 is unused on purpose!

//External Tools Context Menu Commands
// continued at 1109
#define cmdidExtToolsCurPath            925
#define cmdidExtToolsCurDir             926
#define cmdidExtToolsCurFileName        927
#define cmdidExtToolsCurExtension       928
#define cmdidExtToolsProjDir            929
#define cmdidExtToolsProjFileName       930
#define cmdidExtToolsSlnDir             931
#define cmdidExtToolsSlnFileName        932

// Object Browsing & ClassView cmds
// Shared shell cmds (for accessing Object Browsing functionality)
#define cmdidGotoDefn           935
#define cmdidGotoDecl           936
#define cmdidBrowseDefn         937
#define cmdidSyncClassView              938
#define cmdidShowMembers        939
#define cmdidShowBases          940
#define cmdidShowDerived        941
#define cmdidShowDefns          942
#define cmdidShowRefs           943
#define cmdidShowCallers        944
#define cmdidShowCallees        945

#define cmdidAddClass                   946
#define cmdidAddNestedClass             947
#define cmdidAddInterface               948
#define cmdidAddMethod                  949
#define cmdidAddProperty                950
#define cmdidAddEvent                   951
#define cmdidAddVariable                952
#define cmdidImplementInterface         953
#define cmdidOverride                   954
#define cmdidAddFunction                955
#define cmdidAddConnectionPoint         956
#define cmdidAddIndexer                 957

#define cmdidBuildOrder                 958
//959 used above for cmdidSaveOptions

// Object Browser Tool Specific cmds
#define cmdidOBEnableGrouping           961
#define cmdidOBSetGroupingCriteria      962
#define cmdidOBShowPackages             965
#define cmdidOBSearchOptWholeWord       967
#define cmdidOBSearchOptSubstring       968
#define cmdidOBSearchOptPrefix          969
#define cmdidOBSearchOptCaseSensitive   970

// ClassView Tool Specific cmds

#define cmdidCVGroupingNone             971
#define cmdidCVGroupingSortOnly         972
#define cmdidCVGroupingGrouped          973
#define cmdidCVShowPackages             974
#define cmdidCVNewFolder                975
#define cmdidCVGroupingSortAccess       976

#define cmdidObjectSearch               977
#define cmdidObjectSearchResults        978

// Further Obj Browsing cmds at 1095

// build cascade menus
#define cmdidBuild1                     979
#define cmdidBuild2                     980
#define cmdidBuild3                     981
#define cmdidBuild4                     982
#define cmdidBuild5                     983
#define cmdidBuild6                     984
#define cmdidBuild7                     985
#define cmdidBuild8                     986
#define cmdidBuild9                     987
#define cmdidBuildLast                  988

#define cmdidRebuild1                   989
#define cmdidRebuild2                   990
#define cmdidRebuild3                   991
#define cmdidRebuild4                   992
#define cmdidRebuild5                   993
#define cmdidRebuild6                   994
#define cmdidRebuild7                   995
#define cmdidRebuild8                   996
#define cmdidRebuild9                   997
#define cmdidRebuildLast                998

#define cmdidClean1                     999
#define cmdidClean2                     1000
#define cmdidClean3                     1001
#define cmdidClean4                     1002
#define cmdidClean5                     1003
#define cmdidClean6                     1004
#define cmdidClean7                     1005
#define cmdidClean8                     1006
#define cmdidClean9                     1007
#define cmdidCleanLast                  1008

#define cmdidDeploy1                    1009
#define cmdidDeploy2                    1010
#define cmdidDeploy3                    1011
#define cmdidDeploy4                    1012
#define cmdidDeploy5                    1013
#define cmdidDeploy6                    1014
#define cmdidDeploy7                    1015
#define cmdidDeploy8                    1016
#define cmdidDeploy9                    1017
#define cmdidDeployLast                 1018

#define cmdidBuildProjPicker            1019
#define cmdidRebuildProjPicker          1020
#define cmdidCleanProjPicker            1021
#define cmdidDeployProjPicker           1022
#define cmdidResourceView               1023
#define cmdidEditMenuIDs                1025

#define cmdidLineBreak                  1026
#define cmdidCPPIdentifier              1027
#define cmdidQuotedString               1028
#define cmdidSpaceOrTab                 1029
#define cmdidInteger                    1030

//unused 1031-1035

#define cmdidCustomizeToolbars          1036
#define cmdidMoveToTop                  1037
#define cmdidWindowHelp                 1038

#define cmdidViewPopup                  1039
#define cmdidCheckMnemonics             1040

#define cmdidPRSortAlphabeticaly        1041
#define cmdidPRSortByCategory           1042

#define cmdidViewNextTab                1043

#define cmdidCheckForUpdates            1044

#define cmdidBrowser1           1045
#define cmdidBrowser2           1046
#define cmdidBrowser3           1047
#define cmdidBrowser4           1048
#define cmdidBrowser5           1049
#define cmdidBrowser6           1050
#define cmdidBrowser7           1051
#define cmdidBrowser8           1052
#define cmdidBrowser9           1053
#define cmdidBrowser10          1054
#define cmdidBrowser11          1055 //note unused on purpose to end list

#define cmdidOpenDropDownOpen           1058
#define cmdidOpenDropDownOpenWith       1059

#define cmdidToolsDebugProcesses        1060

#define cmdidPaneNextSubPane            1062
#define cmdidPanePrevSubPane            1063

#define cmdidMoveFileToProject1         1070
#define cmdidMoveFileToProject2         1071
#define cmdidMoveFileToProject3         1072
#define cmdidMoveFileToProject4         1073
#define cmdidMoveFileToProject5         1074
#define cmdidMoveFileToProject6         1075
#define cmdidMoveFileToProject7         1076
#define cmdidMoveFileToProject8         1077
#define cmdidMoveFileToProject9         1078
#define cmdidMoveFileToProjectLast      1079 // unused in order to end list
#define cmdidMoveFileToProjectPick      1081


#define cmdidDefineSubset               1095
#define cmdidSubsetCombo                1096
#define cmdidSubsetGetList              1097
#define cmdidOBGroupObjectsAccess       1102

#define cmdidPopBrowseContext           1106
#define cmdidGotoRef            1107
#define cmdidOBSLookInReferences        1108

#define cmdidExtToolsTargetPath         1109
#define cmdidExtToolsTargetDir          1110
#define cmdidExtToolsTargetFileName     1111
#define cmdidExtToolsTargetExtension    1112
#define cmdidExtToolsCurLine            1113
#define cmdidExtToolsCurCol             1114
#define cmdidExtToolsCurText            1115

#define cmdidBrowseNext                 1116
#define cmdidBrowsePrev                 1117
#define cmdidBrowseUnload       1118
#define cmdidQuickObjectSearch          1119
#define cmdidExpandAll                  1120

#define cmdidExtToolsBinDir             1121

#define cmdidBookmarkWindow             1122
#define cmdidCodeExpansionWindow        1123

#define cmdidNextDocumentNav            1124    // added to Set97 because they are extentions on cmdidNextDocument
#define cmdidPrevDocumentNav            1125
#define cmdidForwardBrowseContext       1126

#define cmdidCloneWindow                1127

#define cmdidStandardMax                1500

///////////////////////////////////////////
//
// cmdidStandardMax is now thought to be
// obsolete. Any new shell commands should
// be added to the end of StandardCommandSet2K
// which appears below.
//
// If you are not adding shell commands,
// you shouldn't be doing it in this file!
//
///////////////////////////////////////////


#define cmdidFormsFirst           0x00006000

#define cmdidFormsLast           0x00006FFF

#define cmdidVBEFirst           0x00008000


#define cmdidZoom200            0x00008002
#define cmdidZoom150            0x00008003
#define cmdidZoom100            0x00008004
#define cmdidZoom75         0x00008005
#define cmdidZoom50         0x00008006
#define cmdidZoom25         0x00008007
#define cmdidZoom10         0x00008010


#define cmdidVBELast           0x00009FFF

#define cmdidSterlingFirst           0x0000A000
#define cmdidSterlingLast           0x0000BFFF

#define uieventidFirst                   0xC000
#define uieventidSelectRegion      0xC001
#define uieventidDrop                  0xC002
#define uieventidLast                   0xDFFF




//////////////////////////////////////////////////////////////////
//
// The following commands form CMDSETID_StandardCommandSet2k.
// Note that commands up to ECMD_FINAL are standard editor
// commands and have been moved from editcmd.h.
// NOTE that all these commands are shareable and may be used
// in any appropriate menu.
//
//////////////////////////////////////////////////////////////////
//
// Shareable standard editor commands
//
#define ECMD_TYPECHAR                1
#define ECMD_BACKSPACE               2
#define ECMD_RETURN                  3
#define ECMD_TAB                     4
#define ECMD_BACKTAB                 5
#define ECMD_DELETE                  6
#define ECMD_LEFT                    7
#define ECMD_LEFT_EXT                8
#define ECMD_RIGHT                   9
#define ECMD_RIGHT_EXT              10
#define ECMD_UP                     11
#define ECMD_UP_EXT                 12
#define ECMD_DOWN                   13
#define ECMD_DOWN_EXT               14
#define ECMD_HOME                   15
#define ECMD_HOME_EXT               16
#define ECMD_END                    17
#define ECMD_END_EXT                18
#define ECMD_BOL                    19
#define ECMD_BOL_EXT                20
#define ECMD_FIRSTCHAR              21
#define ECMD_FIRSTCHAR_EXT          22
#define ECMD_EOL                    23
#define ECMD_EOL_EXT                24
#define ECMD_LASTCHAR               25
#define ECMD_LASTCHAR_EXT           26
#define ECMD_PAGEUP                 27
#define ECMD_PAGEUP_EXT             28
#define ECMD_PAGEDN                 29
#define ECMD_PAGEDN_EXT             30
#define ECMD_TOPLINE                31
#define ECMD_TOPLINE_EXT            32
#define ECMD_BOTTOMLINE             33
#define ECMD_BOTTOMLINE_EXT         34
#define ECMD_SCROLLUP               35
#define ECMD_SCROLLDN               36
#define ECMD_SCROLLPAGEUP           37
#define ECMD_SCROLLPAGEDN           38
#define ECMD_SCROLLLEFT             39
#define ECMD_SCROLLRIGHT            40
#define ECMD_SCROLLBOTTOM           41
#define ECMD_SCROLLCENTER           42
#define ECMD_SCROLLTOP              43
#define ECMD_SELECTALL              44
#define ECMD_SELTABIFY              45
#define ECMD_SELUNTABIFY            46
#define ECMD_SELLOWCASE             47
#define ECMD_SELUPCASE              48
#define ECMD_SELTOGGLECASE          49
#define ECMD_SELTITLECASE           50
#define ECMD_SELSWAPANCHOR          51
#define ECMD_GOTOLINE               52
#define ECMD_GOTOBRACE              53
#define ECMD_GOTOBRACE_EXT          54
#define ECMD_GOBACK                 55
#define ECMD_SELECTMODE             56
#define ECMD_TOGGLE_OVERTYPE_MODE   57
#define ECMD_CUT                    58
#define ECMD_COPY                   59
#define ECMD_PASTE                  60
#define ECMD_CUTLINE                61
#define ECMD_DELETELINE             62
#define ECMD_DELETEBLANKLINES       63
#define ECMD_DELETEWHITESPACE       64
#define ECMD_DELETETOEOL            65
#define ECMD_DELETETOBOL            66
#define ECMD_OPENLINEABOVE          67
#define ECMD_OPENLINEBELOW          68
#define ECMD_INDENT                 69
#define ECMD_UNINDENT               70
#define ECMD_UNDO                   71
#define ECMD_UNDONOMOVE             72
#define ECMD_REDO                   73
#define ECMD_REDONOMOVE             74
#define ECMD_DELETEALLTEMPBOOKMARKS 75
#define ECMD_TOGGLETEMPBOOKMARK     76
#define ECMD_GOTONEXTBOOKMARK       77
#define ECMD_GOTOPREVBOOKMARK       78
#define ECMD_FIND                   79
#define ECMD_REPLACE                80
#define ECMD_REPLACE_ALL            81
#define ECMD_FINDNEXT               82
#define ECMD_FINDNEXTWORD           83
#define ECMD_FINDPREV               84
#define ECMD_FINDPREVWORD           85
#define ECMD_FINDAGAIN              86
#define ECMD_TRANSPOSECHAR          87
#define ECMD_TRANSPOSEWORD          88
#define ECMD_TRANSPOSELINE          89
#define ECMD_SELECTCURRENTWORD      90
#define ECMD_DELETEWORDRIGHT        91
#define ECMD_DELETEWORDLEFT         92
#define ECMD_WORDPREV               93
#define ECMD_WORDPREV_EXT           94
#define ECMD_WORDNEXT               96
#define ECMD_WORDNEXT_EXT           97
#define ECMD_COMMENTBLOCK           98
#define ECMD_UNCOMMENTBLOCK         99
#define ECMD_SETREPEATCOUNT         100
#define ECMD_WIDGETMARGIN_LBTNDOWN  101
#define ECMD_SHOWCONTEXTMENU        102
#define ECMD_CANCEL                 103
#define ECMD_PARAMINFO              104
#define ECMD_TOGGLEVISSPACE         105
#define ECMD_TOGGLECARETPASTEPOS    106
#define ECMD_COMPLETEWORD           107
#define ECMD_SHOWMEMBERLIST         108
#define ECMD_FIRSTNONWHITEPREV      109
#define ECMD_FIRSTNONWHITENEXT      110
#define ECMD_HELPKEYWORD            111
#define ECMD_FORMATSELECTION        112
#define ECMD_OPENURL            113
#define ECMD_INSERTFILE         114
#define ECMD_TOGGLESHORTCUT     115
#define ECMD_QUICKINFO              116
#define ECMD_LEFT_EXT_COL           117
#define ECMD_RIGHT_EXT_COL          118
#define ECMD_UP_EXT_COL             119
#define ECMD_DOWN_EXT_COL           120
#define ECMD_TOGGLEWORDWRAP         121
#define ECMD_ISEARCH                122
#define ECMD_ISEARCHBACK            123
#define ECMD_BOL_EXT_COL            124
#define ECMD_EOL_EXT_COL            125
#define ECMD_WORDPREV_EXT_COL       126
#define ECMD_WORDNEXT_EXT_COL       127
#define ECMD_OUTLN_HIDE_SELECTION   128
#define ECMD_OUTLN_TOGGLE_CURRENT   129
#define ECMD_OUTLN_TOGGLE_ALL       130
#define ECMD_OUTLN_STOP_HIDING_ALL  131
#define ECMD_OUTLN_STOP_HIDING_CURRENT 132
#define ECMD_OUTLN_COLLAPSE_TO_DEF  133
#define ECMD_DOUBLECLICK            134
#define ECMD_EXTERNALLY_HANDLED_WIDGET_CLICK 135
#define ECMD_COMMENT_BLOCK          136
#define ECMD_UNCOMMENT_BLOCK        137
#define ECMD_OPENFILE               138
#define ECMD_NAVIGATETOURL          139

// For editor internal use only
#define ECMD_HANDLEIMEMESSAGE       140

#define ECMD_SELTOGOBACK            141
#define ECMD_COMPLETION_HIDE_ADVANCED 142

#define ECMD_FORMATDOCUMENT         143
#define ECMD_OUTLN_START_AUTOHIDING 144
#define ECMD_INCREASEFILTER         145
#define ECMD_DECREASEFILTER         146
#define ECMD_SMARTTASKS             147
#define ECMD_COPYTIP                148
#define ECMD_PASTETIP               149
#define ECMD_LEFTCLICK              150
#define ECMD_GOTONEXTBOOKMARKINDOC  151
#define ECMD_GOTOPREVBOOKMARKINDOC  152
#define ECMD_INVOKESNIPPETFROMSHORTCUT 154

// For managed language services internal use only (clovett)
#define ECMD_AUTOCOMPLETE           155
#define ECMD_INVOKESNIPPETPICKER2    156

#define ECMD_DELETEALLBOOKMARKSINDOC    157

#define ECMD_CONVERTTABSTOSPACES        158
#define ECMD_CONVERTSPACESTOTABS        159

// Last Standard Editor Command (+1)
#define ECMD_FINAL                  160

///////////////////////////////////////////////////////////////
// Some new commands created during CTC file rationalisation
///////////////////////////////////////////////////////////////
#define ECMD_STOP                   220
#define ECMD_REVERSECANCEL          221
#define ECMD_SLNREFRESH             222
#define ECMD_SAVECOPYOFITEMAS       223
//
// Shareable commands originating in the HTML editor
// Shared table commands are obsolete! If you still rely on them, please contact jbresler or mikhaila on
// the HTML editor team
//
#define ECMD_NEWELEMENT             224
#define ECMD_NEWATTRIBUTE           225
#define ECMD_NEWCOMPLEXTYPE         226
#define ECMD_NEWSIMPLETYPE          227
#define ECMD_NEWGROUP               228
#define ECMD_NEWATTRIBUTEGROUP      229
#define ECMD_NEWKEY                 230
#define ECMD_NEWRELATION            231
#define ECMD_EDITKEY                232
#define ECMD_EDITRELATION           233
#define ECMD_MAKETYPEGLOBAL         234
#define ECMD_PREVIEWDATASET         235
#define ECMD_GENERATEDATASET        236
#define ECMD_CREATESCHEMA           237
#define ECMD_LAYOUTINDENT           238
#define ECMD_LAYOUTUNINDENT         239
#define ECMD_REMOVEHANDLER          240
#define ECMD_EDITHANDLER            241
#define ECMD_ADDHANDLER             242
#define ECMD_FONTSTYLE              245
#define ECMD_FONTSTYLEGETLIST       246
#define ECMD_PASTEASHTML            247
#define ECMD_VIEWBORDERS            248
#define ECMD_VIEWDETAILS            249
#define ECMD_INSERTTABLE            253
#define ECMD_INSERTCOLLEFT          254
#define ECMD_INSERTCOLRIGHT         255
#define ECMD_INSERTROWABOVE         256
#define ECMD_INSERTROWBELOW         257
#define ECMD_DELETETABLE            258
#define ECMD_DELETECOLS             259
#define ECMD_DELETEROWS             260
#define ECMD_SELECTTABLE            261
#define ECMD_SELECTTABLECOL         262
#define ECMD_SELECTTABLEROW         263
#define ECMD_SELECTTABLECELL        264
#define ECMD_MERGECELLS             265
#define ECMD_SPLITCELL              266
#define ECMD_INSERTCELLLEFT         267
#define ECMD_DELETECELLS            268
#define ECMD_SHOWGRID               277
#define ECMD_SNAPTOGRID             278
#define ECMD_BOOKMARK               279
#define ECMD_HYPERLINK              280
// unused                           284
#define ECMD_BULLETEDLIST           287
#define ECMD_NUMBEREDLIST           288
#define ECMD_EDITSCRIPT             289
#define ECMD_EDITCODEBEHIND         290
#define ECMD_DOCOUTLINEHTML         291

#define ECMD_RUNATSERVER            293
#define ECMD_WEBFORMSVERBS          294
#define ECMD_WEBFORMSTEMPLATES      295
#define ECMD_ENDTEMPLATE            296
#define ECMD_EDITDEFAULTEVENT       297
#define ECMD_SUPERSCRIPT            298
#define ECMD_SUBSCRIPT              299
#define ECMD_EDITSTYLE              300
#define ECMD_ADDIMAGEHEIGHTWIDTH    301
#define ECMD_REMOVEIMAGEHEIGHTWIDTH 302
#define ECMD_LOCKELEMENT            303
#define ECMD_AUTOCLOSEOVERRIDE      305
#define ECMD_NEWANY                 306
#define ECMD_NEWANYATTRIBUTE        307
#define ECMD_DELETEKEY              308
#define ECMD_AUTOARRANGE            309
#define ECMD_VALIDATESCHEMA         310
#define ECMD_NEWFACET               311
#define ECMD_VALIDATEXMLDATA        312
#define ECMD_DOCOUTLINETOGGLE       313
#define ECMD_VALIDATEHTMLDATA       314
#define ECMD_VIEWXMLSCHEMAOVERVIEW  315
#define ECMD_SHOWDEFAULTVIEW        316
#define ECMD_EXPAND_CHILDREN        317
#define ECMD_COLLAPSE_CHILDREN      318
#define ECMD_TOPDOWNLAYOUT          319
#define ECMD_LEFTRIGHTLAYOUT        320
#define ECMD_INSERTCELLRIGHT        321
#define ECMD_EDITMASTER             322
#define ECMD_INSERTSNIPPET          323
#define ECMD_FORMATANDVALIDATION    324
#define ECMD_COLLAPSETAG            325
#define ECMD_SELECT_TAG             329
#define ECMD_SELECT_TAG_CONTENT     330
#define ECMD_CHECK_ACCESSIBILITY    331
#define ECMD_UNCOLLAPSETAG          332
#define ECMD_GENERATEPAGERESOURCE   333
#define ECMD_SHOWNONVISUALCONTROLS  334
#define ECMD_RESIZECOLUMN           335
#define ECMD_RESIZEROW              336
#define ECMD_MAKEABSOLUTE       337
#define ECMD_MAKERELATIVE       338
#define ECMD_MAKESTATIC             339
#define ECMD_INSERTLAYER        340
#define ECMD_UPDATEDESIGNVIEW       341
#define ECMD_UPDATESOURCEVIEW       342
#define ECMD_INSERTCAPTION      343
#define ECMD_DELETECAPTION          344
#define ECMD_MAKEPOSITIONNOTSET     345
#define ECMD_AUTOPOSITIONOPTIONS    346
#define ECMD_EDITIMAGE              347
#define ECMD_VALIDATION_TARGET          11281
#define ECMD_VALIDATION_TARGET_GET_LIST 11282

#define ECMD_CSS_TARGET                 11283
#define ECMD_CSS_TARGET_GET_LIST        11284
//
// Shareable commands originating in the VC project
//
#define ECMD_COMPILE                350
//
#define ECMD_PROJSETTINGS           352
#define ECMD_LINKONLY               353
//
#define ECMD_REMOVE                 355
#define ECMD_PROJSTARTDEBUG         356
#define ECMD_PROJSTEPINTO           357
#define ECMD_UPDATEMGDRES           358
//
//
#define ECMD_UPDATEWEBREF           360
//
#define ECMD_ADDRESOURCE            362
#define ECMD_WEBDEPLOY              363
//
#define ECMD_PROJTOOLORDER            367
//
#define ECMD_PROJECTTOOLFILES        368
//
#define ECMD_OTB_PGO_INSTRUMENT     369
#define ECMD_OTB_PGO_OPT            370
#define ECMD_OTB_PGO_UPDATE         371
#define ECMD_OTB_PGO_RUNSCENARIO    372

#define cmdidUpgradeProject           390
#define cmdidUpgradeAllProjects       391
#define cmdidShowUpdateSolutionDialog 392

//
// Shareable commands originating in the VB and VBA projects
// Note that there are two versions of each command. One
// version is originally from the main (project) menu and the
// other version from a cascading "Add" context menu. The main
// difference between the two commands is that the main menu
// version starts with the text "Add" whereas this is not
// present on the context menu version.
//
#define ECMD_ADDHTMLPAGE            400
#define ECMD_ADDHTMLPAGECTX         401
#define ECMD_ADDMODULE              402
#define ECMD_ADDMODULECTX           403
// unused 404
// unused 405
#define ECMD_ADDWFCFORM             406
// unused 407
// unused 408
// unused 409
#define ECMD_ADDWEBFORM             410
#define ECMD_ADDMASTERPAGE          411
#define ECMD_ADDUSERCONTROL         412
#define ECMD_ADDCONTENTPAGE         413
// unused 414 to 425
#define ECMD_ADDDHTMLPAGE           426
// unused 427 to 431
#define ECMD_ADDIMAGEGENERATOR      432
// unused 433
#define ECMD_ADDINHERWFCFORM        434
// unused 435
#define ECMD_ADDINHERCONTROL        436
// unused 437
#define ECMD_ADDWEBUSERCONTROL      438
// unused 439
// unused 440
// unused 441
#define ECMD_ADDTBXCOMPONENT        442
// unused 443
#define ECMD_ADDWEBSERVICE          444
#define ECMD_ADDSTYLESHEET          445
#define ECMD_SETBROWSELOCATION      446
#define ECMD_REFRESHFOLDER          447
#define ECMD_SETBROWSELOCATIONCTX   448
#define ECMD_VIEWMARKUP             449
#define ECMD_NEXTMETHOD             450
#define ECMD_PREVMETHOD             451

// VB refactoring commands
#define ECMD_RENAMESYMBOL           452
#define ECMD_SHOWREFERENCES         453
#define ECMD_CREATESNIPPET          454
#define ECMD_CREATEREPLACEMENT      455
#define ECMD_INSERTCOMMENT          456

#define ECMD_VIEWCOMPONENTDESIGNER  457

#define ECMD_GOTOTYPEDEF            458

#define ECMD_SHOWSNIPPETHIGHLIGHTING           459
#define ECMD_HIDESNIPPETHIGHLIGHTING           460
//
// Shareable commands originating in the VFP project
//
#define ECMD_ADDVFPPAGE             500
#define ECMD_SETBREAKPOINT          501
//
// Shareable commands originating in the HELP WORKSHOP project
//
#define ECMD_SHOWALLFILES           600
#define ECMD_ADDTOPROJECT           601
#define ECMD_ADDBLANKNODE           602
#define ECMD_ADDNODEFROMFILE        603
#define ECMD_CHANGEURLFROMFILE      604
#define ECMD_EDITTOPIC              605
#define ECMD_EDITTITLE              606
#define ECMD_MOVENODEUP             607
#define ECMD_MOVENODEDOWN           608
#define ECMD_MOVENODELEFT           609
#define ECMD_MOVENODERIGHT          610
//
// Shareable commands originating in the Deploy project
//
// Note there are two groups of deploy project commands.
// The first group of deploy commands.
#define ECMD_ADDOUTPUT              700
#define ECMD_ADDFILE                701
#define ECMD_MERGEMODULE            702
#define ECMD_ADDCOMPONENTS          703
#define ECMD_LAUNCHINSTALLER        704
#define ECMD_LAUNCHUNINSTALL        705
#define ECMD_LAUNCHORCA             706
#define ECMD_FILESYSTEMEDITOR       707
#define ECMD_REGISTRYEDITOR         708
#define ECMD_FILETYPESEDITOR        709
#define ECMD_USERINTERFACEEDITOR    710
#define ECMD_CUSTOMACTIONSEDITOR    711
#define ECMD_LAUNCHCONDITIONSEDITOR 712
#define ECMD_EDITOR                 713
#define ECMD_EXCLUDE                714
#define ECMD_REFRESHDEPENDENCIES    715
#define ECMD_VIEWOUTPUTS            716
#define ECMD_VIEWDEPENDENCIES       717
#define ECMD_VIEWFILTER             718

//
// The Second group of deploy commands.
// Note that there is a special sub-group in which the relative
// positions are important (see below)
//
#define ECMD_KEY                    750
#define ECMD_STRING                 751
#define ECMD_BINARY                 752
#define ECMD_DWORD                  753
#define ECMD_KEYSOLO                754
#define ECMD_IMPORT                 755
#define ECMD_FOLDER                 756
#define ECMD_PROJECTOUTPUT          757
#define ECMD_FILE                   758
#define ECMD_ADDMERGEMODULES        759
#define ECMD_CREATESHORTCUT         760
#define ECMD_LARGEICONS             761
#define ECMD_SMALLICONS             762
#define ECMD_LIST                   763
#define ECMD_DETAILS                764
#define ECMD_ADDFILETYPE            765
#define ECMD_ADDACTION              766
#define ECMD_SETASDEFAULT           767
#define ECMD_MOVEUP                 768
#define ECMD_MOVEDOWN               769
#define ECMD_ADDDIALOG              770
#define ECMD_IMPORTDIALOG           771
#define ECMD_ADDFILESEARCH          772
#define ECMD_ADDREGISTRYSEARCH      773
#define ECMD_ADDCOMPONENTSEARCH     774
#define ECMD_ADDLAUNCHCONDITION     775
#define ECMD_ADDCUSTOMACTION        776
#define ECMD_OUTPUTS                777
#define ECMD_DEPENDENCIES           778
#define ECMD_FILTER                 779
#define ECMD_COMPONENTS             780
#define ECMD_ENVSTRING              781
#define ECMD_CREATEEMPTYSHORTCUT    782
#define ECMD_ADDFILECONDITION       783
#define ECMD_ADDREGISTRYCONDITION   784
#define ECMD_ADDCOMPONENTCONDITION  785
#define ECMD_ADDURTCONDITION        786
#define ECMD_ADDIISCONDITION        787

//
// The relative positions of the commands within the following deploy
// subgroup must remain unaltered, although the group as a whole may
// be repositioned. Note that the first and last elements are special
// boundary elements.
#define ECMD_SPECIALFOLDERBASE      800
#define ECMD_USERSAPPLICATIONDATAFOLDER 800
#define ECMD_COMMONFILES64FOLDER    801
#define ECMD_COMMONFILESFOLDER      802
#define ECMD_CUSTOMFOLDER           803
#define ECMD_USERSDESKTOP           804
#define ECMD_USERSFAVORITESFOLDER   805
#define ECMD_FONTSFOLDER            806
#define ECMD_GLOBALASSEMBLYCACHEFOLDER  807
#define ECMD_MODULERETARGETABLEFOLDER   808
#define ECMD_USERSPERSONALDATAFOLDER    809
#define ECMD_PROGRAMFILES64FOLDER   810
#define ECMD_PROGRAMFILESFOLDER     811
#define ECMD_USERSPROGRAMSMENU      812
#define ECMD_USERSSENDTOMENU        813
#define ECMD_SHAREDCOMPONENTSFOLDER 814
#define ECMD_USERSSTARTMENU         815
#define ECMD_USERSSTARTUPFOLDER     816
#define ECMD_SYSTEM64FOLDER         817
#define ECMD_SYSTEMFOLDER           818
#define ECMD_APPLICATIONFOLDER      819
#define ECMD_USERSTEMPLATEFOLDER    820
#define ECMD_WEBCUSTOMFOLDER        821
#define ECMD_WINDOWSFOLDER          822
#define ECMD_SPECIALFOLDERLAST      822
// End of deploy sub-group
//
// Shareable commands originating in the Visual Studio Analyzer project
//
#define ECMD_EXPORTEVENTS           900
#define ECMD_IMPORTEVENTS           901
#define ECMD_VIEWEVENT              902
#define ECMD_VIEWEVENTLIST          903
#define ECMD_VIEWCHART              904
#define ECMD_VIEWMACHINEDIAGRAM     905
#define ECMD_VIEWPROCESSDIAGRAM     906
#define ECMD_VIEWSOURCEDIAGRAM      907
#define ECMD_VIEWSTRUCTUREDIAGRAM   908
#define ECMD_VIEWTIMELINE           909
#define ECMD_VIEWSUMMARY            910
#define ECMD_APPLYFILTER            911
#define ECMD_CLEARFILTER            912
#define ECMD_STARTRECORDING         913
#define ECMD_STOPRECORDING          914
#define ECMD_PAUSERECORDING         915
#define ECMD_ACTIVATEFILTER         916
#define ECMD_SHOWFIRSTEVENT         917
#define ECMD_SHOWPREVIOUSEVENT      918
#define ECMD_SHOWNEXTEVENT          919
#define ECMD_SHOWLASTEVENT          920
#define ECMD_REPLAYEVENTS           921
#define ECMD_STOPREPLAY             922
#define ECMD_INCREASEPLAYBACKSPEED  923
#define ECMD_DECREASEPLAYBACKSPEED  924
#define ECMD_ADDMACHINE             925
#define ECMD_ADDREMOVECOLUMNS       926
#define ECMD_SORTCOLUMNS            927
#define ECMD_SAVECOLUMNSETTINGS     928
#define ECMD_RESETCOLUMNSETTINGS    929
#define ECMD_SIZECOLUMNSTOFIT       930
#define ECMD_AUTOSELECT             931
#define ECMD_AUTOFILTER             932
#define ECMD_AUTOPLAYTRACK          933
#define ECMD_GOTOEVENT              934
#define ECMD_ZOOMTOFIT              935
#define ECMD_ADDGRAPH               936
#define ECMD_REMOVEGRAPH            937
#define ECMD_CONNECTMACHINE         938
#define ECMD_DISCONNECTMACHINE      939
#define ECMD_EXPANDSELECTION        940
#define ECMD_COLLAPSESELECTION      941
#define ECMD_ADDFILTER              942
#define ECMD_ADDPREDEFINED0         943
#define ECMD_ADDPREDEFINED1         944
#define ECMD_ADDPREDEFINED2         945
#define ECMD_ADDPREDEFINED3         946
#define ECMD_ADDPREDEFINED4         947
#define ECMD_ADDPREDEFINED5         948
#define ECMD_ADDPREDEFINED6         949
#define ECMD_ADDPREDEFINED7         950
#define ECMD_ADDPREDEFINED8         951
#define ECMD_TIMELINESIZETOFIT      952

//
// Shareable commands originating with Crystal Reports
//
#define ECMD_FIELDVIEW             1000
#define ECMD_SELECTEXPERT          1001
#define ECMD_TOPNEXPERT            1002
#define ECMD_SORTORDER             1003
#define ECMD_PROPPAGE              1004
#define ECMD_HELP                  1005
#define ECMD_SAVEREPORT            1006
#define ECMD_INSERTSUMMARY         1007
#define ECMD_INSERTGROUP           1008
#define ECMD_INSERTSUBREPORT       1009
#define ECMD_INSERTCHART           1010
#define ECMD_INSERTPICTURE         1011
//
// Shareable commands from the common project area (DirPrj)
//
#define ECMD_SETASSTARTPAGE        1100
#define ECMD_RECALCULATELINKS      1101
#define ECMD_WEBPERMISSIONS        1102
#define ECMD_COMPARETOMASTER       1103
#define ECMD_WORKOFFLINE           1104
#define ECMD_SYNCHRONIZEFOLDER     1105
#define ECMD_SYNCHRONIZEALLFOLDERS 1106
#define ECMD_COPYPROJECT           1107
#define ECMD_IMPORTFILEFROMWEB     1108
#define ECMD_INCLUDEINPROJECT      1109
#define ECMD_EXCLUDEFROMPROJECT    1110
#define ECMD_BROKENLINKSREPORT     1111
#define ECMD_ADDPROJECTOUTPUTS     1112
#define ECMD_ADDREFERENCE          1113
#define ECMD_ADDWEBREFERENCE       1114
#define ECMD_ADDWEBREFERENCECTX    1115
#define ECMD_UPDATEWEBREFERENCE    1116
#define ECMD_RUNCUSTOMTOOL         1117
#define ECMD_SETRUNTIMEVERSION     1118
#define ECMD_VIEWREFINOBJECTBROWSER  1119
#define ECMD_PUBLISH               1120
#define ECMD_PUBLISHCTX            1121
#define ECMD_STARTOPTIONS          1124
#define ECMD_ADDREFERENCECTX       1125
  // note cmdidPropertyManager is consuming 1126  and it shouldn't
#define ECMD_STARTOPTIONSCTX       1127
#define ECMD_DETACHLOCALDATAFILECTX  1128
#define ECMD_ADDSERVICEREFERENCE   1129
#define ECMD_ADDSERVICEREFERENCECTX 1130
#define ECMD_UPDATESERVICEREFERENCE 1131
#define ECMD_CONFIGURESERVICEREFERENCE 1132

//
// Shareable commands for right drag operations
//
#define ECMD_DRAG_MOVE             1140
#define ECMD_DRAG_COPY             1141
#define ECMD_DRAG_CANCEL           1142

//
// Shareable commands from the VC resource editor
//
#define ECMD_TESTDIALOG            1200
#define ECMD_SPACEACROSS           1201
#define ECMD_SPACEDOWN             1202
#define ECMD_TOGGLEGRID            1203
#define ECMD_TOGGLEGUIDES          1204
#define ECMD_SIZETOTEXT            1205
#define ECMD_CENTERVERT            1206
#define ECMD_CENTERHORZ            1207
#define ECMD_FLIPDIALOG            1208
#define ECMD_SETTABORDER           1209
#define ECMD_BUTTONRIGHT           1210
#define ECMD_BUTTONBOTTOM          1211
#define ECMD_AUTOLAYOUTGROW        1212
#define ECMD_AUTOLAYOUTNORESIZE    1213
#define ECMD_AUTOLAYOUTOPTIMIZE    1214
#define ECMD_GUIDESETTINGS         1215
#define ECMD_RESOURCEINCLUDES      1216
#define ECMD_RESOURCESYMBOLS       1217
#define ECMD_OPENBINARY            1218
#define ECMD_RESOURCEOPEN          1219
#define ECMD_RESOURCENEW           1220
#define ECMD_RESOURCENEWCOPY       1221
#define ECMD_INSERT                1222
#define ECMD_EXPORT                1223
#define ECMD_CTLMOVELEFT           1224
#define ECMD_CTLMOVEDOWN           1225
#define ECMD_CTLMOVERIGHT          1226
#define ECMD_CTLMOVEUP             1227
#define ECMD_CTLSIZEDOWN           1228
#define ECMD_CTLSIZEUP             1229
#define ECMD_CTLSIZELEFT           1230
#define ECMD_CTLSIZERIGHT          1231
#define ECMD_NEWACCELERATOR        1232
#define ECMD_CAPTUREKEYSTROKE      1233
#define ECMD_INSERTACTIVEXCTL      1234
#define ECMD_INVERTCOLORS          1235
#define ECMD_FLIPHORIZONTAL        1236
#define ECMD_FLIPVERTICAL          1237
#define ECMD_ROTATE90              1238
#define ECMD_SHOWCOLORSWINDOW      1239
#define ECMD_NEWSTRING             1240
#define ECMD_NEWINFOBLOCK          1241
#define ECMD_DELETEINFOBLOCK       1242
#define ECMD_ADJUSTCOLORS          1243
#define ECMD_LOADPALETTE           1244
#define ECMD_SAVEPALETTE           1245
#define ECMD_CHECKMNEMONICS        1246
#define ECMD_DRAWOPAQUE            1247
#define ECMD_TOOLBAREDITOR         1248
#define ECMD_GRIDSETTINGS          1249
#define ECMD_NEWDEVICEIMAGE        1250
#define ECMD_OPENDEVICEIMAGE       1251
#define ECMD_DELETEDEVICEIMAGE     1252
#define ECMD_VIEWASPOPUP           1253
#define ECMD_CHECKMENUMNEMONICS    1254
#define ECMD_SHOWIMAGEGRID         1255
#define ECMD_SHOWTILEGRID          1256
#define ECMD_MAGNIFY               1257
#define cmdidResProps              1258
#define ECMD_IMPORTICONIMAGE       1259
#define ECMD_EXPORTICONIMAGE       1260
#define ECMD_OPENEXTERNALEDITOR    1261

//
// Shareable commands from the VC resource editor (Image editor toolbar)
//
#define ECMD_PICKRECTANGLE         1300
#define ECMD_PICKREGION            1301
#define ECMD_PICKCOLOR             1302
#define ECMD_ERASERTOOL            1303
#define ECMD_FILLTOOL              1304
#define ECMD_PENCILTOOL            1305
#define ECMD_BRUSHTOOL             1306
#define ECMD_AIRBRUSHTOOL          1307
#define ECMD_LINETOOL              1308
#define ECMD_CURVETOOL             1309
#define ECMD_TEXTTOOL              1310
#define ECMD_RECTTOOL              1311
#define ECMD_OUTLINERECTTOOL       1312
#define ECMD_FILLEDRECTTOOL        1313
#define ECMD_ROUNDRECTTOOL         1314
#define ECMD_OUTLINEROUNDRECTTOOL  1315
#define ECMD_FILLEDROUNDRECTTOOL   1316
#define ECMD_ELLIPSETOOL           1317
#define ECMD_OUTLINEELLIPSETOOL    1318
#define ECMD_FILLEDELLIPSETOOL     1319
#define ECMD_SETHOTSPOT            1320
#define ECMD_ZOOMTOOL              1321
#define ECMD_ZOOM1X                1322
#define ECMD_ZOOM2X                1323
#define ECMD_ZOOM6X                1324
#define ECMD_ZOOM8X                1325
#define ECMD_TRANSPARENTBCKGRND    1326
#define ECMD_OPAQUEBCKGRND         1327
//---------------------------------------------------
// The commands ECMD_ERASERSMALL thru ECMD_LINELARGER
// must be left in the same order for the use of the
// Resource Editor - They may however be relocated as
// a complete block
//---------------------------------------------------
#define ECMD_ERASERSMALL           1328
#define ECMD_ERASERMEDIUM          1329
#define ECMD_ERASERLARGE           1330
#define ECMD_ERASERLARGER          1331
#define ECMD_CIRCLELARGE           1332
#define ECMD_CIRCLEMEDIUM          1333
#define ECMD_CIRCLESMALL           1334
#define ECMD_SQUARELARGE           1335
#define ECMD_SQUAREMEDIUM          1336
#define ECMD_SQUARESMALL           1337
#define ECMD_LEFTDIAGLARGE         1338
#define ECMD_LEFTDIAGMEDIUM        1339
#define ECMD_LEFTDIAGSMALL         1340
#define ECMD_RIGHTDIAGLARGE        1341
#define ECMD_RIGHTDIAGMEDIUM       1342
#define ECMD_RIGHTDIAGSMALL        1343
#define ECMD_SPLASHSMALL           1344
#define ECMD_SPLASHMEDIUM          1345
#define ECMD_SPLASHLARGE           1346
#define ECMD_LINESMALLER           1347
#define ECMD_LINESMALL             1348
#define ECMD_LINEMEDIUM            1349
#define ECMD_LINELARGE             1350
#define ECMD_LINELARGER            1351
#define ECMD_LARGERBRUSH           1352
#define ECMD_LARGEBRUSH            1353
#define ECMD_STDBRUSH              1354
#define ECMD_SMALLBRUSH            1355
#define ECMD_SMALLERBRUSH          1356
#define ECMD_ZOOMIN                1357
#define ECMD_ZOOMOUT               1358
#define ECMD_PREVCOLOR             1359
#define ECMD_PREVECOLOR            1360
#define ECMD_NEXTCOLOR             1361
#define ECMD_NEXTECOLOR            1362
#define ECMD_IMG_OPTIONS           1363

//
// Sharable Commands from Visual Web Developer (website projects)
//
#define ECMD_STARTWEBADMINTOOL     1400
#define ECMD_NESTRELATEDFILES      1401
#define ECMD_ADDCONFIGTRANSFORMS   1402

//
// Shareable commands from WINFORMS
//
#define ECMD_CANCELDRAG            1500
#define ECMD_DEFAULTACTION         1501
#define ECMD_CTLMOVEUPGRID         1502
#define ECMD_CTLMOVEDOWNGRID       1503
#define ECMD_CTLMOVELEFTGRID       1504
#define ECMD_CTLMOVERIGHTGRID      1505
#define ECMD_CTLSIZERIGHTGRID      1506
#define ECMD_CTLSIZEUPGRID         1507
#define ECMD_CTLSIZELEFTGRID       1508
#define ECMD_CTLSIZEDOWNGRID       1509
#define ECMD_NEXTCTL               1510
#define ECMD_PREVCTL               1511

#define ECMD_RENAME                1550
#define ECMD_EXTRACTMETHOD         1551
#define ECMD_ENCAPSULATEFIELD      1552
#define ECMD_EXTRACTINTERFACE      1553
#define ECMD_REMOVEPARAMETERS      1555
#define ECMD_REORDERPARAMETERS     1556
#define ECMD_GENERATEMETHODSTUB    1557
#define ECMD_IMPLEMENTINTERFACEIMPLICIT   1558
#define ECMD_IMPLEMENTINTERFACEEXPLICIT   1559
#define ECMD_IMPLEMENTABSTRACTCLASS       1560
#define ECMD_SURROUNDWITH                 1561

//---------------------------------------------------
// Additional shell commands added to CMDSETID_StandardCommandSet2K
// because CLSID_StandardCommandSet97 is now considered closed.
//---------------------------------------------------


#define cmdidToggleWordWrapOW           1600

#define cmdidGotoNextLocationOW         1601
#define cmdidGotoPrevLocationOW         1602

#define cmdidBuildOnlyProject           1603
#define cmdidRebuildOnlyProject         1604
#define cmdidCleanOnlyProject           1605

#define cmdidSetBuildStartupsOnlyOnRun  1606

#define cmdidUnhideAll                  1607

#define cmdidHideFolder                 1608
#define cmdidUnhideFolders              1609

#define cmdidCopyFullPathName           1610

#define cmdidSaveFolderAsSolution       1611

#define cmdidManageUserSettings         1612

#define cmdidNewSolutionFolder          1613

#define cmdidSetTrackSelInSlnExp        1614    //changed to match VS 2005 cmdid

#define cmdidClearPaneOW                1615
#define cmdidGotoErrorTagOW             1616
#define cmdidGotoNextErrorTagOW         1617
#define cmdidGotoPrevErrorTagOW         1618

#define cmdidClearPaneFR1               1619
#define cmdidGotoErrorTagFR1            1620
#define cmdidGotoNextErrorTagFR1        1621
#define cmdidGotoPrevErrorTagFR1        1622

#define cmdidClearPaneFR2               1623
#define cmdidGotoErrorTagFR2            1624
#define cmdidGotoNextErrorTagFR2        1625
#define cmdidGotoPrevErrorTagFR2        1626

// Output Window pane selection dropdown
#define cmdidOutputPaneCombo            1627
#define cmdidOutputPaneComboList        1628

#define cmdidDisableDockingChanges      1629
#define cmdidToggleFloat                1630
#define cmdidResetLayout                1631

#define cmdidEditProjectFile            1632

#define cmdidOpenInFormView             1633
#define cmdidOpenInCodeView             1634

#define cmdidExploreFolderInWindows     1635

#define cmdidEnableDPLSolution          1636
#define cmdidDisableDPLSolution         1637

#define cmdidNewSolutionFolderBar       1638

#define cmdidDataShortcut               1639



// Tool window navigation
#define cmdidNextToolWindow             1640
#define cmdidPrevToolWindow             1641
#define cmdidBrowseToFileInExplorer     1642
#define cmdidShowEzMDIFileMenu          1643
#define cmdidNextToolWindowNav          1644    // command for NextToolWindow with navigator
#define cmdidPrevToolWindowNav          1645

// One Time build with static anlaysis for ProjOnly
#define cmdidStaticAnalysisOnlyProject  1646

//Run Code Analysis on Build Menu
#define ECMD_RUNFXCOPSEL                1647
//Run Code Analysis on Context menu for the selected project
#define ECMD_RUNFXCOPPROJCTX            1648

#define cmdidCloseAllButThis            1650

// not real commands - used to define Class view and Object browser commands
#define SYM_TOOL_COMMAND_FIRST                 1

#define cmdidSymToolShowInheritedMembers       1
#define cmdidSymToolShowBaseTypes              2
#define cmdidSymToolShowDerivedTypes           3
#define cmdidSymToolShowHidden                 4
#define cmdidSymToolBack                       5
#define cmdidSymToolForward                    6
#define cmdidSymToolSearchCombo                7
#define cmdidSymToolSearch                     8
#define cmdidSymToolSortObjectsAlpha           9
#define cmdidSymToolSortObjectsType           10
#define cmdidSymToolSortObjectsAccess         11
#define cmdidSymToolGroupObjectsType          12
#define cmdidSymToolSortMembersAlpha          13
#define cmdidSymToolSortMembersType           14
#define cmdidSymToolSortMembersAccess         15
#define cmdidSymToolTypeBrowserSettings       16
#define cmdidSymToolViewMembersAsImplementor  17
#define cmdidSymToolViewMembersAsSubclass     18
#define cmdidSymToolViewMembersAsUser         19
#define cmdidSymToolNamespacesView            20
#define cmdidSymToolContainersView            21
#define cmdidSymToolShowProjectReferences     22
#define cmdidSymToolGroupMembersType          23
#define cmdidSymToolClearSearch               24
#define cmdidSymToolFilterToType              25
#define cmdidSymToolSortByBestMatch           26
#define cmdidSymToolSearchMRUList             27
#define cmdidSymToolViewOtherMembers          28
#define cmdidSymToolSearchCmd                 29
#define cmdidSymToolGoToSearchCmd             30
#define cmdidSymToolShowExtensionMembers      31    

#define SYM_TOOL_COMMAND_LAST                 31

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// the numbers from 1650 to 1699 are reserved for Class view  specific commands
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

#define CV_COMMANDS_BASE                1650

#define cmdidCVShowInheritedMembers     1651 //CV_COMMANDS_BASE + cmdidSymToolShowInheritedMembers
#define cmdidCVShowBaseTypes            1652 //CV_COMMANDS_BASE + cmdidSymToolShowBaseTypes
#define cmdidCVShowDerivedTypes         1653 //CV_COMMANDS_BASE + cmdidSymToolShowDerivedTypes
#define cmdidCVShowHidden               1654 //CV_COMMANDS_BASE + cmdidSymToolShowHidden
#define cmdidCVBack                     1655 //CV_COMMANDS_BASE + cmdidSymToolBack
#define cmdidCVForward                  1656 //CV_COMMANDS_BASE + cmdidSymToolForward
#define cmdidCVSearchCombo              1657 //CV_COMMANDS_BASE + cmdidSymToolSearchCombo
#define cmdidCVSearch                   1658 //CV_COMMANDS_BASE + cmdidSymToolSearch
#define cmdidCVSortObjectsAlpha         1659 //CV_COMMANDS_BASE + cmdidSymToolSortObjectsAlpha
#define cmdidCVSortObjectsType          1660 //CV_COMMANDS_BASE + cmdidSymToolSortObjectsType
#define cmdidCVSortObjectsAccess        1661 //CV_COMMANDS_BASE + cmdidSymToolSortObjectsAccess
#define cmdidCVGroupObjectsType         1662 //CV_COMMANDS_BASE + cmdidSymToolGroupObjectsType
#define cmdidCVSortMembersAlpha         1663 //CV_COMMANDS_BASE + cmdidSymToolSortMembersAlpha
#define cmdidCVSortMembersType          1664 //CV_COMMANDS_BASE + cmdidSymToolSortMembersType
#define cmdidCVSortMembersAccess        1665 //CV_COMMANDS_BASE + cmdidSymToolSortMembersAccess
#define cmdidCVTypeBrowserSettings      1666 //CV_COMMANDS_BASE + cmdidSymToolTypeBrowserSettings
#define cmdidCVViewMembersAsImplementor 1667 //CV_COMMANDS_BASE + cmdidSymToolViewMembersAsImplementor
#define cmdidCVViewMembersAsSubclass    1668 //CV_COMMANDS_BASE + cmdidSymToolViewMembersAsSubclass
#define cmdidCVViewMembersAsUser        1669 //CV_COMMANDS_BASE + cmdidSymToolViewMembersAsUser
#define cmdidCVReserved1                1670 //CV_COMMANDS_BASE + cmdidSymToolNamespacesView
#define cmdidCVReserved2                1671 //CV_COMMANDS_BASE + cmdidSymToolContainersView
#define cmdidCVShowProjectReferences    1672 //CV_COMMANDS_BASE + cmdidSymToolShowProjectReferences
#define cmdidCVGroupMembersType         1673 //CV_COMMANDS_BASE + cmdidSymToolGroupMembersType
#define cmdidCVClearSearch              1674 //CV_COMMANDS_BASE + cmdidSymToolClearSearch
#define cmdidCVFilterToType             1675 //CV_COMMANDS_BASE + cmdidSymToolFilterToType
#define cmdidCVSortByBestMatch          1676 //CV_COMMANDS_BASE + cmdidSymToolSortByBestMatch
#define cmdidCVSearchMRUList            1677 //CV_COMMANDS_BASE + cmdidSymToolSearchMRUList
#define cmdidCVViewOtherMembers         1678 //CV_COMMANDS_BASE + cmdidSymToolViewOtherMembers
#define cmdidCVSearchCmd                1679 //CV_COMMANDS_BASE + cmdidSymToolSearchCmd
#define cmdidCVGoToSearchCmd            1680 //CV_COMMANDS_BASE + cmdidSymToolGoToSearchCmd

#define cmdidCVUnused9                  1681 // Reserved for future use
#define cmdidCVUnused10                 1682 // Reserved for future use
#define cmdidCVUnused11                 1683 // Reserved for future use
#define cmdidCVUnused12                 1684 // Reserved for future use
#define cmdidCVUnused13                 1685 // Reserved for future use
#define cmdidCVUnused14                 1686 // Reserved for future use
#define cmdidCVUnused15                 1687 // Reserved for future use
#define cmdidCVUnused16                 1688 // Reserved for future use
#define cmdidCVUnused17                 1689 // Reserved for future use
#define cmdidCVUnused18                 1690 // Reserved for future use
#define cmdidCVUnused19                 1691 // Reserved for future use
#define cmdidCVUnused20                 1692 // Reserved for future use
#define cmdidCVUnused21                 1693 // Reserved for future use
#define cmdidCVUnused22                 1694 // Reserved for future use
#define cmdidCVUnused23                 1695 // Reserved for future use
#define cmdidCVUnused24                 1696 // Reserved for future use
#define cmdidCVUnused25                 1697 // Reserved for future use
#define cmdidCVUnused26                 1698 // Reserved for future use
#define cmdidCVUnused27                 1699 // Reserved for future use

//-------------------end of CV commands----------------------------------------------

#define cmdidControlGallery             1700

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// the numbers from 1710 to 1759 are reserved for Object Browser specific commands
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

#define OB_COMMANDS_BASE                1710

#define cmdidOBShowInheritedMembers     1711 //OB_COMMANDS_BASE + cmdidSymToolShowInheritedMembers
#define cmdidOBShowBaseTypes            1712 //OB_COMMANDS_BASE + cmdidSymToolShowBaseTypes
#define cmdidOBShowDerivedTypes         1713 //OB_COMMANDS_BASE + cmdidSymToolShowDerivedTypes
#define cmdidOBShowHidden               1714 //OB_COMMANDS_BASE + cmdidSymToolShowHidden
#define cmdidOBBack                     1715 //OB_COMMANDS_BASE + cmdidSymToolBack
#define cmdidOBForward                  1716 //OB_COMMANDS_BASE + cmdidSymToolForward
#define cmdidOBSearchCombo              1717 //OB_COMMANDS_BASE + cmdidSymToolSearchCombo
#define cmdidOBSearch                   1718 //OB_COMMANDS_BASE + cmdidSymToolSearch
#define cmdidOBSortObjectsAlpha         1719 //OB_COMMANDS_BASE + cmdidSymToolSortObjectsAlpha
#define cmdidOBSortObjectsType          1720 //OB_COMMANDS_BASE + cmdidSymToolSortObjectsType
#define cmdidOBSortObjectsAccess        1721 //OB_COMMANDS_BASE + cmdidSymToolSortObjectsAccess
#define cmdidOBGroupObjectsType         1722 //OB_COMMANDS_BASE + cmdidSymToolGroupObjectsType
#define cmdidOBSortMembersAlpha         1723 //OB_COMMANDS_BASE + cmdidSymToolSortMembersAlpha
#define cmdidOBSortMembersType          1724 //OB_COMMANDS_BASE + cmdidSymToolSortMembersType
#define cmdidOBSortMembersAccess        1725 //OB_COMMANDS_BASE + cmdidSymToolSortMembersAccess
#define cmdidOBTypeBrowserSettings      1726 //OB_COMMANDS_BASE + cmdidSymToolTypeBrowserSettings
#define cmdidOBViewMembersAsImplementor 1727 //OB_COMMANDS_BASE + cmdidSymToolViewMembersAsImplementor
#define cmdidOBViewMembersAsSubclass    1728 //OB_COMMANDS_BASE + cmdidSymToolViewMembersAsSubclass
#define cmdidOBViewMembersAsUser        1729 //OB_COMMANDS_BASE + cmdidSymToolViewMembersAsUser
#define cmdidOBNamespacesView           1730 //OB_COMMANDS_BASE + cmdidSymToolNamespacesView
#define cmdidOBContainersView           1731 //OB_COMMANDS_BASE + cmdidSymToolContainersView
#define cmdidOBReserved1                1732 //OB_COMMANDS_BASE + cmdidSymToolShowProjectReferences
#define cmdidOBGroupMembersType         1733 //OB_COMMANDS_BASE + cmdidSymToolGroupMembersType
#define cmdidOBClearSearch              1734 //OB_COMMANDS_BASE + cmdidSymToolClearSearch
#define cmdidOBFilterToType             1735 //OB_COMMANDS_BASE + cmdidSymToolFilterToType
#define cmdidOBSortByBestMatch          1736 //OB_COMMANDS_BASE + cmdidSymToolSortByBestMatch
#define cmdidOBSearchMRUList            1737 //OB_COMMANDS_BASE + cmdidSymToolSearchMRUList
#define cmdidOBViewOtherMembers         1738 //OB_COMMANDS_BASE + cmdidSymToolViewOtherMembers
#define cmdidOBSearchCmd                1739 //OB_COMMANDS_BASE + cmdidSymToolSearchCmd
#define cmdidOBGoToSearchCmd            1740 //OB_COMMANDS_BASE + cmdidSymToolGoToSearchCmd
#define cmdidOBShowExtensionMembers     1741 //OB_COMMANDS_BASE + cmdidSymToolShowExtensionMembers

#define cmdidOBUnused10                 1742 // Reserved for future use
#define cmdidOBUnused11                 1743 // Reserved for future use
#define cmdidOBUnused12                 1744 // Reserved for future use
#define cmdidOBUnused13                 1745 // Reserved for future use
#define cmdidOBUnused14                 1746 // Reserved for future use
#define cmdidOBUnused15                 1747 // Reserved for future use
#define cmdidOBUnused16                 1748 // Reserved for future use
#define cmdidOBUnused17                 1749 // Reserved for future use
#define cmdidOBUnused18                 1750 // Reserved for future use
#define cmdidOBUnused19                 1751 // Reserved for future use
#define cmdidOBUnused20                 1752 // Reserved for future use
#define cmdidOBUnused21                 1753 // Reserved for future use
#define cmdidOBUnused22                 1754 // Reserved for future use
#define cmdidOBUnused23                 1755 // Reserved for future use
#define cmdidOBUnused24                 1756 // Reserved for future use
#define cmdidOBUnused25                 1757 // Reserved for future use
#define cmdidOBUnused26                 1758 // Reserved for future use
#define cmdidOBUnused27                 1759 // Reserved for future use

//-------------------end of OB commands----------------------------------------------

#define cmdidFullScreen2                1775

// find symbol results sorting command
#define cmdidFSRSortObjectsAlpha        1776
#define cmdidFSRSortByBestMatch         1777

#define cmdidNavigateBack               1800
#define cmdidNavigateForward            1801

// Error correction commands (need to be consecutive)
#define ECMD_CORRECTION_1               1900
#define ECMD_CORRECTION_2               1901
#define ECMD_CORRECTION_3               1902
#define ECMD_CORRECTION_4               1903
#define ECMD_CORRECTION_5               1904
#define ECMD_CORRECTION_6               1905
#define ECMD_CORRECTION_7               1906
#define ECMD_CORRECTION_8               1907
#define ECMD_CORRECTION_9               1908
#define ECMD_CORRECTION_10              1909

// Object Browser commands
#define cmdidOBAddReference             1914

// Edit.FindAllReferences
#define cmdidFindReferences             1915

// Code Definition View
#define cmdidCodeDefView                1926
#define cmdidCodeDefViewGoToPrev        1927
#define cmdidCodeDefViewGoToNext        1928
#define cmdidCodeDefViewEditDefinition  1929
#define cmdidCodeDefViewChooseEncoding  1930

// Class view
//#define cmdidCVShowProjectReferences    1930
#define cmdidViewInClassDiagram         1931

//
// Shareable commands from VSDesigner
//
#define ECMD_ADDDBTABLE                 1950
#define ECMD_ADDDATATABLE               1951
#define ECMD_ADDFUNCTION                1952
#define ECMD_ADDRELATION                1953
#define ECMD_ADDKEY                     1954
#define ECMD_ADDCOLUMN                  1955
#define ECMD_CONVERT_DBTABLE            1956
#define ECMD_CONVERT_DATATABLE          1957
#define ECMD_GENERATE_DATABASE          1958
#define ECMD_CONFIGURE_CONNECTIONS      1959
#define ECMD_IMPORT_XMLSCHEMA           1960
#define ECMD_SYNC_WITH_DATABASE         1961
#define ECMD_CONFIGURE                  1962
#define ECMD_CREATE_DATAFORM            1963
#define ECMD_CREATE_ENUM                1964
#define ECMD_INSERT_FUNCTION            1965
#define ECMD_EDIT_FUNCTION              1966
#define ECMD_SET_PRIMARY_KEY            1967
#define ECMD_INSERT_COLUMN              1968
#define ECMD_AUTO_SIZE                  1969
#define ECMD_SHOW_RELATION_LABELS       1970

#define cmdid_VSD_GenerateDataSet       1971
#define cmdid_VSD_Preview               1972
#define cmdid_VSD_ConfigureAdapter      1973
#define cmdid_VSD_ViewDatasetSchema     1974
#define cmdid_VSD_DatasetProperties     1975
#define cmdid_VSD_ParameterizeForm      1976
#define cmdid_VSD_AddChildForm          1977

#define ECMD_EDITCONSTRAINT             1978
#define ECMD_DELETECONSTRAINT           1979
#define ECMD_EDITDATARELATION           1980

#define cmdidCloseProject               1982
#define cmdidReloadCommandBars          1983

#define cmdidSolutionPlatform           1990
#define cmdidSolutionPlatformGetList    1991

// Initially used by DataSet Editor
#define ECMD_DATAACCESSOR               2000
#define ECMD_ADD_DATAACCESSOR           2001
#define ECMD_QUERY                      2002
#define ECMD_ADD_QUERY                  2003

// Publish solution
#define ECMD_PUBLISHSELECTION           2005
#define ECMD_PUBLISHSLNCTX              2006
#define ECMD_MSDEPLOYPUBLISHSLNCTX              2007

// Call Browser
#define cmdidCallBrowserShowCallsTo         2010
#define cmdidCallBrowserShowCallsFrom       2011
#define cmdidCallBrowserShowNewCallsTo      2012
#define cmdidCallBrowserShowNewCallsFrom    2013

#define cmdidCallBrowser1ShowCallsTo        2014
#define cmdidCallBrowser2ShowCallsTo        2015
#define cmdidCallBrowser3ShowCallsTo        2016
#define cmdidCallBrowser4ShowCallsTo        2017
#define cmdidCallBrowser5ShowCallsTo        2018
#define cmdidCallBrowser6ShowCallsTo        2019
#define cmdidCallBrowser7ShowCallsTo        2020
#define cmdidCallBrowser8ShowCallsTo        2021
#define cmdidCallBrowser9ShowCallsTo        2022
#define cmdidCallBrowser10ShowCallsTo       2023
#define cmdidCallBrowser11ShowCallsTo       2024
#define cmdidCallBrowser12ShowCallsTo       2025
#define cmdidCallBrowser13ShowCallsTo       2026
#define cmdidCallBrowser14ShowCallsTo       2027
#define cmdidCallBrowser15ShowCallsTo       2028
#define cmdidCallBrowser16ShowCallsTo       2029

#define cmdidCallBrowser1ShowCallsFrom      2030
#define cmdidCallBrowser2ShowCallsFrom      2031
#define cmdidCallBrowser3ShowCallsFrom      2032
#define cmdidCallBrowser4ShowCallsFrom      2033
#define cmdidCallBrowser5ShowCallsFrom      2034
#define cmdidCallBrowser6ShowCallsFrom      2035
#define cmdidCallBrowser7ShowCallsFrom      2036
#define cmdidCallBrowser8ShowCallsFrom      2037
#define cmdidCallBrowser9ShowCallsFrom      2038
#define cmdidCallBrowser10ShowCallsFrom     2039
#define cmdidCallBrowser11ShowCallsFrom     2040
#define cmdidCallBrowser12ShowCallsFrom     2041
#define cmdidCallBrowser13ShowCallsFrom     2042
#define cmdidCallBrowser14ShowCallsFrom     2043
#define cmdidCallBrowser15ShowCallsFrom     2044
#define cmdidCallBrowser16ShowCallsFrom     2045

#define cmdidCallBrowser1ShowFullNames      2046
#define cmdidCallBrowser2ShowFullNames      2047
#define cmdidCallBrowser3ShowFullNames      2048
#define cmdidCallBrowser4ShowFullNames      2049
#define cmdidCallBrowser5ShowFullNames      2050
#define cmdidCallBrowser6ShowFullNames      2051
#define cmdidCallBrowser7ShowFullNames      2052
#define cmdidCallBrowser8ShowFullNames      2053
#define cmdidCallBrowser9ShowFullNames      2054
#define cmdidCallBrowser10ShowFullNames     2055
#define cmdidCallBrowser11ShowFullNames     2056
#define cmdidCallBrowser12ShowFullNames     2057
#define cmdidCallBrowser13ShowFullNames     2058
#define cmdidCallBrowser14ShowFullNames     2059
#define cmdidCallBrowser15ShowFullNames     2060
#define cmdidCallBrowser16ShowFullNames     2061

#define cmdidCallBrowser1Settings           2062
#define cmdidCallBrowser2Settings           2063
#define cmdidCallBrowser3Settings           2064
#define cmdidCallBrowser4Settings           2065
#define cmdidCallBrowser5Settings           2066
#define cmdidCallBrowser6Settings           2067
#define cmdidCallBrowser7Settings           2068
#define cmdidCallBrowser8Settings           2069
#define cmdidCallBrowser9Settings           2070
#define cmdidCallBrowser10Settings          2071
#define cmdidCallBrowser11Settings          2072
#define cmdidCallBrowser12Settings          2073
#define cmdidCallBrowser13Settings          2074
#define cmdidCallBrowser14Settings          2075
#define cmdidCallBrowser15Settings          2076
#define cmdidCallBrowser16Settings          2077

#define cmdidCallBrowser1SortAlpha          2078
#define cmdidCallBrowser2SortAlpha          2079
#define cmdidCallBrowser3SortAlpha          2080
#define cmdidCallBrowser4SortAlpha          2081
#define cmdidCallBrowser5SortAlpha          2082
#define cmdidCallBrowser6SortAlpha          2083
#define cmdidCallBrowser7SortAlpha          2084
#define cmdidCallBrowser8SortAlpha          2085
#define cmdidCallBrowser9SortAlpha          2086
#define cmdidCallBrowser10SortAlpha         2087
#define cmdidCallBrowser11SortAlpha         2088
#define cmdidCallBrowser12SortAlpha         2089
#define cmdidCallBrowser13SortAlpha         2090
#define cmdidCallBrowser14SortAlpha         2091
#define cmdidCallBrowser15SortAlpha         2092
#define cmdidCallBrowser16SortAlpha         2093

#define cmdidCallBrowser1SortAccess         2094
#define cmdidCallBrowser2SortAccess         2095
#define cmdidCallBrowser3SortAccess         2096
#define cmdidCallBrowser4SortAccess         2097
#define cmdidCallBrowser5SortAccess         2098
#define cmdidCallBrowser6SortAccess         2099
#define cmdidCallBrowser7SortAccess         2100
#define cmdidCallBrowser8SortAccess         2101
#define cmdidCallBrowser9SortAccess         2102
#define cmdidCallBrowser10SortAccess        2103
#define cmdidCallBrowser11SortAccess        2104
#define cmdidCallBrowser12SortAccess        2105
#define cmdidCallBrowser13SortAccess        2106
#define cmdidCallBrowser14SortAccess        2107
#define cmdidCallBrowser15SortAccess        2108
#define cmdidCallBrowser16SortAccess        2109

#define cmdidCallBrowser1                   2121
#define cmdidCallBrowser2                   2122
#define cmdidCallBrowser3                   2123
#define cmdidCallBrowser4                   2124
#define cmdidCallBrowser5                   2125
#define cmdidCallBrowser6                   2126
#define cmdidCallBrowser7                   2127
#define cmdidCallBrowser8                   2128
#define cmdidCallBrowser9                   2129
#define cmdidCallBrowser10                  2130
#define cmdidCallBrowser11                  2131
#define cmdidCallBrowser12                  2132
#define cmdidCallBrowser13                  2133
#define cmdidCallBrowser14                  2134
#define cmdidCallBrowser15                  2135
#define cmdidCallBrowser16                  2136
#define cmdidCallBrowser17                  2137

// Closed file undo
#define cmdidGlobalUndo                     2138
#define cmdidGlobalRedo                     2139

// Call Browser Commands (No UI, Command window only).
#define cmdidCallBrowserShowCallsToCmd      2140
#define cmdidCallBrowserShowCallsFromCmd    2141
#define cmdidCallBrowserShowNewCallsToCmd   2142
#define cmdidCallBrowserShowNewCallsFromCmd 2143

#define cmdidCallBrowser1Search             2145
#define cmdidCallBrowser2Search             2146
#define cmdidCallBrowser3Search             2147
#define cmdidCallBrowser4Search             2148
#define cmdidCallBrowser5Search             2149
#define cmdidCallBrowser6Search             2150
#define cmdidCallBrowser7Search             2151
#define cmdidCallBrowser8Search             2152
#define cmdidCallBrowser9Search             2153
#define cmdidCallBrowser10Search            2154
#define cmdidCallBrowser11Search            2155
#define cmdidCallBrowser12Search            2156
#define cmdidCallBrowser13Search            2157
#define cmdidCallBrowser14Search            2158
#define cmdidCallBrowser15Search            2159
#define cmdidCallBrowser16Search            2160

#define cmdidCallBrowser1Refresh            2161
#define cmdidCallBrowser2Refresh            2162
#define cmdidCallBrowser3Refresh            2163
#define cmdidCallBrowser4Refresh            2164
#define cmdidCallBrowser5Refresh            2165
#define cmdidCallBrowser6Refresh            2166
#define cmdidCallBrowser7Refresh            2167
#define cmdidCallBrowser8Refresh            2168
#define cmdidCallBrowser9Refresh            2169
#define cmdidCallBrowser10Refresh           2170
#define cmdidCallBrowser11Refresh           2171
#define cmdidCallBrowser12Refresh           2172
#define cmdidCallBrowser13Refresh           2173
#define cmdidCallBrowser14Refresh           2174
#define cmdidCallBrowser15Refresh           2175
#define cmdidCallBrowser16Refresh           2176

#define cmdidCallBrowser1SearchCombo        2180
#define cmdidCallBrowser2SearchCombo        2181
#define cmdidCallBrowser3SearchCombo        2182
#define cmdidCallBrowser4SearchCombo        2183
#define cmdidCallBrowser5SearchCombo        2184
#define cmdidCallBrowser6SearchCombo        2185
#define cmdidCallBrowser7SearchCombo        2186
#define cmdidCallBrowser8SearchCombo        2187
#define cmdidCallBrowser9SearchCombo        2188
#define cmdidCallBrowser10SearchCombo       2189
#define cmdidCallBrowser11SearchCombo       2190
#define cmdidCallBrowser12SearchCombo       2191
#define cmdidCallBrowser13SearchCombo       2192
#define cmdidCallBrowser14SearchCombo       2193
#define cmdidCallBrowser15SearchCombo       2194
#define cmdidCallBrowser16SearchCombo       2195

// Callbrowser SearchComboList commands start with  2215

// Task List

#define cmdidTaskListProviderCombo          2200
#define cmdidTaskListProviderComboList      2201

// User Task toolbar commands
#define cmdidCreateUserTask                 2202

// Error List

// Error List toolbar commands
#define cmdidErrorListShowErrors            2210
#define cmdidErrorListShowWarnings          2211
#define cmdidErrorListShowMessages          2212

// Product activation (registration)
#define cmdidRegistration                   2214


// Callbrowser SearchComboList commands
#define cmdidCallBrowser1SearchComboList    2215
#define cmdidCallBrowser2SearchComboList    2216
#define cmdidCallBrowser3SearchComboList    2217
#define cmdidCallBrowser4SearchComboList    2218
#define cmdidCallBrowser5SearchComboList    2219
#define cmdidCallBrowser6SearchComboList    2220
#define cmdidCallBrowser7SearchComboList    2221
#define cmdidCallBrowser8SearchComboList    2222
#define cmdidCallBrowser9SearchComboList    2223
#define cmdidCallBrowser10SearchComboList   2224
#define cmdidCallBrowser11SearchComboList   2225
#define cmdidCallBrowser12SearchComboList   2226
#define cmdidCallBrowser13SearchComboList   2227
#define cmdidCallBrowser14SearchComboList   2228
#define cmdidCallBrowser15SearchComboList   2229
#define cmdidCallBrowser16SearchComboList   2230


//Snippet window buttons
#define cmdidSnippetProp                    2240
#define cmdidSnippetRef                     2241
#define cmdidSnippetRepl                    2242

//Start Page Command:
#define cmdidStartPage                      2245

// More editor commands
#define cmdidEditorLineFirstColumn          2250
#define cmdidEditorLineFirstColumnExtend    2251

// Server Explorer Menu commands
#define cmdid_SE_ServerExplorer             2260
#define cmdid_SE_DataExplorer               2261

// Commands for Floating, Docking and Hiding documents
#define cmdidDocumentFloat                  2270

// Call hierarchy
#define cmdidContextMenuViewCallHierarchy           2301

#define cmdidToggleConsumeFirstMode         2303

// Highlight References commands
#define cmdidNextHighlightedReference       2400
#define cmdidPreviousHighlightedReference   2401

//
// Shareable commands from the VC resource editor (Ribbon editor toolbar)
//
#define ECMD_TESTRIBBON                     2504
#define ECMD_RIBBON_VM                      2505
#define ECMD_RIBBON_VM_GET_LIST             2506
#define ECMD_RIBBON_ITEMS                   2507
#define ECMD_RIBBON_BUTTONS                 2508

// Regexp expression builder new commands
#define cmdidWordChar             2509
#define cmdidCharInRange          2510
#define cmdidOneAndZeroOrOne      2511
#define cmdidOneAndZeroOrMore     2512
#define cmdidOneAndOneOrMore      2513
#define cmdidQuantifier           2514
#define cmdidBackreference        2515
#define cmdidNamedBackreference   2516

#define cmdidTaggedExp              2517
#define cmdidNamedTaggedExp         2518
#define cmdidDollarSubstitute       2519
#define cmdidWholeMatch             2520
#define cmdidLastTaggedExp          2521
#define cmdidSpaceOrTabMap			2522
#define cmdidNumericCharacterMap	2523
#define cmdidCPPIdentifierMap		2524
#define cmdidQuotedStringMap		2525
#define cmdidHexadecimalNumberMap	2526
#define cmdidIntegersDecimalsMap	2527

#define cmdidBuildFullPDB          2528
#define cmdidBuildFullPDBSolution  2529

//////////////////////////////////////////////////////////////////
//
// The following commands form CMDSETID_StandardCommandSet10.
// NOTE that all these commands are shareable and may be used
// in any appropriate menu.
//
//////////////////////////////////////////////////////////////////


//The command that returns all shell toolbars sorted lexographically by their text
//
// NOTE:  The range between cmdidDynamicToolBarListFirst and cmdidDynamicToolBarListLast is reserved
//        for the dynamic toolbar list.  Do not use command IDs in this range.
#define cmdidDynamicToolBarListFirst 1
#define cmdidDynamicToolBarListLast 300

// Command for dropping window frame docking menu
#define cmdidWindowFrameDockMenu     500

// Commands for going to the next/previous tab in the document well
#define cmdidDocumentTabNext         600
#define cmdidDocumentTabPrevious     601


//////////////////////////////////////////////////////////////////
//
// Command ids for navigate backward submenus. These replaces cmdidShellNavigate* commands
//
// There must be 33 ids between each one since these are dynamic start items and each one 
// can have up to 32 items.
//
//////////////////////////////////////////////////////////////////

#define cmdidShellNavigate1First         1000
#define cmdidShellNavigate2First         1033
#define cmdidShellNavigate3First         1066
#define cmdidShellNavigate4First         1099
#define cmdidShellNavigate5First         1132
#define cmdidShellNavigate6First         1165
#define cmdidShellNavigate7First         1198
#define cmdidShellNavigate8First         1231
#define cmdidShellNavigate9First         1264
#define cmdidShellNavigate10First         1297
#define cmdidShellNavigate11First         1330
#define cmdidShellNavigate12First         1363
#define cmdidShellNavigate13First         1396
#define cmdidShellNavigate14First         1429
#define cmdidShellNavigate15First         1462
#define cmdidShellNavigate16First         1495
#define cmdidShellNavigate17First         1528
#define cmdidShellNavigate18First         1561
#define cmdidShellNavigate19First         1594
#define cmdidShellNavigate20First         1627
#define cmdidShellNavigate21First         1660
#define cmdidShellNavigate22First         1693
#define cmdidShellNavigate23First         1726
#define cmdidShellNavigate24First         1759
#define cmdidShellNavigate25First         1792
#define cmdidShellNavigate26First         1825
#define cmdidShellNavigate27First         1858
#define cmdidShellNavigate28First         1891
#define cmdidShellNavigate29First         1924
#define cmdidShellNavigate30First         1957
#define cmdidShellNavigate31First         1990
#define cmdidShellNavigate32First         2023
#define cmdidShellNavigateLast            2055   // last command in this series

// Command ID for ForceGC
#define cmdidShellForceGC                 2090

//
// Command ids for global zoom operations
//
#define cmdidViewZoomIn                   2100
#define cmdidViewZoomOut                  2101

// More outlining commands
#define ECMD_OUTLN_EXPAND_ALL             2500
#define ECMD_OUTLN_COLLAPSE_ALL           2501
#define ECMD_OUTLN_EXPAND_CURRENT         2502
#define ECMD_OUTLN_COLLAPSE_CURRENT       2503

// Command ID for Extension Manager command
#define cmdidExtensionManager             3000


//////////////////////////////////////////////////////////////////
//
// End CMDSETID_StandardCommandSet10 commands.
//
//////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////
//
// The following commands form CMDSETID_StandardCommandSet11.
// NOTE that all these commands are shareable and may be used
// in any appropriate menu.
//
//////////////////////////////////////////////////////////////////

// Commands for document management
#define cmdidFloatAll                       1
#define cmdidMoveAllToNext                  2
#define cmdidMoveAllToPrevious              3
#define cmdidMultiSelect                    4
#define cmdidPaneNextTabAndMultiSelect      5
#define cmdidPanePrevTabAndMultiSelect      6
#define cmdidPinTab                         7
#define cmdidBringFloatingWindowsToFront    8
#define cmdidPromoteTab                     9
#define cmdidMoveToMainTabWell              10

//Commands for error/task list filtering
#define cmdidToggleFilter                   11
#define cmdidFilterToCurrentProject         12
#define cmdidFilterToCurrentDocument        13
#define cmdidFilterToOpenDocuments          14

//15-16 range reserved for cmdidHelpSelectPreference commands

//Commands for activating the search controls
#define cmdidWindowSearch                   17
#define cmdidGlobalSearch                   18
#define cmdidGlobalSearchBack               19
#define cmdidSolutionExplorerSearch         20
#define cmdidStartupProjectProperties       21

#define cmdidCloseAllButPinned              22

#define cmdidResolveFaultedProjects         23

//Commands for Interactive scripting (F#/Roslyn)
#define cmdidExecuteSelectionInInteractive  24
#define cmdidExecuteLineInInteractive       25
#define cmdidInteractiveSessionInterrupt    26
#define cmdidInteractiveSessionRestart      27

//unused									28
#define cmdidSolutionExplorerCollapseAll    29
#define cmdidSolutionExplorerBack           30
#define cmdidSolutionExplorerHome           31
// unused                                   32
#define cmdidSolutionExplorerForward        33
#define cmdidSolutionExplorerNewScopedWindow    34
#define cmdidSolutionExplorerToggleSingleClickPreview   35
#define cmdidSolutionExplorerSyncWithActiveDocument 36

#define cmdidNewProjectFromTemplate         37

#define cmdidSolutionExplorerScopeToThis    38
#define cmdidSolutionExplorerFilterOpened   39
#define cmdidSolutionExplorerFilterPendingChanges 40

#define cmdidPasteAsLink    41

// Used by the find manager to locate find targets
#define cmdidLocateFindTarget 42

//////////////////////////////////////////////////////////////////
//
// End CMDSETID_StandardCommandSet11 commands.
//
//////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////
//
// The following commands form CMDSETID_StandardCommandSet12.
// NOTE that all these commands are shareable and may be used
// in any appropriate menu.
//
//////////////////////////////////////////////////////////////////

#define cmdidShowUserNotificationsToolWindow    1
#define cmdidOpenProjectFromScc                 2
#define cmdidShareProject                       3
#define cmdidPeekDefinition                     4
#define cmdidAccountSettings                    5
#define cmdidPeekNavigateForward                6
#define cmdidPeekNavigateBackward               7
#define cmdidRetargetProject                    8
#define cmdidRetargetProjectInstallComponent    9
#define cmdidAddReferenceProjectOnly            10
#define cmdidAddWebReferenceProjectOnly         11
#define cmdidAddServiceReferenceProjectOnly     12
#define cmdidAddReferenceNonProjectOnly         13
#define cmdidAddWebReferenceNonProjectOnly      14
#define cmdidAddServiceReferenceNonProjectOnly  15

// Do not change the NavigateTo values (they were set to match when switching from a private to public release to prevent breaks)
#define cmdidNavigateTo	                    256

#define cmdidMoveSelLinesUp                 258
#define cmdidMoveSelLinesDown               259
/* Add cmdid values in the 1-256 range first*/

//////////////////////////////////////////////////////////////////
//
// End CMDSETID_StandardCommandSet12 commands.
//
//////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////
//
// The following commands form CMDSETID_StandardCommandSet14.
// NOTE that all these commands are shareable and may be used
// in any appropriate menu.
//
//////////////////////////////////////////////////////////////////

#define cmdidShowQuickFixes             1
#define cmdidShowRefactorings           2
#define cmdidSmartBreakLine             3
#define cmdidManageLayouts              4
#define cmdidSaveLayout                 5
#define cmdidShowQuickFixesForPosition  6
#define cmdidShowQuickFixesForPosition2 7

// Delete toolbar button commands for Find Results (FR) 1 & 2 tool windows
#define cmdidDeleteFR1             10
#define cmdidDeleteFR2             20

#define cmdidErrorContextComboList      30
#define cmdidErrorContextComboGetList   31

#define cmdidErrorBuildContextComboList      40
#define cmdidErrorBuildContextComboGetList   41

#define cmdidErrorListClearFilters      50

// The values 0x1000 to 0x1FFF are reserved for the Apply Window Layout's list
// The first 9 commands are explicitly defined so they can be assigned key bindings
#define cmdidWindowLayoutList0            0x1000
#define cmdidWindowLayoutList1            0x1001
#define cmdidWindowLayoutList2            0x1002
#define cmdidWindowLayoutList3            0x1003
#define cmdidWindowLayoutList4            0x1004
#define cmdidWindowLayoutList5            0x1005
#define cmdidWindowLayoutList6            0x1006
#define cmdidWindowLayoutList7            0x1007
#define cmdidWindowLayoutList8            0x1008
#define cmdidWindowLayoutList9            0x1009
#define cmdidWindowLayoutListFirst        cmdidWindowLayoutList0
#define cmdidWindowLayoutListDynamicFirst 0x1010
#define cmdidWindowLayoutListLast         0x1FFF

//////////////////////////////////////////////////////////////////
//
// End CMDSETID_StandardCommandSet14 commands.
//
//////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////
//
// The following commands form CMDSETID_StandardCommandSet15.
// NOTE that all these commands are shareable and may be used
// in any appropriate menu.
//
//////////////////////////////////////////////////////////////////

#define cmdidNavigateToFile                 1
#define cmdidNavigateToType                 2
#define cmdidNavigateToSymbol               3
#define cmdidNavigateToMember               4
// Please start after 25 or after to reserve a few for filtered NavigateTo commands.

// Find All References preset groupings
#define cmdidFindAllRefPresetGroupingComboList     0x2A
#define cmdidFindAllRefPresetGroupingComboGetList  0x2B
#define cmdidFindAllRefLockWindow                  0x2C

#define cmdidGetToolsAndFeatures            0x3C

#define cmdidShowLineAnnotations            0x4C
#define cmdidMoveToNextAnnotation           0x4D
#define cmdidMoveToPreviousAnnotation       0x4E

#define cmdidShowStructure                  0x4F

#define cmdidHelpAccessibility              0x70

//////////////////////////////////////////////////////////////////
//
// End CMDSETID_StandardCommandSet15 commands.
//
//////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////
//
// The following commands form guidDataCmdId.
// NOTE that all these commands are shareable and may be used
// in any appropriate menu.
//
//////////////////////////////////////////////////////////////////
#define icmdDesign                  0x3000      // design command for project items
#define icmdDesignOn                0x3001      // design on... command for project items

#define icmdSEDesign                0x3003      // design command for the SE side
#define icmdNewDiagram              0x3004
#define icmdNewTable                0x3006

#define icmdNewDBItem               0x300E
#define icmdNewTrigger              0x3010

#define icmdDebug                   0x3012
#define icmdNewProcedure            0x3013
#define icmdNewQuery                0x3014
#define icmdRefreshLocal            0x3015

#define icmdDbAddDataConnection         0x3017
#define icmdDBDefDBRef              0x3018
#define icmdRunCmd                  0x3019
#define icmdRunOn                   0x301A
#define icmdidNewDBRef              0x301B
#define icmdidSetAsDef              0x301C
#define icmdidCreateCmdFile         0x301D
#define icmdCancel                  0x301E

#define icmdNewDatabase             0x3020
#define icmdNewUser                 0x3021
#define icmdNewRole                 0x3022
#define icmdChangeLogin             0x3023
#define icmdNewView                 0x3024
#define icmdModifyConnection        0x3025
#define icmdDisconnect              0x3026
#define icmdCopyScript              0x3027
#define icmdAddSCC                  0x3028
#define icmdRemoveSCC               0x3029
#define icmdGetLatest               0x3030
#define icmdCheckOut                0x3031
#define icmdCheckIn                 0x3032
#define icmdUndoCheckOut            0x3033
#define icmdAddItemSCC              0x3034
#define icmdNewPackageSpec          0x3035
#define icmdNewPackageBody          0x3036
#define icmdInsertSQL               0x3037
#define icmdRunSelection            0x3038
#define icmdUpdateScript            0x3039
#define icmdCreateScript            0x303A  // to be used by db project side
#define icmdSECreateScript          0x303B  // to be used by SE side as opposed to db project side
#define icmdNewScript               0x303C
#define icmdNewFunction             0x303D
#define icmdNewTableFunction        0x303E
#define icmdNewInlineFunction       0x303F

#define icmdAddDiagram              0x3040
#define icmdAddTable                0x3041
#define icmdAddSynonym              0x3042
#define icmdAddView                 0x3043
#define icmdAddProcedure            0x3044
#define icmdAddFunction             0x3045
#define icmdAddTableFunction        0x3046
#define icmdAddInlineFunction       0x3047
#define icmdAddPkgSpec              0x3048
#define icmdAddPkgBody              0x3049
#define icmdAddTrigger              0x304A
#define icmdExportData              0x304B

#define icmdDbnsVcsAdd              0x304C
#define icmdDbnsVcsRemove           0x304D
#define icmdDbnsVcsCheckout         0x304E
#define icmdDbnsVcsUndoCheckout     0x304F
#define icmdDbnsVcsCheckin          0x3050

#define icmdSERetrieveData          0x3060
#define icmdSEEditTextObject        0x3061
#define icmdSERun                   0x3062  // to be used by SE side as opposed to db project side
#define icmdSERunSelection          0x3063  // to be used by SE side as opposed to db project side
#define icmdDesignSQLBlock          0x3064

#define icmdRegisterSQLInstance     0x3065
#define icmdUnregisterSQLInstance   0x3066

/////////////////////////////////////////////////////////
//
// Command Windows submenu commands 0x31xx
//
//

// It would be nice to make the 3 commands below
// group with the new related commands in VS 2005
// but there are hard coded references to the original
// values above.
// Modifying the values above to those below
// would require changes to those references
// (in src\vsdesigner\..., and elsewhere)
// #define cmdidCommandWindow              0x3100
// #define cmdidCommandWindowMarkMode      0x3101
// #define cmdidLogCommandWindow       0x3102
#define cmdidCommandWindowSaveScript    0x3106
#define cmdidCommandWindowRunScript     0x3107
#define cmdidCommandWindowCursorUp      0x3108
#define cmdidCommandWindowCursorDown    0x3109
#define cmdidCommandWindowCursorLeft    0x310A
#define cmdidCommandWindowCursorRight   0x310B
#define cmdidCommandWindowHistoryUp     0x310C
#define cmdidCommandWindowHistoryDown   0x310D


// Command ids reserved for data driven implementation of data explorer.
// used under guidDataCmdId


// from datamenu.h
#define icmdidCmdDTStart                       0x3500
#define icmdidCmdHeirarchyView0                0x3610
#define icmdidCmdHeirarchyView1                0x3611
#define icmdidCmdHeirarchyView2                0x3612
#define icmdidCmdHeirarchyView3                0x3613
#define icmdidCmdHeirarchyView4                0x3614
#define icmdidCmdHeirarchyView5                0x3615
#define icmdidCmdHeirarchyView6                0x3616
#define icmdidCmdHeirarchyView7                0x3617
#define icmdidCmdHeirarchyView8                0x3618

#define icmdidCmdModify                        0x3620
#define icmdidCmdClose                        0x3621
#define icmdidCmdDTLast                        0x36FF




// end of Command Windows submenu commands


//////////////////////////////////////////////////////////////////
//
// The following commands form guidDavDataCmdId.
// NOTE that all these commands are shareable and may be used
// in any appropriate menu.
//
//////////////////////////////////////////////////////////////////
#define    cmdidAddRelatedTables            0x0001
#define cmdidLayoutDiagram                0x0002
#define cmdidLayoutSelection            0x0003
#define cmdidInsertColumn                0x0004
#define cmdidDeleteColumn                0x0005
#define cmdidNewTextAnnotation            0x0006
#define cmdidShowRelLabels                0x0007
#define cmdidViewPageBreaks                0x0008
#define cmdidRecalcPageBreaks            0x0009
#define cmdidViewUserDefined            0x000a
#define cmdidGenerateQuery                0x000b
#define cmdidDeleteFromDB                0x000c
#define cmdidAutoSize                    0x000d
#define cmdidEditViewUserDefined        0x000e
#define cmdidSetAnnotationFont            0x000f
#define cmdidZoomPercent200                0x0010
#define cmdidZoomPercent150                0x0011
#define cmdidZoomPercent100                0x0012
#define cmdidZoomPercent75                0x0013
#define cmdidZoomPercent50                0x0014
#define cmdidZoomPercent25                0x0015
#define cmdidZoomPercent10                0x0016
#define cmdidZoomPercentSelection        0x0017
#define cmdidZoomPercentFit                0x0018
#define cmdidInsertQBERow                0x0019
#define cmdidInsertCriteria                0x0020
#define cmdidAddTableView                0x0021
#define cmdidManageTriggers                0x0022
#define cmdidManagePermissions            0x0023
#define cmdidViewDependencies            0x0024
#define cmdidGenerateSQLScript            0x0025
#define cmdidVerifySQLSilent            0x0026
#define cmdidAddTableViewForQRY            0x0027
#define cmdidManageIndexesForQRY        0x0028
#define cmdidViewFieldListQry            0x0029
#define cmdidViewCollapsedQry            0x002a
#define cmdidCopyDiagram                0x002b
#define cmdidRemoveFromDiagram          0x0033
// defined in davmenu.h
// cmdidQryAddCTEBasic                    0x002c
// cmdidQryAddCTERecursive                0x002d
// cmdidQryAddCTEMerged                    0x002e
// cmdidQryAddDerivedTable                0x002f
// cmdidQryNavigate                        0x0030
// cmdidQryClear                        0x0031
// cmdidQryMerge                        0x0032

// Emacs editor emulator commands. They are in their
// own group, guidEmacsCommandGroup, so they won't clash
#define cmdidEmacsCharLeft                    1
#define cmdidEmacsCharRight                   2
#define cmdidEmacsLineUp                      3
#define cmdidEmacsLineDown                    4
#define cmdidEmacsLineEnd                     5
#define cmdidEmacsHome                        6
#define cmdidEmacsEnd                         7
#define cmdidEmacsDocumentStart               8
#define cmdidEmacsWordLeft                    9
#define cmdidEmacsWordRight                  10
#define cmdidEmacsGoto                       11
#define cmdidEmacsWindowScrollUp             12
#define cmdidEmacsWindowScrollDown           13
#define cmdidEmacsWindowScrollToCenter       14
#define cmdidEmacsWindowStart                15
#define cmdidEmacsWindowEnd                  16
#define cmdidEmacsWindowLineToTop            17
#define cmdidEmacsWindowSplitVertical        18
#define cmdidEmacsWindowOther                19
#define cmdidEmacsWindowCloseOther           20
#define cmdidEmacsReturn                     21
#define cmdidEmacsReturnAndIndent            22
#define cmdidEmacsLineOpen                   23
#define cmdidEmacsCharTranspose              24
#define cmdidEmacsWordTranspose              25
//#define cmdidEmacsBackspaceUntabify        26
//#define cmdidEmacsBackspace                27
//#define cmdidEmacsDelete                   28
#define cmdidEmacsWordUpperCase              29
#define cmdidEmacsWordLowerCase              30
#define cmdidEmacsWordCapitalize             31
#define cmdidEmacsWordDeleteToEnd            32
#define cmdidEmacsWordDeleteToStart          33
#define cmdidEmacsLineCut                    34
//#define cmdidEmacsCutSelection             35
//#define cmdidEmacsPaste                    36
#define cmdidEmacsPasteRotate                37
//#define cmdidEmacsCopySelection            38
#define cmdidEmacsSetMark                    39
#define cmdidEmacsPopMark                    40
#define cmdidEmacsSwapPointAndMark           41
#define cmdidEmacsDeleteSelection            42
#define cmdidEmacsFileOpen                   43
#define cmdidEmacsFileSave                   44
#define cmdidEmacsFileSaveAs                 45
#define cmdidEmacsFileSaveSome               46
#define cmdidEmacsSearchIncremental          47
#define cmdidEmacsSearchIncrementalBack      48
#define cmdidEmacsFindReplace                49
//#define cmdidEmacsUndo                     50
#define cmdidEmacsQuit                       51
#define cmdidEmacsUniversalArgument          52
#define cmdidEmacsExtendedCommand            53
#define cmdidEmacsStartKbdMacro              54
#define cmdidEmacsEndKbdMacro                55
#define cmdidEmacsExecuteKbdMacro            56
//#define cmdidEmacsIndentLine               57
#define cmdidEmacsQuotedInsert               58
#define cmdidEmacsActivateRegion             59

// Brief editor emulator commands. They are in their
// own group, guidBriefCommandGroup, so they won't clash
//#define cmdidBriefCharLeft                      1
//#define cmdidBriefCharRight                     2
//#define cmdidBriefLineUp                        3
//#define cmdidBriefLineDown                      4
#define cmdidBriefSelectColumn                    5
#define cmdidBriefLineIndent                      7
#define cmdidBriefBookmarks                       8
#define cmdidBriefSelectLine                      9
//#define cmdidBriefSelectionLowercase           10
#define cmdidBriefSelectChar                     11
#define cmdidBriefSelectCharInclusive            12
#define cmdidBriefLineUnindent                   13
#define cmdidBriefFilePrint                      14
#define cmdidBriefSelectSwapAnchor               15
//#define cmdidBriefSelectionUppercase           16
//#define cmdidBriefFileClose                    17
//#define cmdidBriefFileOpen
//#define cmdidBriefWindowNext                   18
//#define cmdidBriefWindowPrevious               19
#define cmdidBriefInsertFile                     20
//#define cmdidBriefHome                         21
//#define cmdidBriefDocumentEnd                  22
//#define cmdidBriefEnd                          23
//#define cmdidBriefWindowEnd                    24
#define cmdidBriefGoTo                           25
#define cmdidBriefWindowLeftEdge                 26
#define cmdidBriefWordRight                      27
//#define cmdidBriefPageDown                     28
//#define cmdidBriefPageUp                       29
#define cmdidBriefWordLeft                       30
#define cmdidBriefWindowRightEdge                31
#define cmdidBriefWindowScrollUp                 32
#define cmdidBriefWindowScrollDown               33
#define cmdidBriefWindowStart                    34
#define cmdidBriefLineDelete                     35
#define cmdidBriefWordDeleteToEnd                36
#define cmdidBriefWordDeleteToStart              37
#define cmdidBriefLineDeleteToStart              38
#define cmdidBriefLineDeleteToEnd                39
//#define cmdidBriefToggleOvertype               40
#define cmdidBriefLineOpenBelow                  41
#define cmdidBriefInsertQuoted                   42
#define cmdidBriefFileExit                       43
#define cmdidBriefFileSave                       44
#define cmdidBriefFileSaveAllExit                45
//#define cmdidBriefCopy                         46
//#define cmdidBriefCut                          47
//#define cmdidBriefPaste                        48
#define cmdidBriefFindToggleCaseSensitivity      49
#define cmdidBriefSearchIncremental              50
#define cmdidBriefFindToggleRegExpr              51
#define cmdidBriefFindRepeat                     52
#define cmdidBriefFindPrev                       53
#define cmdidBriefFind                           54
#define cmdidBriefFindReplace                    55
#define cmdidBriefBrowse                         56
#define cmdidBriefGoToNextErrorTag               57
#define cmdidBriefSetRepeatCount                 58
//#define cmdidBriefUndo                         59
//#define cmdidBriefRedo                         60
#define cmdidBriefWindowScrollToCenter           61
#define cmdidBriefWindowSwitchPane               62
#define cmdidBriefWindowSplit                    63
//#define cmdidBriefWindowDelete                 64
#define cmdidBriefWindowScrollToBottom           65
#define cmdidBriefWindowScrollToTop              66
#define cmdidBriefWindowMaximize                 67
#define cmdidBriefBackspace                      68
//#define cmdidBriefDelete                       69
#define cmdidBriefReturn                         70
#define cmdidBriefEscape                         71
#define cmdidBriefBookmarkDrop0                  72
#define cmdidBriefBookmarkDrop1                  73
#define cmdidBriefBookmarkDrop2                  74
#define cmdidBriefBookmarkDrop3                  75
#define cmdidBriefBookmarkDrop4                  76
#define cmdidBriefBookmarkDrop5                  77
#define cmdidBriefBookmarkDrop6                  78
#define cmdidBriefBookmarkDrop7                  79
#define cmdidBriefBookmarkDrop8                  80
#define cmdidBriefBookmarkDrop9                  81

//////////////////////////////////////////////////////////////////
//
// The following commands form Yukon projects.
//
//////////////////////////////////////////////////////////////////
#define cmdidAddNewUDF                  0x0101
#define cmdidAddNewSProc                0x0102
#define cmdidAddNewAggregarte           0x0103
#define cmdidAddNewTrigger              0x0104
#define cmdidAddNewUDT                  0x0105
#define cmdidSetDefaultDebugScript      0x0106
#define cmdidStartDebugScript           0x0107
#define cmdidAddNewDebugScript          0x0108

#define cmdidPromptDatabaseConnection   0x0201


//---------------------------------------------------

//
// Shareable commands for VS Enterprise team
//
#define cmdidAddToFavorites             0x0001
#define cmdidAddNewFolder               0x0002
#define cmdidStopTeamExplorerRefresh    0x0003
#define cmdidAddDataboundMpp            0x0004
#define cmdidAddDataboundXls            0x0005

// End of shareable commands for VS Enterprise team


//////////////////////////////////////////////////////////////////
//
// This command range is private to the EzMDI command set
//
//////////////////////////////////////////////////////////////////

#define cmdidEzMDIFile1                      0x0001

//////////////////////////////////////////////////////////////////
//
// Command ids for Server Explorer commands (guid_SE_CommandID group)
//
//////////////////////////////////////////////////////////////////

#define cmdid_SE_ToolbarRefresh                     0x03004
#define cmdid_SE_ToolbarStopRefresh                 0x03005
#define cmdid_SE_AddToForm                          0x03009
#define cmdid_SE_AddConnection                      0x03100
#define cmdid_SE_AddServer                          0x03101

//////////////////////////////////////////////////////////////////
//
// Command ids for task list / error list commands (CLSID_VsTaskListPackage group)
//
//////////////////////////////////////////////////////////////////

// These were used in Everett, but are no longer applicable in Whidbey.  I don't want to re-use
// them because it could cause unexpected behavior for third-party code which sends these
// so I'll just comment them out.

//#define cmdidTaskListFilterByCategoryUser     359
//#define cmdidTaskListFilterByCategoryShortcut     360
//#define cmdidTaskListFilterByCategoryHTML     361
//#define cmdidTaskListFilterByCurrentFile      362
//#define cmdidTaskListFilterByChecked          363
//#define cmdidTaskListFilterByUnchecked            364
//#define cmdidTaskListSortByDescription            365
//#define cmdidTaskListSortByChecked            366
//#define cmdidTaskListCustomView1        678
//#define cmdidTaskListCustomView2        679
//#define cmdidTaskListCustomView3        680
//#define cmdidTaskListCustomView4        681
//#define cmdidTaskListCustomView5        682
//#define cmdidTaskListCustomView6        683
//#define cmdidTaskListCustomView7        684
//#define cmdidTaskListCustomView8        685
//#define cmdidTaskListCustomView9        686
//#define cmdidTaskListCustomView10       687
//#define cmdidTaskListCustomView11       688
//#define cmdidTaskListCustomView12       689
//#define cmdidTaskListCustomView13       690
//#define cmdidTaskListCustomView14       691
//#define cmdidTaskListCustomView15       692
//#define cmdidTaskListCustomView16       693
//#define cmdidTaskListCustomView17       694
//#define cmdidTaskListCustomView18       695
//#define cmdidTaskListCustomView19       696
//#define cmdidTaskListCustomView20       697
//#define cmdidTaskListCustomView21       698
//#define cmdidTaskListCustomView22       699
//#define cmdidTaskListCustomView23       700
//#define cmdidTaskListCustomView24       701
//#define cmdidTaskListCustomView25       702
//#define cmdidTaskListCustomView26       703
//#define cmdidTaskListCustomView27       704
//#define cmdidTaskListCustomView28       705
//#define cmdidTaskListCustomView29       706
//#define cmdidTaskListCustomView30       707
//#define cmdidTaskListCustomView31       708
//#define cmdidTaskListCustomView32       709
//#define cmdidTaskListCustomView33       710
//#define cmdidTaskListCustomView34       711
//#define cmdidTaskListCustomView35       712
//#define cmdidTaskListCustomView36       713
//#define cmdidTaskListCustomView37       714
//#define cmdidTaskListCustomView38       715
//#define cmdidTaskListCustomView39       716
//#define cmdidTaskListCustomView40       717
//#define cmdidTaskListCustomView41       718
//#define cmdidTaskListCustomView42       719
//#define cmdidTaskListCustomView43       720
//#define cmdidTaskListCustomView44       721
//#define cmdidTaskListCustomView45       722
//#define cmdidTaskListCustomView46       723
//#define cmdidTaskListCustomView47       724
//#define cmdidTaskListCustomView48       725
//#define cmdidTaskListCustomView49       726
//#define cmdidTaskListCustomView50       727 //not used on purpose, ends the list

// Pre-Whidbey commands

#define cmdidTaskListNextError      357
#define cmdidTaskListPrevError      358
#define cmdidTaskListTaskHelp       598

// Whidbey commands

#define cmdidErrorListNextError     1
#define cmdidErrorListPrevError     2

#define cmdidTaskListColumnToggle1  3
#define cmdidTaskListColumnToggle2  4
#define cmdidTaskListColumnToggle3  5
#define cmdidTaskListColumnToggle4  6
#define cmdidTaskListColumnToggle5  7
#define cmdidTaskListColumnToggle6  8
#define cmdidTaskListColumnToggle7  9
#define cmdidTaskListColumnToggle8  10
#define cmdidTaskListColumnToggle9  11
#define cmdidTaskListColumnToggle10 12
#define cmdidTaskListColumnToggle11 13
#define cmdidTaskListColumnToggle12 14
#define cmdidTaskListColumnToggle13 15
#define cmdidTaskListColumnToggle14 16
#define cmdidTaskListColumnToggle15 17
#define cmdidTaskListColumnToggle16 18
#define cmdidTaskListColumnToggle17 19
#define cmdidTaskListColumnToggle18 20
#define cmdidTaskListColumnToggle19 21
#define cmdidTaskListColumnToggle20 22
#define cmdidTaskListColumnToggle21 23
#define cmdidTaskListColumnToggle22 24
#define cmdidTaskListColumnToggle23 25
#define cmdidTaskListColumnToggle24 26
#define cmdidTaskListColumnToggle25 27

// Leave some space for the future, just in case...since the handling of the
// cmdidTaskListColumnToggle* commands requires them to be in a contiguous range.

#define cmdidTaskListColumnSort1    200
#define cmdidTaskListColumnSort2    201
#define cmdidTaskListColumnSort3    202
#define cmdidTaskListColumnSort4    203
#define cmdidTaskListColumnSort5    204
#define cmdidTaskListColumnSort6    205
#define cmdidTaskListColumnSort7    206
#define cmdidTaskListColumnSort8    207
#define cmdidTaskListColumnSort9    208
#define cmdidTaskListColumnSort10   209
#define cmdidTaskListColumnSort11   210
#define cmdidTaskListColumnSort12   211
#define cmdidTaskListColumnSort13   212
#define cmdidTaskListColumnSort14   213
#define cmdidTaskListColumnSort15   214
#define cmdidTaskListColumnSort16   215
#define cmdidTaskListColumnSort17   216
#define cmdidTaskListColumnSort18   217
#define cmdidTaskListColumnSort19   218
#define cmdidTaskListColumnSort20   219
#define cmdidTaskListColumnSort21   220
#define cmdidTaskListColumnSort22   221
#define cmdidTaskListColumnSort23   222
#define cmdidTaskListColumnSort24   223
#define cmdidTaskListColumnSort25   224

#define cmdidTaskListColumnGroup1 600
#define cmdidTaskListColumnGroup2 601
#define cmdidTaskListColumnGroup3 602
#define cmdidTaskListColumnGroup4 603
#define cmdidTaskListColumnGroup5 604
#define cmdidTaskListColumnGroup6 605
#define cmdidTaskListColumnGroup7 606
#define cmdidTaskListColumnGroup8 607
#define cmdidTaskListColumnGroup9 608
#define cmdidTaskListColumnGroup10 609
#define cmdidTaskListColumnGroup11 610
#define cmdidTaskListColumnGroup12 611
#define cmdidTaskListColumnGroup13 612
#define cmdidTaskListColumnGroup14 613
#define cmdidTaskListColumnGroup15 614
#define cmdidTaskListColumnGroup16 615
#define cmdidTaskListColumnGroup17 616
#define cmdidTaskListColumnGroup18 617
#define cmdidTaskListColumnGroup19 618
#define cmdidTaskListColumnGroup20 619
#define cmdidTaskListColumnGroup21 620
#define cmdidTaskListColumnGroup22 621
#define cmdidTaskListColumnGroup23 622
#define cmdidTaskListColumnGroup24 623
#define cmdidTaskListColumnGroup25 624

//////////////////////////////////////////////////////////////////
//
// Command ids for the ReferenceManager commands
//
//////////////////////////////////////////////////////////////////

#define cmdidReferenceManagerRecentContextMenu 0x1020
#define cmdidReferenceManagerRecentContextMenuGroup 0x1021
#define cmdidClearRecentReferences 0x100
#define cmdidRemoveFromRecentReferences 0x200

#endif //_STDIDCMD_H_
