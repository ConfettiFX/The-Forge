/*-------------------------------------------------------------------------------
Microsoft Visual Studio Enterprise Edition

Namespace: None
Subsystem: Visual Studio Source Code Control
Copyright: (c) 1997-2000, Microsoft Corporation
           All Rights Reserved

@doc	internal

@module sccmnid.h - SCC Package Menu IDs |

-------------------------------------------------------------------------------*/

// Can't use pragma once here, as this passes through ctc
#ifndef SccMnID_H_Included
#define SccMnID_H_Included

// Note that we have code that depends on the adjacency of the context and non-context
// versions of the commands, and also upon the odd/even dichotomy

#define icmdFlagContext							1

#define icmdSccAdd								21000
#define icmdSccContextAdd						21001	// (icmdSccAdd+icmdFlagContext)
#define icmdSccCheckout							21002
#define icmdSccContextCheckout					21003	// (icmdSccCheckout+icmdFlagContext)
#define icmdSccCheckoutShared					21004
#define icmdSccContextCheckoutShared			21005	// (icmdSccCheckoutShared+icmdFlagContext)
#define icmdSccCheckoutExclusive				21006
#define icmdSccContextCheckoutExclusive			21007	// (icmdSccCheckoutExclusive+icmdFlagContext)
#define icmdSccUndoCheckout						21008
#define icmdSccContextUndoCheckout				21009	// (icmdSccUndoCheckout+icmdFlagContext)
#define icmdSccGetLatestVersion					21010	
#define icmdSccContextGetLatestVersion			21011	// (icmdSccGetLatestVersion+icmdFlagContext)	
#define icmdSccShowNonEmptyCheckinWindow		21012
#define icmdSccContextShowNonEmptyCheckinWindow	21013	// (icmdSccShowNonEmptyCheckinWindow+icmdFlagContext)
#define icmdSccCheckin							21014
#define icmdSccContextCheckin					21015	// (icmdSccCheckin+icmdFlagContext)

// The order for the "Add" commands are important because they are used as a range
#define icmdSccAddSolution						21016	
#define icmdSccContextAddSolution				21017	// (icmdSccAddSolution + icmdFlagContext)
#define icmdSccAddSelection						21018	
#define icmdSccContextAddSelection				21019	// (icmdSccAddSelection + icmdFlagContext)

#define icmdSccShelve							21020
#define icmdSccContextShelve					21021	// (icmdSccShelve + icmdFlagContext)

#define icmdSccGetVersion						21500
#define icmdSccContextGetVersion				21501  // (icmdSccGetVersion + icmdFlagContext)
#define icmdSccShowCheckinWindow				21502
#define icmdSccProperties						21504
#define icmdSccDiff								21506
#define icmdSccHistory							21508
#define icmdSccShare							21510
#define icmdSccRemove							21512
#define icmdSccAdmin							21514
#define icmdSccRefreshStatus					21516
#define icmdSccRename							21518
#define icmdSccSetLocation						21520

#define icmdSccOpenFromSourceControl						21522
#define icmdSccAddSelectionWithSolution						21524	// "Virtual provider" - the same provider as the current solution has
#define icmdSccShowConnectionManager						21526

#define icmdSccAddFromSourceControlSingleProvider			21536	// AddFromSC with a single versioning provider

#define igrpSccMainAdd										22000	// IDG_SCC_ADD 28
#define	igrpSccMainCommands									22001	
#define	igrpSccMainAction									22002	// IDG_SCC_MAIN 26
#define	igrpSccMainSecondary								22003	// IDG_SCC_MAIN2 30
#define	igrpSccMainAdmin									22004	// IDG_SCC_MAIN3 31
#define	igrpSccCommands										22005	// IDG_SCC_SUBMENU 29
#define	igrpSccEditorContext								22006	// IDG_SCC_CTXT_EDIT 32
#define igrpSccOpenFromSourceControl						22007
#define igrpSccOpenFromSourceControlProviders				22008
#define igrpSccAddSolutionToSourceControlProviders			22009
#define igrpSccAddSelectionToSourceControlProviders			22010

#define igrpSccSccAddSelectionWithSolution					22011
#define igrpSccOpenFromSourceControlMSSCCIProvider			22012
#define igrpSccAddSolutionToSourceControlMSSCCIProvider		22013
#define igrpSccAddSelectionToSourceControlMSSCCIProvider	22014
#define igrpSccAddFromSourceControl							22015
#define igrpSccAddFromSourceControlMSSCCIProvider			22016
#define igrpSccAddFromSourceControlProviders				22017
												
#define imnuSccMenu											23000	// IDM_VS_MENU_SCC 18 
#define imnuSccOpenFromSourceControl						23001
#define imnuSccAddSolutionToSourceControl					23002
#define imnuSccAddSelectionToSourceControl					23003
#define imnuSccAddFromSourceControl							23004
												
#define itbrSccToolbar										24000	// IDM_VS_TOOL_SCC 17 

#ifdef DEFINE_GUID // presumably compiling code, not ctc.

DEFINE_GUID(guidSccPkg,
0xAA8EB8CD, 0x7A51, 0x11D0, 0x92, 0xC3, 0x00, 0xA0, 0xC9, 0x13, 0x8C, 0x45);

// {53544C4D-C4AD-4998-9808-00935EA47729}
DEFINE_GUID(guidSccOpenFromSourceControl, 
0x53544C4D, 0xc4ad, 0x4998, 0x98, 0x8, 0x0, 0x93, 0x5e, 0xa4, 0x77, 0x29);

// {53544C4D-0E51-4941-83F6-29423FED03EF}
DEFINE_GUID(guidSccAddSolutionToSourceControl, 
0x53544C4D, 0xe51, 0x4941, 0x83, 0xf6, 0x29, 0x42, 0x3f, 0xed, 0x3, 0xef);

// {53544C4D-5DAE-4c96-A292-5057FD62BCC2}
DEFINE_GUID(guidSccAddSelectionToSourceControl, 
0x53544C4D, 0x5dae, 0x4c96, 0xa2, 0x92, 0x50, 0x57, 0xfd, 0x62, 0xbc, 0xc2);

// {53544C4D-7D04-46b0-87D4-35A81DC2FEFC}
DEFINE_GUID(guidSccAddFromSourceControl, 
0x53544C4D, 0x7d04, 0x46b0, 0x87, 0xd4, 0x35, 0xa8, 0x1d, 0xc2, 0xfe, 0xfc);

// {53544C4D-3BF2-4b83-A468-295691EB8609}
DEFINE_GUID(guidSccViewTeamExplorer, 
0x53544C4D, 0x3bf2, 0x4b83, 0xa4, 0x68, 0x29, 0x56, 0x91, 0xeb, 0x86, 0x9);

// {53544C4D-3BF3-4b83-A468-295691EB8609}
DEFINE_GUID(guidSccViewVisualComponentManager, 
0x53544C4D, 0x3bf3, 0x4b83, 0xa4, 0x68, 0x29, 0x56, 0x91, 0xeb, 0x86, 0x9);

#else // ctc

#define guidSccPkg { \
0xAA8EB8CD, 0x7A51, 0x11D0, { 0x92, 0xC3, 0x00, 0xA0, 0xC9, 0x13, 0x8C, 0x45 }}

// {53544C4D-C4AD-4998-9808-00935EA47729}
#define guidSccOpenFromSourceControl { \
0x53544C4D, 0xC4Ad, 0x4998, { 0x98, 0x08, 0x00, 0x93, 0x5E, 0xA4, 0x77, 0x29 }}

// {53544C4D-0E51-4941-83F6-29423FED03EF}
#define guidSccAddSolutionToSourceControl { \
0x53544C4D, 0x0E51, 0x4941, { 0x83, 0xF6, 0x29, 0x42, 0x3F, 0xED, 0x03, 0xEF }}

// {53544C4D-5DAE-4c96-A292-5057FD62BCC2}
#define guidSccAddSelectionToSourceControl { \
0x53544C4D, 0x5DAE, 0x4C96, { 0xA2, 0x92, 0x50, 0x57, 0xFD, 0x62, 0xBC, 0xC2 }}

// {53544C4D-7D04-46b0-87D4-35A81DC2FEFC}
#define guidSccAddFromSourceControl { \
0x53544C4D, 0x7d04, 0x46b0, { 0x87, 0xd4, 0x35, 0xa8, 0x1d, 0xc2, 0xfe, 0xfc }}

// {53544C4D-3BF2-4b83-A468-295691EB8609}
#define guidSccViewTeamExplorer { \
0x53544C4D, 0x3bf2, 0x4b83, { 0xa4, 0x68, 0x29, 0x56, 0x91, 0xeb, 0x86, 0x9	}}

// {53544C4D-3BF3-4b83-A468-295691EB8609}
#define guidSccViewVisualComponentManager { \
0x53544C4D, 0x3bf3, 0x4b83, { 0xa4, 0x68, 0x29, 0x56, 0x91, 0xeb, 0x86, 0x9	}}

#endif // DEFINE_GUID

#endif // #pragma once
