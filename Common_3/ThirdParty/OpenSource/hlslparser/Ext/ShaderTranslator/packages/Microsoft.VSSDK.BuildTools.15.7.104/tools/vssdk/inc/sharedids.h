#ifndef _SHAREDIDS_H_
#define _SHAREDIDS_H_

//////////////////////////////////////////////////////////////////////////////
//
// GUID Identifiers, created by WebBrowse package
//
//////////////////////////////////////////////////////////////////////////////
#ifndef NOGUIDS

#ifdef DEFINE_GUID
  // {83285929-227C-11d3-B870-00C04F79F802}
  DEFINE_GUID(Group_Undefined, 
    0x83285929, 0x227c, 0x11d3, 0xb8, 0x70, 0x0, 0xc0, 0x4f, 0x79, 0xf8, 0x2);

  // {8328592A-227C-11d3-B870-00C04F79F802}
  DEFINE_GUID(Pkg_Undefined, 
    0x8328592a, 0x227c, 0x11d3, 0xb8, 0x70, 0x0, 0xc0, 0x4f, 0x79, 0xf8, 0x2);

  // {8328592B-227C-11d3-B870-00C04F79F802}
  DEFINE_GUID(guidSharedCmd, 
    0x8328592b, 0x227c, 0x11d3, 0xb8, 0x70, 0x0, 0xc0, 0x4f, 0x79, 0xf8, 0x2);

  // {8328592C-227C-11d3-B870-00C04F79F802}
  DEFINE_GUID(guidSharedBmps, 
    0x8328592c, 0x227c, 0x11d3, 0xb8, 0x70, 0x0, 0xc0, 0x4f, 0x79, 0xf8, 0x2);

	// {52FD9855-984F-48af-99F2-B718F913FF02}
  DEFINE_GUID(guidSharedBmps2, 
    0x52fd9855, 0x984f, 0x48af, 0x99, 0xf2, 0xb7, 0x18, 0xf9, 0x13, 0xff, 0x2);

  // {DF81EA62-BAAB-4d89-B550-073BA96AD0A2}
  DEFINE_GUID(guidSharedBmps3, 
  0xdf81ea62, 0xbaab, 0x4d89, 0xb5, 0x50, 0x7, 0x3b, 0xa9, 0x6a, 0xd0, 0xa2);

  // {B155A99C-CBFC-4de4-B99A-ED6B1FB03217}
  DEFINE_GUID(guidSharedBmps4, 
  0xb155a99c, 0xcbfc, 0x4de4, 0xb9, 0x9a, 0xed, 0x6b, 0x1f, 0xb0, 0x32, 0x17);

  // {2BBED035-8A0C-4c19-8CD2-298937BEB38C}
  DEFINE_GUID(guidSharedBmps5, 
  0x2bbed035, 0x8a0c, 0x4c19, 0x8c, 0xd2, 0x29, 0x89, 0x37, 0xbe, 0xb3, 0x8c);

  // {EB28B762-7E54-492b-9336-4853994FE349}
  DEFINE_GUID(guidSharedBmps6, 
  0xeb28b762, 0x7e54, 0x492b, 0x93, 0x36, 0x48, 0x53, 0x99, 0x4f, 0xe3, 0x49);

  // {634F8946-FFF0-491f-AF41-B599FC20D561}
  DEFINE_GUID(guidSharedBmps7, 
  0x634f8946, 0xfff0, 0x491f, 0xaf, 0x41, 0xb5, 0x99, 0xfc, 0x20, 0xd5, 0x61);

  // {2B671D3D-AB51-434a-8D38-CBF1728530BB}
  DEFINE_GUID(guidSharedBmps8, 
  0x2b671d3d, 0xab51, 0x434a, 0x8d, 0x38, 0xcb, 0xf1, 0x72, 0x85, 0x30, 0xbb);

  // {222989A7-37A5-429f-AE43-8E9E960E7025}
  DEFINE_GUID(guidSharedBmps9, 
  0x222989a7, 0x37a5, 0x429f, 0xae, 0x43, 0x8e, 0x9e, 0x96, 0xe, 0x70, 0x25);

  // {3EA44CF4-2BBE-4d17-AA21-63B6A24BE9F6}
  DEFINE_GUID(guidSharedBmps10, 
  0x3ea44cf4, 0x2bbe, 0x4d17, 0xaa, 0x21, 0x63, 0xb6, 0xa2, 0x4b, 0xe9, 0xf6);

  // {7C9FA578-7C66-4495-98E6-1F5457E6C7AA}
  DEFINE_GUID(guidSharedBmps11, 
  0x7c9fa578, 0x7c66, 0x4495, 0x98, 0xe6, 0x1f, 0x54, 0x57, 0xe6, 0xc7, 0xaa);

  // guid for C# groups and menus (used because the IDM_VS_CTX_REFACTORING menu is defined under this GUID and is publically
  // exposed).
  // {5D7E7F65-A63F-46ee-84F1-990B2CAB23F9}
  DEFINE_GUID (guidCSharpGrpId, 0x5d7e7f65, 0xa63f, 0x46ee, 0x84, 0xf1, 0x99, 0xb, 0x2c, 0xab, 0x23, 0xf9);
#else

// {83285929-227C-11d3-B870-00C04F79F802}
#define Group_Undefined  { 0x83285929, 0x227c, 0x11d3, { 0xb8, 0x70, 0x00, 0xc0, 0x4f, 0x79, 0xf8, 0x02 } }

// {8328592A-227C-11d3-B870-00C04F79F802}
#define Pkg_Undefined    { 0x8328592a, 0x227c, 0x11d3, { 0xb8, 0x70, 0x0, 0xc0, 0x4f, 0x79, 0xf8, 0x2 } }

// {8328592B-227C-11d3-B870-00C04F79F802}
#define guidSharedCmd    { 0x8328592b, 0x227c, 0x11d3, { 0xb8, 0x70, 0x0, 0xc0, 0x4f, 0x79, 0xf8, 0x2 } }

// {8328592C-227C-11d3-B870-00C04F79F802}
#define guidSharedBmps   { 0x8328592c, 0x227c, 0x11d3, { 0xb8, 0x70, 0x0, 0xc0, 0x4f, 0x79, 0xf8, 0x2 } }

// {52FD9855-984F-48af-99F2-B718F913FF02}
#define guidSharedBmps2  { 0x52fd9855, 0x984f, 0x48af, { 0x99, 0xf2, 0xb7, 0x18, 0xf9, 0x13, 0xff, 0x2 } }

// {DF81EA62-BAAB-4d89-B550-073BA96AD0A2}
#define guidSharedBmps3 { 0xdf81ea62, 0xbaab, 0x4d89, { 0xb5, 0x50, 0x7, 0x3b, 0xa9, 0x6a, 0xd0, 0xa2 } }

// {B155A99C-CBFC-4de4-B99A-ED6B1FB03217}
#define guidSharedBmps4 { 0xb155a99c, 0xcbfc, 0x4de4, { 0xb9, 0x9a, 0xed, 0x6b, 0x1f, 0xb0, 0x32, 0x17 } }

// {2BBED035-8A0C-4c19-8CD2-298937BEB38C}
#define guidSharedBmps5 { 0x2bbed035, 0x8a0c, 0x4c19, { 0x8c, 0xd2, 0x29, 0x89, 0x37, 0xbe, 0xb3, 0x8c } }

// {EB28B762-7E54-492b-9336-4853994FE349}
#define guidSharedBmps6 { 0xeb28b762, 0x7e54, 0x492b, { 0x93, 0x36, 0x48, 0x53, 0x99, 0x4f, 0xe3, 0x49 } }

// {634F8946-FFF0-491f-AF41-B599FC20D561}
#define guidSharedBmps7 { 0x634f8946, 0xfff0, 0x491f, { 0xaf, 0x41, 0xb5, 0x99, 0xfc, 0x20, 0xd5, 0x61 } }

// {2B671D3D-AB51-434a-8D38-CBF1728530BB}
#define guidSharedBmps8 { 0x2b671d3d, 0xab51, 0x434a, { 0x8d, 0x38, 0xcb, 0xf1, 0x72, 0x85, 0x30, 0xbb } }

// {222989A7-37A5-429f-AE43-8E9E960E7025}
#define guidSharedBmps9 { 0x222989a7, 0x37a5, 0x429f, { 0xae, 0x43, 0x8e, 0x9e, 0x96, 0xe, 0x70, 0x25 } }

// {3EA44CF4-2BBE-4d17-AA21-63B6A24BE9F6}
#define guidSharedBmps10 { 0x3ea44cf4, 0x2bbe, 0x4d17, { 0xaa, 0x21, 0x63, 0xb6, 0xa2, 0x4b, 0xe9, 0xf6 } }

// {7C9FA578-7C66-4495-98E6-1F5457E6C7AA}
#define guidSharedBmps11 { 0x7c9fa578, 0x7c66, 0x4495, { 0x98, 0xe6, 0x1f, 0x54, 0x57, 0xe6, 0xc7, 0xaa } }

// {5D7E7F65-A63F-46ee-84F1-990B2CAB23F9}
#define guidCSharpGrpId { 0x5D7E7F65, 0xA63F, 0x46ee, { 0x84, 0xF1, 0x99, 0x0B, 0x2C, 0xAB, 0x23, 0xF9 } }

#endif //DEFINE_GUID

#endif //NOGUIDS


///////////////////////////////////////////////////////////////////////////////
// Command IDs


////////////////////////////////////////////////////////////////
// BITMAPS
////////////////////////////////////////////////////////////////
// guidSharedBmps
////////////////////////////////////////////////////////////////
#define bmpidVisibleBorders				1
#define bmpidShowDetails				2
#define bmpidMake2d						3
#define bmpidLockElement				4
#define bmpid2dDropMode					5
#define bmpidSnapToGrid					6
#define bmpidForeColor					7
#define bmpidBackColor					8
#define bmpidScriptOutline				9
#define bmpidDisplay1D					10
#define bmpidDisplay2D					11
#define bmpidInsertLink					12
#define bmpidInsertBookmark				13
#define bmpidInsertImage				14
#define bmpidInsertForm					15
#define bmpidInsertDiv					16
#define bmpidInsertSpan					17
#define bmpidInsertMarquee				18
#define bmpidOutlineHTML				19
#define bmpidOutlineScript				20
#define	bmpidShowGrid					21
#define bmpidCopyWeb                    22
#define bmpidHyperLink					23
#define bmpidSynchronize                24
#define bmpidIsolatedMode               25
#define bmpidDirectMode                 26
#define bmpidDiscardChanges             27
#define bmpidGetWorkingCopy             28
#define bmpidReleaseWorkingCopy         29
#define bmpidGet                        30
#define	bmpidShowAllFiles				31
#define bmpidStopNow                    32
#define bmpidBrokenLinkReport           33
#define bmpidAddDataCommand             34
#define bmpidRemoveWebFromScc           35
//
#define	bmpidAddPageFromFile			36
#define	bmpidOpenTopic					37
#define	bmpidAddBlankPage				38
#define	bmpidEditTitleString			39
#define	bmpidChangeNodeURL				40
//
#define bmpidDeleteTable                41
#define bmpidSelectTable                42
#define bmpidSelectColumn               43
#define bmpidSelectRow                  44
#define bmpidSelectCell                 45

#define bmpidAddNewWebForm              46
#define bmpidAddNewHTMLPage             47
#define bmpidAddNewWebService           48
#define bmpidAddNewComponent            49
#define bmpidaddNewModule               50
#define bmpidAddNewForm                 51
#define bmpidAddNewInheritedForm        52
#define bmpidAddNewUserControl          53
#define bmpidAddNewInheritedUserControl 54
#define bmpidAddNewXSDSchema            55
#define bmpidAddNewXMLPage              56
#define bmpidNewLeftFrame               57
#define bmpidNewRightFrame              58
#define bmpidNewTopFrame                59
#define bmpidNewBottomFrame             60
#define bmpidNewWebUserControl          61
//
#define bmpidCompile                    62
#define bmpidStartWebAdminTool          63
#define bmpidNestRelatedFiles           64
#define bmpidGenPageResource            65

////////////////////////////////////////////////////////////////
// guidSharedBmps2
////////////////////////////////////////////////////////////////
#define bmpid2Filter                     1
#define bmpid2EventLog                   2
#define bmpid2View                       3
#define bmpid2TimelineViewer             4
#define bmpid2BlockDiagramViewer         5
#define bmpid2MultipleEventViewer        6
#define bmpid2SingleEventViewer          7
#define bmpid2SummaryViewer              8
#define bmpid2ChartViewer                9
#define bmpid2AddMachine                10
#define bmpid2AddFilter                 11
#define bmpid2EditFilter                12
#define bmpid2ApplyFilter               13
#define bmpid2StartCollecting           14
#define bmpid2StopCollecting            15
#define bmpid2IncreaseSpeed             16
#define bmpid2DecreaseSpeed             17
#define bmpid2Unknown1                  18
#define bmpid2FirstRecord               19
#define bmpid2PrevRecord                20
#define bmpid2NextRecord                21
#define bmpid2LastRecord                22
#define bmpid2Play                      23
#define bmpid2Stop                      24
#define bmpid2Duplicate                 25
#define bmpid2Export                    26
#define bmpid2Import                    27
#define bmpid2PlayFrom                  28
#define bmpid2PlayTo                    29
#define bmpid2Goto                      30
#define bmpid2ZoomToFit                 31
#define bmpid2AutoFilter                32
#define bmpid2AutoSelect                33
#define bmpid2AutoPlayTrack             34
#define bmpid2ExpandSelection           35
#define bmpid2ContractSelection         36
#define bmpid2PauseRecording            37
#define bmpid2AddLog                    38
#define bmpid2Connect                   39
#define bmpid2Disconnect                40
#define bmpid2MachineDiagram            41
#define bmpid2ProcessDiagram            42
#define bmpid2ComponentDiagram          43
#define bmpid2StructureDiagram          44
////////////////////////////////////////////////////////////////
// guidSharedBmps3
////////////////////////////////////////////////////////////////
#define bmpid3FileSystemEditor           1
#define bmpid3RegistryEditor             2
#define bmpid3FileTypesEditor            3
#define bmpid3UserInterfaceEditor        4
#define bmpid3CustomActionsEditor        5
#define bmpid3LaunchConditionsEditor     6
////////////////////////////////////////////////////////////////
// guidSharedBmps4
////////////////////////////////////////////////////////////////
#define bmpid4FldView                    1
#define bmpid4SelExpert                  2
#define bmpid4TopNExpert                 3
#define bmpid4SortOrder                  4
#define bmpid4PropPage                   5
#define bmpid4Help                       6
#define	bmpid4SaveRpt                    7
#define bmpid4InsSummary                 8
#define bmpid4InsGroup                   9
#define bmpid4InsSubreport              10
#define bmpid4InsChart                  11
#define bmpid4InsPicture                12
#define bmpid4SortCategory              13
////////////////////////////////////////////////////////////////
// guidSharedBmps5
////////////////////////////////////////////////////////////////
#define bmpid5AddDataConn                1
////////////////////////////////////////////////////////////////
// guidSharedBmps6
////////////////////////////////////////////////////////////////
#define bmpid6ViewFieldList              1
#define bmpid6ViewGrid                   2
#define bmpid6ViewKeys                   3
#define bmpid6ViewCollapsed              4
#define bmpid6Remove                     5
#define bmpid6Refresh                    6
#define bmpid6ViewUserDefined            7
#define bmpid6ViewPageBreaks             8
#define bmpid6RecalcPageBreaks           9
#define bmpid6ZoomToFit                 10
#define bmpid6DeleteFromDB              11
////////////////////////////////////////////////////////////////
// guidSharedBmps7
////////////////////////////////////////////////////////////////
#define bmpid7SelectQuery                1
#define bmpid7InsertQuery                2
#define bmpid7UpdateQuery                3
#define bmpid7DeleteQuery                4
#define bmpid7SortAsc                    5
#define bmpid7SortDesc                   6
#define bmpid7RemoveFilter               7
#define bmpid7VerifySQL                  8
#define bmpid7RunQuery                   9
#define bmpid7DiagramPane               10
#define bmpid7GridPane                  11
#define bmpid7ResultsPane               12
#define bmpid7SQLPane                   13
#define bmpid7Totals                    14
#define bmpid7MakeTableQuery            15
#define bmpid7InsertValuesQuery         16
#define bmpid7RowFirst                  17
#define bmpid7RowLast                   18
#define bmpid7RowNext                   19
#define bmpid7RowPrevious               20
#define bmpid7RowNew                    21
#define bmpid7RowDelete                 22
#define bmpid7GenerateSQL               23	
#define bmpid7JoinLeftAll               24
#define bmpid7JoinRightAll              25
#define bmpid7RowGoto                   26
#define bmpid7ClearQuery                27
#define bmpid7QryManageIndexes          28
////////////////////////////////////////////////////////////////
// guidSharedBmps8
////////////////////////////////////////////////////////////////
#define bmpid8NewTable                   1
#define bmpid8SaveChangeScript           2
#define bmpid8PrimaryKey                 3
#define bmpid8LayoutDiagram              4
#define bmpid8LayoutSelection            5
#define bmpid8AddRelatedTables           6
#define bmpid8NewTextAnnotation          7
#define bmpid8InsertCol                  8
#define bmpid8DeleteCol                  9
#define bmpid8ShowRelLabels             10
#define bmpid8AutosizeSelTables         11
#define	bmpid8SaveSelection             12
#define bmpid8EditUDV                   13
#define bmpid8AddTableView              14
#define bmpid8ManangeIndexes            15
#define bmpid8ManangeConstraints        16
#define bmpid8ManangeRelationships      17
#define bmpid8AddDerivedTable 		18
#define bmpid8Navigate 			19
////////////////////////////////////////////////////////////////
// guidSharedBmps9
////////////////////////////////////////////////////////////////
#define bmpid9NewElement                 1
#define bmpid9NewSimpleType              2
#define bmpid9NewComplexType             3
#define bmpid9NewAttribute               4
#define bmpid9NewGroup                   5
#define bmpid9NewAttributeGroup          6
#define bmpid9Diamond                    7
#define bmpid9NewAnyAttribute            8
#define bmpid9NewKey                     9
#define bmpid9NewRelation               10
#define bmpid9EditKey                   11
#define bmpid9EditRelation              12
#define bmpid9MakeTypeGlobal            13
#define bmpid9CreateSchema              14
#define bmpid9PreviewDataSet            15
#define bmpid9NewFacet                  16
#define bmpid9ValidateHtmlData          17
#define bmpid9DataPreview               18
#define bmpid9DataGenerateDataSet       19
#define bmpid9DataGenerateMethods       20
////////////////////////////////////////////////////////////////
// guidSharedBmps10
////////////////////////////////////////////////////////////////
#define bmpid10NewDialog                 1
#define bmpid10NewMenu                   2
#define bmpid10NewCursor                 3
#define bmpid10NewIcon                   4
#define bmpid10NewBitmap                 5
#define bmpid10NewToolbar                6
#define bmpid10NewAccel                  7
#define bmpid10NewString                 8
#define bmpid10NewVersion                9
#define bmpid10ResourceInc              10
//
#define bmpid10DlgTest                  12
//
#define bmpid10CenterVert               17
#define bmpid10CenterHorz               18
#define bmpid10SpaceAcross              19
#define bmpid10SpaceDown                20
//
#define bmpid10ToggleGrid               24
#define bmpid10ToggleGuides             25
//
#define bmpid10CheckMnemonics           27
#define bmpid10AutoLayoutGrow           28
#define bmpid10AutoLayoutOptimize       29
#define bmpid10AutoLayoutNoResize       30
////////////////////////////////////////////////////////////////
// guidSharedBmps11
////////////////////////////////////////////////////////////////
#define bmpid11Pick                      1
#define bmpid11PickRegion                2
#define bmpid11PickColor                 3
#define bmpid11Eraser                    4
#define bmpid11Fill                      5
#define bmpid11Zoom                      6
#define bmpid11Pencil                    7
#define bmpid11Brush                     8
#define bmpid11AirBrush                  9
#define bmpid11Line                     10
#define bmpid11Curve                    11
#define bmpid11Text                     12
#define bmpid11Rect                     13
#define bmpid11OutlineRect              14
#define bmpid11FilledRect               15
#define bmpid11RoundedRect              16
#define bmpid11OutlineRoundedRect       17
#define bmpid11FilledRoundedRect        18
#define bmpid11Ellipse                  19
#define bmpid11OutlineEllipse           20
#define bmpid11FilledEllipse            21
#define bmpid11HotSpot                  22
#define bmpid11EraserSize1              23
#define bmpid11EraserSize2              24
#define bmpid11EraserSize3              25
#define bmpid11EraserSize4              26
#define bmpid11LineWidth1               27
#define bmpid11LineWidth2               28
#define bmpid11LineWidth3               29
#define bmpid11LineWidth4               30
#define bmpid11LineWidth5               31
#define bmpid11LargeCircle              32
#define bmpid11MediumCircle             33
#define bmpid11SmallCircle              34
#define bmpid11SmallSquare              35
#define bmpid11LeftDiagLarge            36
#define bmpid11LeftDiagMedium           37
#define bmpid11LeftDiagSmall            38
#define bmpid11RightDiagLarge           39
#define bmpid11RightDiagMedium          40
#define bmpid11RightDiagSmall           41
#define bmpid11SplashSmall              42
#define bmpid11SplashMedium             43
#define bmpid11SplashLarge              44
#define bmpid11Transparent              45
#define bmpid11Opaque                   46
#define bmpid11Zoom1x                   47
#define bmpid11Zoom2x                   48
#define bmpid11Zoom6x                   49
#define bmpid11Zoom8x                   50
#define bmpid11ColorWindow              51
#define bmpid11ResView                  52
// These two were removed from the bitmap strip
//#define bmpid11Flip                     53
//#define bmpid11Stretch                  54
//
#define bmpid11NewImageType             53
#define bmpid11ImageOptions		54

#endif //_SHAREDIDS_H_
