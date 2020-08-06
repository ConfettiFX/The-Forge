/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/

#include <UIKit/UIKit.h>

#include "../Interfaces/ILog.h"
#include "../Interfaces/IMemory.h"
#include "../Interfaces/IFileSystem.h"


//FileWatcher* fsCreateFileWatcher(const Path* path, FileWatcherEventMask eventMask, FileWatcherCallback callback)
//{
//	LOGF(LogLevel::eERROR, "FileWatcher is unsupported on iOS.");
//	return NULL;
//}
//
//void fsFreeFileWatcher(FileWatcher* fileWatcher) { LOGF(LogLevel::eERROR, "FileWatcher is unsupported on iOS."); }
//
////-------------------------------------------------------------------------------------------------------------
//// CAUTION using URLs for file system directories!
////
//// We should resolve the symlinks:
////
//// iOS URLs contain a 'private' folder in the link when debugging on the device.
//// Without using URLByResolvingSymlinksInPath below, we would see the following scenario:
//// - directoryURL = file:///private/var/...
//// - filePathURL  = file:///var/...
//// In this case, the substring() logic will fail.
////
//
//#pragma mark - Document Browser Functionality
//
//@interface Document: UIDocument
//+ (void)createDocumentWithCompletionBlock:(void (^)(Document*))completionBlock fileExt:(NSString*)fileExtPtr;
//@end
//
//@implementation Document
//- (id)contentsForType:(NSString*)typeName error:(NSError**)errorPtr
//{
//	// Encode your document with an instance of NSData or NSFileWrapper
//	return [NSData alloc];
//}
//- (BOOL)loadFromContents:(id)contents ofType:(NSString*)typeName error:(NSError**)errorPtr
//{
//	// Load your document from contents
//	return YES;
//}
//+ (NSDateFormatter*)yyyyMMddHHmmssFormatter
//{
//	static NSDateFormatter* _formatter;
//	static dispatch_once_t  onceToken;
//	dispatch_once(&onceToken, ^{
//		_formatter = [[NSDateFormatter alloc] init];
//		_formatter.locale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US_POSIX"];
//		_formatter.dateFormat = @"yyyyMMdd-HHmmss";
//	});
//	return _formatter;
//}
//+ (void)createDocumentWithCompletionBlock:(void (^)(Document*))completionBlock fileExt:(NSString*)fileExtPtr
//{
//	dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(void) {
//		// unique filename
//		NSDate* today = [NSDate date];
//		__block NSString* datePrefix = [[self yyyyMMddHHmmssFormatter] stringFromDate:today];
//		id                fileName = [NSString stringWithFormat:@"%@%@%@", datePrefix, @"-gladiator", fileExtPtr];
//
//		// get a temp URL
//		NSString* tempPath = NSTemporaryDirectory();
//		NSString* tempFile = [tempPath stringByAppendingPathComponent:fileName];
//		NSURL*    docURL = [NSURL fileURLWithPath:tempFile];
//
//		// create the file
//		NSString* content = @"";
//		NSData*   fileContents = [content dataUsingEncoding:NSUTF8StringEncoding];
//		[[NSFileManager defaultManager] createFileAtPath:docURL.path contents:fileContents attributes:nil];
//		// allocate a Document with the created file
//		Document* doc = [[Document alloc] initWithFileURL:docURL];
//
//		dispatch_async(dispatch_get_main_queue(), ^{
//			if (completionBlock)
//				completionBlock(doc);
//		});
//	});
//}
//@end
//
//typedef void (*ActionPickDocumentCallbackFn)(NSURL*, void*, void*);
//
//API_AVAILABLE(ios(11.0))
//@interface DocumentBrowserViewController: UIDocumentBrowserViewController<UIDocumentBrowserViewControllerDelegate>
//@property(nonnull) ActionPickDocumentCallbackFn actionDocumentPicked;
//@property(nonnull) NSString*                    defaultSaveFileExt;
//@property(nonnull) void*                        callback;
//@property(nonnull) void*                        userData;
//@end
//
//@implementation DocumentBrowserViewController
//- (void)viewDidLoad
//{
//	[super viewDidLoad];
//
//	self.delegate = self;
//}
//
//#pragma mark UIDocumentBrowserViewControllerDelegate
//- (void)documentBrowser:(UIDocumentBrowserViewController*)controller
//	didRequestDocumentCreationWithHandler:(void (^)(NSURL* _Nullable, UIDocumentBrowserImportMode))importHandler
//{
//	// set the URL for the new document
//	// always call the importHandler, even if the user cancels the creation request
//	__block NSURL* newDocumentURL = nil;
//
//	@try
//	{
//		[Document
//			createDocumentWithCompletionBlock:^(Document* doc) {
//				if (doc != nil)
//				{
//					newDocumentURL = doc.fileURL;
//					importHandler(newDocumentURL, UIDocumentBrowserImportModeMove);
//				}
//				else
//				{
//					// show error
//					importHandler(newDocumentURL, UIDocumentBrowserImportModeNone);
//				}
//			}
//									  fileExt:_defaultSaveFileExt];
//	}
//	@catch (NSException* exception)
//	{
//		// show error
//		importHandler(newDocumentURL, UIDocumentBrowserImportModeNone);
//	}
//}
//
//- (void)documentBrowser:(UIDocumentBrowserViewController*)controller didPickDocumentsAtURLs:(nonnull NSArray<NSURL*>*)documentURLs
//{
//	// callback after user picks a file
//	NSURL* sourceURL = documentURLs.firstObject;
//	if (!sourceURL)
//		return;
//
//	// present the Document View Controller for the first document that was picked
//	[self presentDocumentAtURL:sourceURL title:@"RestoreOrImport"];
//}
//
//- (void)documentBrowser:(UIDocumentBrowserViewController*)controller
//	didImportDocumentAtURL:(NSURL*)sourceURL
//		  toDestinationURL:(NSURL*)destinationURL
//{
//	// callback after user clicks on (+) Create Document
//	// present the Document View Controller for the new newly created document
//	//	 [self presentDocumentAtURL:destinationURL title:@"BackupOrExport"];
//}
//
//- (void)documentBrowser:(UIDocumentBrowserViewController*)controller
//	failedToImportDocumentAtURL:(NSURL*)documentURL
//						  error:(NSError* _Nullable)error
//{
//	// handle the failed import
//	[self showError];
//}
//
//// MARK: Document Presentation
//- (void)doneTapped:(UIButton*)sender
//{
//	//print("returnToDocumentBrowser")
//	UINavigationController* nc = (UINavigationController*)[self presentedViewController];
//	if (nc)
//		[nc dismissViewControllerAnimated:NO
//							   completion:^{
//								   // dismiss view
//								   [self dismissViewControllerAnimated:NO completion:nil];
//							   }];
//}
//
//- (void)presentDocumentAtURL:(NSURL*)documentURL title:(NSString*)title
//{
//	// access the document
//	Document* doc = [[Document alloc] initWithFileURL:documentURL];
//	[doc openWithCompletionHandler:^(BOOL success) {
//		if (success)
//		{
//			self->_actionDocumentPicked(doc.fileURL, self->_callback, self->_userData);
//			// dismiss view
//			[self dismissViewControllerAnimated:NO completion:^{ [doc closeWithCompletionHandler:nil]; }];
//		}
//		else
//		{
//			// handle the failed access
//			[self showError];
//		}
//	}];
//}
//
//- (void)showError
//{
//	UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Error"
//																   message:@"open failed"
//															preferredStyle:UIAlertControllerStyleAlert];
//	UIAlertAction*     okButton = [UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil];
//	[alert addAction:okButton];
//	[self presentViewController:alert animated:YES completion:nil];
//}
//@end
//
//static NSMutableDictionary<NSString*, NSString*>* gExtensionToUtiId = nil;
//extern UIViewController*                          pMainViewController;
//static UIBarButtonItem*                           gDoneBtnItem;
//
//@interface DocumentBrowserCancelDelegate: NSObject
//{
//}
//- (void)cancelTapped:(UIButton*)sender;
//@end
//
//@implementation DocumentBrowserCancelDelegate
//- (void)cancelTapped:(UIButton*)sender
//{
//	UINavigationController* nc = (UINavigationController*)[pMainViewController presentedViewController];
//	if (nc)
//		[nc dismissViewControllerAnimated:YES completion:nil];
//}
//@end
//
//static DocumentBrowserCancelDelegate* gCancelDelegate;
//
//void on_document_picked(NSURL* url, void* callback, void* userData)
//{
//	FileDialogCallbackFn fn = (FileDialogCallbackFn)callback;
//	Path*                path = fsCreatePath(fsGetSystemFileSystem(), url.path.UTF8String);
//	fn(path, userData);
//	fsFreePath(path);
//}
//
//// Filtering for iOS requires the proper UTI identifier //(https://developer.apple.com/library/archive/documentation/FileManagement/Conceptual/understanding_utis/understand_utis_intro/understand_utis_intro.html).
//// One can figure out which identifier to use by utilizing the mdls command (https://www.real-world-systems.com/docs/mdfind.1.html#mdls).
//void fsRegisterUTIForExtension(const char* extension, const char* uti)
//{
//	if (!gExtensionToUtiId)
//	{
//		gExtensionToUtiId = [NSMutableDictionary dictionary];
//	}
//	NSString* extensionStr = [NSString stringWithUTF8String:extension];
//	gExtensionToUtiId[extensionStr] = [NSString stringWithUTF8String:uti];
//}
//
//API_AVAILABLE(ios(11.0))
//static void create_document_browser(
//	const char* title, void* callback, void* userData, NSArray* extList, NSString* defaultExt, bool allowDocumentCreation,
//	bool allowMultipleSelection)
//{
//	// create a cancel button
//	gCancelDelegate = [DocumentBrowserCancelDelegate alloc];
//	gDoneBtnItem = [[UIBarButtonItem alloc] initWithTitle:@"Cancel"
//													style:UIBarButtonItemStyleDone
//												   target:gCancelDelegate
//												   action:@selector(cancelTapped:)];
//
//	DocumentBrowserViewController* dbvc = [[DocumentBrowserViewController alloc] initForOpeningFilesWithContentTypes:extList];
//	dbvc.allowsDocumentCreation = allowDocumentCreation ? YES : NO;
//	dbvc.allowsPickingMultipleItems = allowMultipleSelection ? YES : NO;
//	dbvc.title = [NSString stringWithCString:title encoding:[NSString defaultCStringEncoding]];
//	dbvc.defaultSaveFileExt = defaultExt;
//	dbvc.actionDocumentPicked = on_document_picked;
//	dbvc.callback = (void*)callback;
//	dbvc.userData = (void*)userData;
//	dbvc.navigationItem.leftBarButtonItem = gDoneBtnItem;
//
//	UIViewController* dummyVC = [UIViewController alloc];
//	id                nc = [[UINavigationController alloc] initWithRootViewController:dummyVC];
//	// Hide main view
//	[pMainViewController presentViewController:nc animated:NO completion:nil];
//	// Push child view
//	[dummyVC.navigationController pushViewController:dbvc animated:NO];
//}
//
//void fsShowOpenFileDialog(
//	const char* title, const Path* directory, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
//	const char** fileExtensions, size_t fileExtensionCount)
//{
//	if (@available(iOS 11.0, *))
//	{
//		NSMutableArray* extensionsArray = [NSMutableArray arrayWithCapacity:fileExtensionCount];
//		if (fileExtensionCount > 0)
//		{
//			for (size_t i = 0; i < fileExtensionCount; i += 1)
//			{
//				NSString* extensionStr = [NSString stringWithUTF8String:fileExtensions[i]];
//				if (NSString* uti = gExtensionToUtiId[extensionStr])
//				{
//					extensionStr = uti;
//				}
//				[extensionsArray addObject:extensionStr];
//			}
//		}
//		create_document_browser(title, (void*)callback, (void*)userData, extensionsArray, nil, false, false);
//	}
//}
//
//void fsShowSaveFileDialog(
//	const char* title, const Path* directory, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
//	const char** fileExtensions, size_t fileExtensionCount)
//{
//	if (@available(iOS 11.0, *))
//	{
//		NSMutableArray* extensionsArray = [NSMutableArray arrayWithCapacity:fileExtensionCount];
//		if (fileExtensionCount > 0)
//		{
//			for (size_t i = 0; i < fileExtensionCount; i += 1)
//			{
//				NSString* extensionStr = [NSString stringWithUTF8String:fileExtensions[i]];
//				if (NSString* uti = gExtensionToUtiId[extensionStr])
//				{
//					extensionStr = uti;
//				}
//				[extensionsArray addObject:extensionStr];
//			}
//		}
//
//		// Create default extension string
//		NSString* defaultExt = nil;
//		if ([extensionsArray count] > 0)
//		{
//			defaultExt = [@"." stringByAppendingString:extensionsArray[0]];
//		}
//		create_document_browser(title, (void*)callback, (void*)userData, extensionsArray, defaultExt, true, false);
//	}
//}
//
