/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "../../ThirdParty/OpenSource/TinySTL/unordered_map.h"

#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IMemoryManager.h"

#include <sys/stat.h>
#include <unistd.h>

#define RESOURCE_DIR "Shaders/Metal"

const char* pszRoots[FSR_Count] = {
	RESOURCE_DIR "/Binary/",    // FSR_BinShaders
	RESOURCE_DIR "/",           // FSR_SrcShaders
	"Textures/",                // FSR_Textures
	"Meshes/",                  // FSR_Meshes
	"Fonts/",                   // FSR_Builtin_Fonts
	"GPUCfg/",                  // FSR_GpuConfig
	"Animation/",               // FSR_Animation
	"",                         // FSR_OtherFiles
};

//-------------------------------------------------------------------------------------------------------------
// CAUTION using URLs for file system directories!
//
// We should resolve the symlinks:
//
// iOS URLs contain a 'private' folder in the link when debugging on the device.
// Without using URLByResolvingSymlinksInPath below, we would see the following scenario:
// - directoryURL = file:///private/var/...
// - filePathURL  = file:///var/...
// In this case, the substring() logic will fail.
//
//-------------------------------------------------------------------------------------------------------------
FileHandle open_file(const char* filename, const char* flags)
{
#if 0    // OLD IMPLEMENTATION HERE FOR REFERENCE
	//first we need to look into main bundle which has read-only access
	NSString * fileUrl = [[NSBundle mainBundle] pathForResource:[NSString stringWithUTF8String:filename] ofType:@""];

	//there is no write permission for files in bundle,
	//iOS can only write in documents
	//if 'w' flag is present then look in documents folder of Application (Appdata equivalent)
	//if file has been found in bundle but we want to write to it that means we have to use the one in Documents.
	if(strstr(flags,"w") != NULL)
	{
		NSString * documentsDirectory = [NSHomeDirectory() stringByAppendingPathComponent:@"Documents"];
		fileUrl = [documentsDirectory stringByAppendingString:[NSString stringWithUTF8String:filename]];
	}

	//No point in calling fopen if file path is null
	if(fileUrl == nil)
	{
		LOGWARNINGF("Path '%s' is invalid!", filename);
		return NULL;
	}
	
	filename = [fileUrl fileSystemRepresentation];
	//LOGINFOF("Open File: %s", filename); // debug log
#endif

	// iOS file system has security measures for directories hence not all directories are writeable by default.
	// The executable directory (the bundle directory) is one of the write-protected directories.
	// iOS file system has pre-defined folders that are write-able. Documents is one of them.
	// Hence, if we request to open a file with write permissions, we will use Documents for that.
	if (strstr(flags, "w") != NULL)
	{
		const tinystl::string currDir = get_current_dir();
		tinystl::string       strFileName(filename);

		// @filename will have absolute path before being passed into this function.
		// if we want to write, we want to change the directory. Hence, strip away the
		// resolved bundle path (which as prepended to the actual file name the App wants to open)
		// and prepend the 'Documents' folder to the filename
		const unsigned findPos = strFileName.find(currDir, 0);
		if (findPos != tinystl::string::npos)
		{
			strFileName.replace(currDir, "");
		}

		NSString*       documentsDirectory = [NSHomeDirectory() stringByAppendingPathComponent:@"Documents"];
		tinystl::string docDirStr([documentsDirectory UTF8String]);

		// Don't want to append if already in the incoming file name
		if (strFileName.find_last(docDirStr) == tinystl::string::npos)
			filename =
				[[documentsDirectory stringByAppendingString:[NSString stringWithUTF8String:strFileName.c_str()]] fileSystemRepresentation];
	}

	FILE* fp = fopen(filename, flags);
	return fp;    // error logging is done from the caller in case fp == NULL
}

bool close_file(FileHandle handle)
{
	//ASSERT(FALSE);
	return (fclose((::FILE*)handle) == 0);
}

void flush_file(FileHandle handle)
{    // TODO: use NSBundle
	fflush((::FILE*)handle);
}

size_t read_file(void* buffer, size_t byteCount, FileHandle handle)
{    // TODO: use NSBundle
	return fread(buffer, 1, byteCount, (::FILE*)handle);
}

bool seek_file(FileHandle handle, long offset, int origin)
{    // TODO: use NSBundle
	return fseek((::FILE*)handle, offset, origin) == 0;
}

long tell_file(FileHandle handle)
{    // TODO: use NSBundle
	return ftell((::FILE*)handle);
}

size_t write_file(const void* buffer, size_t byteCount, FileHandle handle)
{    // TODO: use NSBundle
	return fwrite(buffer, 1, byteCount, (::FILE*)handle);
}

tinystl::string get_current_dir()
{
	return tinystl::string([[[NSBundle mainBundle] bundlePath] cStringUsingEncoding:NSUTF8StringEncoding]);
}

tinystl::string get_exe_path()
{
	const char*     exeDir = [[[[NSBundle mainBundle] bundlePath] stringByStandardizingPath] cStringUsingEncoding:NSUTF8StringEncoding];
	tinystl::string str(exeDir);
	return str;
}

size_t get_file_last_modified_time(const char* _fileName)
{    // TODO: use NSBundle
	struct stat fileInfo;

	if (!stat(_fileName, &fileInfo))
	{
		return (size_t)fileInfo.st_mtime;
	}
	else
	{
		// return an impossible large mod time as the file doesn't exist
		return ~0;
	}
}

tinystl::string get_app_prefs_dir(const char* org, const char* app)
{
	ASSERT(false && "Unsupported on target iOS");
	return tinystl::string();
}

tinystl::string get_user_documents_dir()
{
	ASSERT(false && "Unsupported on target iOS");
	return tinystl::string();
}

void set_current_dir(const char* path)
{    // TODO: use NSBundle
	chdir(path);
}

void get_files_with_extension(const char* dir, const char* ext, tinystl::vector<tinystl::string>& filesIn)
{
	if (!dir || strlen(dir) == 0)
	{
		LOGWARNINGF("%s directory passed as argument!", (!dir ? "NULL" : "Empty"));
		return;
	}

	// Use Paths instead of URLs
	NSFileManager* fileMan = [NSFileManager defaultManager];
	NSString*      pStrDir = [NSString stringWithUTF8String:dir];
	NSArray*       pathFragments = [NSArray arrayWithObjects:[[NSBundle mainBundle] bundlePath], pStrDir, nil];
	NSString*      pStrSearchDir = [NSString pathWithComponents:pathFragments];

	BOOL isDir = YES;
	if (![fileMan fileExistsAtPath:pStrSearchDir isDirectory:&isDir])
	{
		LOGERRORF("Directory '%s' doesn't exist.", dir);
		return;
	}
	NSArray* pContents = [fileMan subpathsAtPath:pStrSearchDir];

	const char* extension = ext;
	if (ext[0] == '.')
	{
		extension = ext + 1;    // get the next address after '.'
	}

	// predicate for querying files with extension 'ext'
	NSString* formattedPredicate = @"pathExtension == '";
	formattedPredicate = [formattedPredicate stringByAppendingString:[NSString stringWithUTF8String:extension]];
	formattedPredicate = [formattedPredicate stringByAppendingString:@"'"];
	NSPredicate* filePredicate = [NSPredicate predicateWithFormat:formattedPredicate];

	// save the files with the given extension in the output container
	for (NSString* filePath in [pContents filteredArrayUsingPredicate:filePredicate])
	{
		pathFragments = [NSArray arrayWithObjects:pStrDir, filePath, nil];
		NSString* fullFilePath = [NSString pathWithComponents:pathFragments];
		filesIn.push_back([fullFilePath cStringUsingEncoding:NSUTF8StringEncoding]);
	}
}

bool file_exists(const char* fileFullPath)
{
	NSFileManager* fileMan = [NSFileManager defaultManager];
	BOOL           isDir = NO;
	BOOL           fileExists = [fileMan fileExistsAtPath:[NSString stringWithUTF8String:fileFullPath] isDirectory:&isDir];
	return fileExists ? true : false;
}

bool absolute_path(const char* fileFullPath) { return (([NSString stringWithUTF8String:fileFullPath].absolutePath == YES) ? true : false); }

bool copy_file(const char* src, const char* dst)
{
	NSError* error = nil;
	if (NO == [[NSFileManager defaultManager] copyItemAtPath:[NSString stringWithUTF8String:src]
													  toPath:[NSString stringWithUTF8String:dst]
													   error:&error])
	{
		LOGINFOF("Failed to copy file with error : %s", [[error localizedDescription] UTF8String]);
		return false;
	}

	return true;
}
/************************************************************************/
// Document Browser Functionality
/************************************************************************/
@interface Document: UIDocument
+ (void)createDocumentWithCompletionBlock:(void (^)(Document*))completionBlock fileExt:(NSString*)fileExtPtr;
@end

@implementation Document
- (id)contentsForType:(NSString*)typeName error:(NSError**)errorPtr
{
	// Encode your document with an instance of NSData or NSFileWrapper
	return [NSData alloc];
}
- (BOOL)loadFromContents:(id)contents ofType:(NSString*)typeName error:(NSError**)errorPtr
{
	// Load your document from contents
	return YES;
}
+ (NSDateFormatter*)yyyyMMddHHmmssFormatter
{
	static NSDateFormatter* _formatter;
	static dispatch_once_t  onceToken;
	dispatch_once(&onceToken, ^{
		_formatter = [[NSDateFormatter alloc] init];
		_formatter.locale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US_POSIX"];
		_formatter.dateFormat = @"yyyyMMdd-HHmmss";
	});
	return _formatter;
}
+ (void)createDocumentWithCompletionBlock:(void (^)(Document*))completionBlock fileExt:(NSString*)fileExtPtr
{
	dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(void) {
		// unique filename
		NSDate* today = [NSDate date];
		__block NSString* datePrefix = [[self yyyyMMddHHmmssFormatter] stringFromDate:today];
		id                fileName = [NSString stringWithFormat:@"%@%@%@", datePrefix, @"-gladiator", fileExtPtr];

		// get a temp URL
		NSString* tempPath = NSTemporaryDirectory();
		NSString* tempFile = [tempPath stringByAppendingPathComponent:fileName];
		NSURL*    docURL = [NSURL fileURLWithPath:tempFile];

		// create the file
		NSString* content = @"";
		NSData*   fileContents = [content dataUsingEncoding:NSUTF8StringEncoding];
		[[NSFileManager defaultManager] createFileAtPath:docURL.path contents:fileContents attributes:nil];
		// allocate a Document with the created file
		Document* doc = [[Document alloc] initWithFileURL:docURL];

		dispatch_async(dispatch_get_main_queue(), ^{
			if (completionBlock)
				completionBlock(doc);
		});
	});
}
@end

typedef void (*ActionPickDocumentCallbackFn)(NSURL*, void*, void*);

@interface DocumentBrowserViewController: UIDocumentBrowserViewController<UIDocumentBrowserViewControllerDelegate>
@property(nonnull) ActionPickDocumentCallbackFn actionDocumentPicked;
@property(nonnull) NSString*                    defaultSaveFileExt;
@property(nonnull) void*                        callback;
@property(nonnull) void*                        userData;
@end

@implementation DocumentBrowserViewController
- (void)viewDidLoad
{
	[super viewDidLoad];

	self.delegate = self;
}

#pragma mark UIDocumentBrowserViewControllerDelegate
- (void)documentBrowser:(UIDocumentBrowserViewController*)controller
	didRequestDocumentCreationWithHandler:(void (^)(NSURL* _Nullable, UIDocumentBrowserImportMode))importHandler
{
	// set the URL for the new document
	// always call the importHandler, even if the user cancels the creation request
	__block NSURL* newDocumentURL = nil;

	@try
	{
		[Document
			createDocumentWithCompletionBlock:^(Document* doc) {
				if (doc != nil)
				{
					newDocumentURL = doc.fileURL;
					importHandler(newDocumentURL, UIDocumentBrowserImportModeMove);
				}
				else
				{
					// show error
					importHandler(newDocumentURL, UIDocumentBrowserImportModeNone);
				}
			}
									  fileExt:_defaultSaveFileExt];
	}
	@catch (NSException* exception)
	{
		// show error
		importHandler(newDocumentURL, UIDocumentBrowserImportModeNone);
	}
}

- (void)documentBrowser:(UIDocumentBrowserViewController*)controller didPickDocumentsAtURLs:(nonnull NSArray<NSURL*>*)documentURLs
{
	// callback after user picks a file
	NSURL* sourceURL = documentURLs.firstObject;
	if (!sourceURL)
		return;

	// present the Document View Controller for the first document that was picked
	[self presentDocumentAtURL:sourceURL title:@"RestoreOrImport"];
}

- (void)documentBrowser:(UIDocumentBrowserViewController*)controller
	didImportDocumentAtURL:(NSURL*)sourceURL
		  toDestinationURL:(NSURL*)destinationURL
{
	// callback after user clicks on (+) Create Document
	// present the Document View Controller for the new newly created document
	//	 [self presentDocumentAtURL:destinationURL title:@"BackupOrExport"];
}

- (void)documentBrowser:(UIDocumentBrowserViewController*)controller
	failedToImportDocumentAtURL:(NSURL*)documentURL
						  error:(NSError* _Nullable)error
{
	// handle the failed import
	[self showError];
}

// MARK: Document Presentation
- (void)doneTapped:(UIButton*)sender
{
	//print("returnToDocumentBrowser")
	UINavigationController* nc = (UINavigationController*)[self presentedViewController];
	if (nc)
		[nc dismissViewControllerAnimated:NO
							   completion:^{
								   // dismiss view
								   [self dismissViewControllerAnimated:NO completion:nil];
							   }];
}

- (void)presentDocumentAtURL:(NSURL*)documentURL title:(NSString*)title
{
	// access the document
	Document* doc = [[Document alloc] initWithFileURL:documentURL];
	[doc openWithCompletionHandler:^(BOOL success) {
		if (success)
		{
			self->_actionDocumentPicked(doc.fileURL, self->_callback, self->_userData);
			// dismiss view
			[self dismissViewControllerAnimated:NO completion:^{ [doc closeWithCompletionHandler:nil]; }];
		}
		else
		{
			// handle the failed access
			[self showError];
		}
	}];
}

- (void)showError
{
	UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Error"
																   message:@"open failed"
															preferredStyle:UIAlertControllerStyleAlert];
	UIAlertAction*     okButton = [UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil];
	[alert addAction:okButton];
	[self presentViewController:alert animated:YES completion:nil];
}
@end

static tinystl::unordered_map<tinystl::string, tinystl::string> gExtensionToUtiId;
extern UIViewController*                                        pMainViewController;
static UIBarButtonItem*                                         gDoneBtnItem;

@interface DocumentBrowserCancelDelegate: NSObject
{
}
- (void)cancelTapped:(UIButton*)sender;
@end

@implementation DocumentBrowserCancelDelegate
- (void)cancelTapped:(UIButton*)sender
{
	UINavigationController* nc = (UINavigationController*)[pMainViewController presentedViewController];
	if (nc)
		[nc dismissViewControllerAnimated:YES completion:nil];
}
@end

static DocumentBrowserCancelDelegate* gCancelDelegate;

void on_document_picked(NSURL* url, void* callback, void* userData)
{
	FileDialogCallbackFn fn = (FileDialogCallbackFn)callback;
	fn(url.fileSystemRepresentation, userData);
}

void add_uti_to_map(const char* extension, const char* uti) { gExtensionToUtiId.insert({ extension, uti }); }
/************************************************************************/
/************************************************************************/
static void format_file_extensions_filter(
	tinystl::string const& fileDesc, tinystl::vector<tinystl::string> const& extFiltersIn, tinystl::string& extFiltersOut)
{
	extFiltersOut = "";

	// Filtering for iOS requires the proper UTI identifier (https://developer.apple.com/library/archive/documentation/FileManagement/Conceptual/understanding_utis/understand_utis_intro/understand_utis_intro.html).
	// One can figure out which identifier to use by utilizing the mdls command (https://www.real-world-systems.com/docs/mdfind.1.html#mdls).

	for (size_t i = 0; i < extFiltersIn.size(); ++i)
	{
		tinystl::unordered_map<tinystl::string, tinystl::string>::iterator iter = gExtensionToUtiId.find(extFiltersIn[i]);
		if (iter != gExtensionToUtiId.end())
		{
			extFiltersOut += iter->second;
			if (i != extFiltersIn.size() - 1)
				extFiltersOut += ";";
		}
	}
}

static void create_document_browser(
	const char* title, void* callback, void* userData, NSArray* extList, NSString* defaultExt, bool allowDocumentCreation,
	bool allowMultipleSelection)
{
	// create a cancel button
	gCancelDelegate = [DocumentBrowserCancelDelegate alloc];
	gDoneBtnItem = [[UIBarButtonItem alloc] initWithTitle:@"Cancel"
													style:UIBarButtonItemStyleDone
												   target:gCancelDelegate
												   action:@selector(cancelTapped:)];

	DocumentBrowserViewController* dbvc = [[DocumentBrowserViewController alloc] initForOpeningFilesWithContentTypes:extList];
	dbvc.allowsDocumentCreation = allowDocumentCreation ? YES : NO;
	dbvc.allowsPickingMultipleItems = allowMultipleSelection ? YES : NO;
	dbvc.title = [NSString stringWithCString:title encoding:[NSString defaultCStringEncoding]];
	dbvc.defaultSaveFileExt = defaultExt;
	dbvc.actionDocumentPicked = on_document_picked;
	dbvc.callback = (void*)callback;
	dbvc.userData = (void*)userData;
	dbvc.navigationItem.leftBarButtonItem = gDoneBtnItem;

	UIViewController* dummyVC = [UIViewController alloc];
	id                nc = [[UINavigationController alloc] initWithRootViewController:dummyVC];
	// Hide main view
	[pMainViewController presentViewController:nc animated:NO completion:nil];
	// Push child view
	[dummyVC.navigationController pushViewController:dbvc animated:NO];
}

void open_file_dialog(
	const char* title, const char* dir, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
	const tinystl::vector<tinystl::string>& fileExtensions)
{
	tinystl::string extFilter;
	format_file_extensions_filter(fileDesc, fileExtensions, extFilter);
	// Create array of filtered extentions
	NSString* extString = [NSString stringWithCString:extFilter encoding:[NSString defaultCStringEncoding]];
	NSArray*  extList = [extString componentsSeparatedByString:@";"];
	create_document_browser(title, (void*)callback, (void*)userData, extList, nil, false, false);
}

void save_file_dialog(
	const char* title, const char* dir, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
	const tinystl::vector<tinystl::string>& fileExtensions)
{
	tinystl::string extFilter;
	format_file_extensions_filter(fileDesc, fileExtensions, extFilter);
	// Create array of filtered extentions
	NSString* extString = [NSString stringWithCString:extFilter encoding:[NSString defaultCStringEncoding]];
	NSArray*  extList = [extString componentsSeparatedByString:@";"];

	// Create default extension string
	NSString* defaultExt = nil;
	if (!fileExtensions.empty())
	{
		tinystl::string ext = fileExtensions[0];
		if (ext[0] != '.')
			ext.insert(0, ".", 1);
		defaultExt = [NSString stringWithCString:ext encoding:[NSString defaultCStringEncoding]];
	}
	create_document_browser(title, (void*)callback, (void*)userData, extList, defaultExt, true, false);
}
