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

#include "CocoaFileSystem.h"
#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../FileSystem/FileSystemInternal.h"
#include "../FileSystem/UnixFileSystem.h"

#include "../Interfaces/IMemory.h"

bool CocoaFileSystem::IsCaseSensitive() const {
#if TARGET_OS_IPHONE
    return true;
#else
    return false;
#endif
}

bool CocoaFileSystem::CopyFile(const Path* sourcePath, const Path* destinationPath, bool overwriteIfExists) const {
    if (this->FileExists(destinationPath)) {
        if (!overwriteIfExists) {
            return false;
        } else {
            this->DeleteFile(destinationPath);
        }
    }
    
    NSError *error = nil;
    NSString *sourcePathString = [NSString stringWithUTF8String:fsGetPathAsNativeString(sourcePath)];
    NSString *destinationPathString = [NSString stringWithUTF8String:fsGetPathAsNativeString(destinationPath)];
    
    bool success = [[NSFileManager defaultManager] copyItemAtPath:sourcePathString toPath:destinationPathString error:&error];
    
    if (!success) {
        LOGF(LogLevel::eWARNING, "Failed to copy file from %s to %s: %s", fsGetPathAsNativeString(sourcePath), fsGetPathAsNativeString(destinationPath), [[error description] UTF8String]);
    }
    return success;
}

void CocoaFileSystem::EnumerateFilesWithExtension(const Path* directory, const char* extension, bool (*processFile)(const Path*, void* userData), void* userData) const {
    NSURL *pathURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:fsGetPathAsNativeString(directory)]];
    NSDirectoryEnumerator<NSURL*>* enumerator = [[NSFileManager defaultManager] enumeratorAtURL:pathURL includingPropertiesForKeys:@[NSURLPathKey] options:0 errorHandler:^BOOL(NSURL * _Nonnull url, NSError * _Nonnull error) {
        LOGF(LogLevel::eWARNING, "Error enumerating directory at url %s: %s", [[url path] UTF8String], [[error description] UTF8String]);
        return YES;
    }];
    
    NSString *extensionString = nil;
    if (extension) {
        extensionString = [[NSString stringWithUTF8String:extension] lowercaseString];
    }
    
    for (NSURL* url in enumerator) {
        if (extensionString && ![[url.pathExtension lowercaseString] isEqualToString:extensionString]) {
            continue;
        }
        
        Path *path = fsCreatePath(this, [url.path UTF8String]);
        bool shouldContinue = processFile(path, userData);
        fsFreePath(path);
        
        if (!shouldContinue) {
            break;
        } 
    }
}

void CocoaFileSystem::EnumerateSubDirectories(const Path* directory, bool (*processDirectory)(const Path*, void* userData), void* userData) const {
    NSURL *pathURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:fsGetPathAsNativeString(directory)]];
    NSDirectoryEnumerator<NSURL*>* enumerator = [[NSFileManager defaultManager] enumeratorAtURL:pathURL includingPropertiesForKeys:@[NSURLPathKey, NSURLIsDirectoryKey] options:0 errorHandler:^BOOL(NSURL * _Nonnull url, NSError * _Nonnull error) {
        LOGF(LogLevel::eWARNING, "Error enumerating directory at url %s: %s", [[url path] UTF8String], [[error description] UTF8String]);
        return YES;
    }];
    
    for (NSURL* url in enumerator) {
        NSNumber *isDirectory = nil;
        [url getResourceValue:&isDirectory forKey:NSURLIsDirectoryKey error:nil];
        
        if (![isDirectory boolValue]) {
            continue;
        }
        
        Path *path = fsCreatePath(this, [url.path UTF8String]);
        bool shouldContinue = processDirectory(path, userData);
        fsFreePath(path);
        
        if (!shouldContinue) {
            break;
        }
    }
}

CocoaFileSystem gDefaultFS;

FileSystem* fsGetSystemFileSystem() {
    return &gDefaultFS;
}

Path* fsCopyWorkingDirectoryPath() {
    NSString *directoryPath = [NSFileManager defaultManager].currentDirectoryPath;
    const char *utf8String = directoryPath.UTF8String;
    return fsCreatePath(fsGetSystemFileSystem(), utf8String);
}

Path* fsCopyProgramDirectoryPath() {
#if TARGET_OS_IPHONE
    NSString *directoryPath = [NSBundle mainBundle].bundlePath;
#else
    NSString *directoryPath = [[NSBundle mainBundle].bundleURL URLByDeletingLastPathComponent].path;
#endif
    // Go one directory up from the bundleURL
    return fsCreatePath(fsGetSystemFileSystem(), [directoryPath UTF8String]);
}

Path* fsCopyExecutablePath() {
    NSString *path = [[NSBundle mainBundle] executablePath];
    const char *utf8String = [path UTF8String];
    return fsCreatePath(fsGetSystemFileSystem(), utf8String);
}

Path* fsCopyPreferencesDirectoryPath(const char* organisation, const char* application) {
    NSFileManager *fileManager = [NSFileManager defaultManager];
    
    NSError *error = nil;
    NSURL *url = [fileManager URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:true error:&error];
    
    if (error) {
        LOGF(LogLevel::eERROR, "Error retrieving user preferences directory: %s", [[error description] UTF8String]);
        return NULL;
    }
    
    NSURL *fullURL = [url URLByAppendingPathComponent:[NSString stringWithFormat:@"%s/%s", organisation, application]];
    
    if (![fileManager fileExistsAtPath:fullURL.path]) {
        [fileManager createDirectoryAtURL:fullURL withIntermediateDirectories:true attributes:nil error:&error];
        
        if (error) {
            LOGF(LogLevel::eERROR, "Error retrieving user preferences directory: %s", [[error description] UTF8String]);
            return NULL;
        }
    }
    
    return fsCreatePath(fsGetSystemFileSystem(), [[fullURL path] UTF8String]);
}

Path* fsCopyUserDocumentsDirectoryPath() {
    NSFileManager *fileManager = [NSFileManager defaultManager];
    
    NSError *error = nil;
    NSURL *url = [fileManager URLForDirectory:NSDocumentDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:true error:&error];
    if (error) {
        LOGF(LogLevel::eERROR, "Error retrieving user documents directory: %s", [[error description] UTF8String]);
        return NULL;
    }
    
    return fsCreatePath(fsGetSystemFileSystem(), [[url path] UTF8String]);
}


Path* fsCopyLogFileDirectoryPath() {
#if TARGET_OS_IPHONE
    // Place log files in the application support directory on iOS.
    NSFileManager *fileManager = [NSFileManager defaultManager];
    
    NSError *error = nil;
    NSURL *url = [fileManager URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:true error:&error];
    
    return fsCreatePath(fsGetSystemFileSystem(), url.path.UTF8String);
#else
    return fsCopyProgramDirectoryPath();
#endif
}
