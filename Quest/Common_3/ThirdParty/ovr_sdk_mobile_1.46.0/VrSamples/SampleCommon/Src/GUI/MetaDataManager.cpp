/************************************************************************************

Filename    :   MetaDataManager.cpp
Content     :   A class to manage metadata used by FolderBrowser
Created     :   January 26, 2015
Authors     :   Jonathan E. Wright, Warsam Osman, Madhu Kalva

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#include "MetaDataManager.h"
#include "PackageFiles.h"
#include "OVR_FileSys.h"
#include "OVR_JSON.h"
#include "Misc/Log.h"

#include <algorithm>
#include <locale>

#include <dirent.h>
#include <unistd.h>

using OVR::JSON;
using OVR::JsonReader;

namespace OVRFW {

void SortStringArray(std::vector<std::string>& strings) {
    std::sort(strings.begin(), strings.end());
}

// if pathToAppend is an empty string, this just adds a slash
void AppendPath(std::string& startPath, const char* pathToAppend) {
    int const len = startPath.length();
    if (len == 0) {
        startPath = pathToAppend;
        return;
    }
    uint32_t lastCh = startPath[len - 1];
    if (lastCh != '/' && lastCh != '\\') {
        // always append the linux path, assuming it will be corrected elsewhere if necessary for
        // Windows
        startPath += '/';
    }
    startPath += pathToAppend;
}

// DirPath should by a directory with a trailing slash.
// Returns all files in all search paths, as unique relative paths.
// Subdirectories will have a trailing slash.
// All files and directories that start with . are skipped.
std::unordered_map<std::string, std::string> RelativeDirectoryFileList(
    const std::vector<std::string>& searchPaths,
    const char* RelativeDirPath) {
    // Check each of the mirrors in searchPaths and build up a list of unique strings
    std::unordered_map<std::string, std::string> uniqueStrings;
    std::string relativeDirPathString = std::string(RelativeDirPath);

#if defined(OVR_BUILD_DEBUG)
    ALOG(
        "RelativeDirectoryFileList searchPaths=%d relative='%s'",
        (int)searchPaths.size(),
        relativeDirPathString.c_str());
#endif

    const int numSearchPaths = static_cast<const int>(searchPaths.size());
    for (int index = 0; index < numSearchPaths; ++index) {
        const std::string fullPath = searchPaths[index] + relativeDirPathString;
        DIR* dir = opendir(fullPath.c_str());
        if (dir != NULL) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.') {
                    continue;
                }
                if (entry->d_type == DT_DIR) {
                    std::string s = relativeDirPathString;
                    s += entry->d_name;
                    s += "/";
#if defined(OVR_BUILD_DEBUG)
                    ALOG("RelativeDirectoryFileList adding - %s", s.c_str());
#endif

                    std::string lowerCaseS = s.c_str();
                    std::transform(
                        lowerCaseS.begin(), lowerCaseS.end(), lowerCaseS.begin(), ::tolower);
                    uniqueStrings[lowerCaseS] = s;
                } else if (entry->d_type == DT_REG) {
                    std::string s = relativeDirPathString;
                    s += entry->d_name;
#if defined(OVR_BUILD_DEBUG)
                    ALOG("RelativeDirectoryFileList adding - %s", s.c_str());
#endif

                    std::string lowerCaseS = s.c_str();
                    std::transform(
                        lowerCaseS.begin(), lowerCaseS.end(), lowerCaseS.begin(), ::tolower);
                    uniqueStrings[lowerCaseS] = s;
                }
            }
            closedir(dir);
        }
    }

    return uniqueStrings;
}

std::string ExtractFileBase(const std::string& s) {
    const int l = static_cast<int>(s.length());
    if (l == 0) {
        return std::string("");
    }

    int end;
    if (s[l - 1] == '/') { // directory ends in a slash
        end = l - 1;
    } else {
        for (end = l - 1; end > 0 && s[end] != '.'; end--)
            ;
        if (end == 0) {
            end = l;
        }
    }
    int start;
    for (start = end - 1; start > -1 && s[start] != '/'; start--)
        ;
    start++;

    return std::string(&s[start], end - start);
}

bool MatchesExtension(const char* fileName, const char* ext) {
    const int extLen = static_cast<int>(OVR::OVR_strlen(ext));
    const int sLen = static_cast<int>(OVR::OVR_strlen(fileName));
    if (sLen < extLen + 1) {
        return false;
    }
    return (0 == strcmp(&fileName[sLen - extLen], ext));
}

//==============================
// OvrMetaData

const char* const VERSION = "Version";
const char* const CATEGORIES = "Categories";
const char* const DATA = "Data";
const char* const FAVORITES_TAG = "Favorites";
const char* const TAG = "tag";
const char* const LABEL = "label";
const char* const TAGS = "tags";
const char* const CATEGORY = "category";
const char* const URL_INNER = "url";

void OvrMetaData::InitFromDirectory(
    const char* relativePath,
    const std::vector<std::string>& searchPaths,
    const OvrMetaDataFileExtensions& fileExtensions) {
    ALOG("OvrMetaData::InitFromDirectory( %s )", relativePath);

    // Find all the files - checks all search paths
    std::unordered_map<std::string, std::string> uniqueFileList =
        RelativeDirectoryFileList(searchPaths, relativePath);
    std::vector<std::string> fileList;
    for (auto iter = uniqueFileList.begin(); iter != uniqueFileList.end(); ++iter) {
#if defined(OVR_BUILD_DEBUG)
        /// We map the lowercase variant (for searching) as a key to the real MixedCase actual file
        /// name make sure we use the real MixedCase name hence forth ( aka ->second )
        ALOG("OvrMetaData  fileList[ '%s' ] = '%s'", iter->first.c_str(), iter->second.c_str());
#endif
        fileList.push_back(iter->second);
    }
    SortStringArray(fileList);
    Category currentCategory;
    currentCategory.CategoryTag = ExtractFileBase(relativePath);
    // The label is the same as the tag by default.
    // Will be replaced if definition found in loaded metadata
    currentCategory.LocaleKey = currentCategory.CategoryTag;

    ALOG("OvrMetaData start category: %s", currentCategory.CategoryTag.c_str());
    std::vector<std::string> subDirs;
    // Grab the categories and loose files
    for (const std::string& s : fileList) {
        ALOG("OvrMetaData category: %s file: %s", currentCategory.CategoryTag.c_str(), s.c_str());

        const std::string fileBase = ExtractFileBase(s);
        // subdirectory - add category
        if (MatchesExtension(s.c_str(), "/")) {
            subDirs.push_back(s);
            continue;
        }

        // See if we want this loose-file
        if (!ShouldAddFile(s.c_str(), fileExtensions)) {
            continue;
        }

        // Add loose file
        const int dataIndex = static_cast<int>(MetaData.size());
        OvrMetaDatum* datum = CreateMetaDatum(fileBase.c_str());
        if (datum) {
            datum->Id = dataIndex;
            datum->Tags.push_back(currentCategory.CategoryTag);
            if (GetFullPath(searchPaths, s.c_str(), datum->Url)) {
                // always use the lowercase version of the URL to search the map
                std::string lowerCaseUrl = datum->Url.c_str();
                auto& loc = std::use_facet<std::ctype<char>>(std::locale());
                loc.tolower(&lowerCaseUrl[0], &lowerCaseUrl[0] + lowerCaseUrl.length());

                auto datumIter = UrlToIndex.find(lowerCaseUrl);
                if (datumIter == UrlToIndex.end()) {
                    // always use the lowercase version of the URL as map key
                    UrlToIndex[lowerCaseUrl] = dataIndex;
                    MetaData.push_back(datum);
                    ALOG(
                        "OvrMetaData adding datum %s with index %d to %s",
                        datum->Url.c_str(),
                        dataIndex,
                        currentCategory.CategoryTag.c_str());
                    // Register with category
                    currentCategory.DatumIndicies.push_back(dataIndex);
                } else {
                    ALOGW(
                        "OvrMetaData::InitFromDirectory found duplicate url %s",
                        datum->Url.c_str());
                }
            } else {
                ALOGW("OvrMetaData::InitFromDirectory failed to find %s", s.c_str());
            }
        }
    }

    if (!currentCategory.DatumIndicies.empty()) {
        Categories.push_back(currentCategory);
    }

    // Recurse into subdirs
    for (const std::string& subDir : subDirs) {
        InitFromDirectory(subDir.c_str(), searchPaths, fileExtensions);
    }
}

void OvrMetaData::InitFromFileList(
    const std::vector<std::string>& fileList,
    const OvrMetaDataFileExtensions& fileExtensions) {
    // Create unique categories
    std::unordered_map<std::string, int> uniqueCategoryList;
    for (int i = 0; i < static_cast<int>(fileList.size()); ++i) {
        const std::string& filePath = fileList[i];
        const std::string categoryTag = ExtractDirectory(fileList[i]);
        auto iter = uniqueCategoryList.find(categoryTag);
        int catIndex = -1;
        if (iter == uniqueCategoryList.end()) {
            ALOG(" category: %s", categoryTag.c_str());
            Category cat;
            cat.CategoryTag = categoryTag;
            // The label is the same as the tag by default.
            // Will be replaced if definition found in loaded metadata
            cat.LocaleKey = cat.CategoryTag;
            catIndex = static_cast<int>(Categories.size());
            Categories.push_back(cat);
            uniqueCategoryList[categoryTag] = catIndex;
        } else {
            catIndex = iter->second;
        }

        assert(catIndex > -1);
        Category& currentCategory = Categories.at(catIndex);

        // See if we want this loose-file
        if (!ShouldAddFile(filePath.c_str(), fileExtensions)) {
            continue;
        }

        // Add loose file
        const int dataIndex = static_cast<int>(MetaData.size());
        OvrMetaDatum* datum = CreateMetaDatum(filePath.c_str());
        if (datum != NULL) {
            datum->Id = dataIndex;
            datum->Url = filePath;
            datum->Tags.push_back(currentCategory.CategoryTag);

            // always use the lowercase version of the URL
            std::string lowerCaseUrl = datum->Url.c_str();
            auto& loc = std::use_facet<std::ctype<char>>(std::locale());
            loc.tolower(&lowerCaseUrl[0], &lowerCaseUrl[0] + lowerCaseUrl.length());
            auto datumIter = UrlToIndex.find(lowerCaseUrl);
            if (datumIter == UrlToIndex.end()) {
                // keep the lowercase version in the map for insensitive find
                UrlToIndex[lowerCaseUrl] = dataIndex;
                MetaData.push_back(datum);
                ALOG(
                    "OvrMetaData::InitFromFileList adding datum %s with index %d to %s",
                    datum->Url.c_str(),
                    dataIndex,
                    currentCategory.CategoryTag.c_str());
                // Register with category
                currentCategory.DatumIndicies.push_back(dataIndex);
            } else {
                ALOGW("OvrMetaData::InitFromFileList found duplicate url %s", datum->Url.c_str());
            }
        }
    }
}

void OvrMetaData::RenameCategory(const char* currentTag, const char* newName) {
    for (Category& cat : Categories) {
        if (cat.CategoryTag == currentTag) {
            cat.LocaleKey = newName;
            break;
        }
    }
}

// Set a category label after construction
bool OvrMetaData::RenameCategoryTag(const char* currentTag, const char* newName) {
    Category* category = GetCategory(currentTag);
    if (!category) {
        return false;
    }
    for (OvrMetaDatum* datum : MetaData) {
        std::vector<std::string>& tags = datum->Tags;
        for (int t = 0; t < static_cast<int>(tags.size()); ++t) {
            if (tags.at(t) == currentTag) {
                tags.at(t) = newName;
                break;
            }
        }
    }
    category->CategoryTag = newName;
    return true;
}

std::shared_ptr<JSON> LoadPackageMetaFile(const char* metaFile) {
    int bufferLength = 0;
    void* buffer = NULL;
    std::string assetsMetaFile = "assets/";
    assetsMetaFile += metaFile;
    ovr_ReadFileFromApplicationPackage(assetsMetaFile.c_str(), bufferLength, buffer);
    if (!buffer) {
        ALOGW("LoadPackageMetaFile failed to read %s", assetsMetaFile.c_str());
    }
    return JSON::Parse(static_cast<const char*>(buffer));
}

std::shared_ptr<JSON> OvrMetaData::CreateOrGetStoredMetaFile(
    const char* appFileStoragePath,
    const char* metaFile) {
    FilePath = appFileStoragePath;
    FilePath += metaFile;

    ALOG("CreateOrGetStoredMetaFile FilePath: %s", FilePath.c_str());

    std::shared_ptr<JSON> dataFile = JSON::Load(FilePath.c_str());
    if (dataFile == NULL) {
        // If this is the first run, or we had an error loading the file, we copy the meta file from
        // assets to app's cache
        WriteMetaFile(metaFile);

        // try loading it again
        dataFile = JSON::Load(FilePath.c_str());
        if (dataFile == NULL) {
            ALOGW("OvrMetaData failed to load JSON meta file: %s", metaFile);
        }
    } else {
        ALOG("OvrMetaData::CreateOrGetStoredMetaFile found %s", FilePath.c_str());
    }
    return dataFile;
}

void OvrMetaData::WriteMetaFile(const char* metaFile) const {
    ALOG("Writing metafile from apk");

    if (FILE* newMetaFile = fopen(FilePath.c_str(), "w")) {
        int bufferLength = 0;
        void* buffer = NULL;
        std::string assetsMetaFile = "assets/";
        assetsMetaFile += metaFile;
        ovr_ReadFileFromApplicationPackage(assetsMetaFile.c_str(), bufferLength, buffer);
        if (!buffer) {
            ALOGW("OvrMetaData failed to read %s", assetsMetaFile.c_str());
        } else {
            size_t writtenCount = fwrite(buffer, 1, bufferLength, newMetaFile);
            if (writtenCount != static_cast<size_t>(bufferLength)) {
                OVR_FAIL("OvrMetaData::WriteMetaFile failed to write %s", metaFile);
            }
            free(buffer);
        }
        fclose(newMetaFile);
    } else {
        OVR_FAIL("OvrMetaData failed to create %s - check app permissions", FilePath.c_str());
    }
}

void OvrMetaData::InitFromDirectoryMergeMeta(
    const char* relativePath,
    const std::vector<std::string>& searchPaths,
    const OvrMetaDataFileExtensions& fileExtensions,
    const char* metaFile,
    const char* packageName) {
    ALOG("OvrMetaData::InitFromDirectoryMergeMeta");

    std::string appFileStoragePath = "/data/data/";
    appFileStoragePath += packageName;
    appFileStoragePath += "/files/";

    FilePath = appFileStoragePath + metaFile;

    assert(HasPermission(FilePath.c_str(), permissionFlags_t(PERMISSION_READ)));

    std::shared_ptr<JSON> dataFile =
        CreateOrGetStoredMetaFile(appFileStoragePath.c_str(), metaFile);

    InitFromDirectory(relativePath, searchPaths, fileExtensions);
    ProcessMetaData(dataFile, searchPaths, metaFile);
}

void OvrMetaData::InitFromFileListMergeMeta(
    const std::vector<std::string>& fileList,
    const std::vector<std::string>& searchPaths,
    const OvrMetaDataFileExtensions& fileExtensions,
    const char* appFileStoragePath,
    const char* metaFile,
    std::shared_ptr<JSON> storedMetaData) {
    ALOG("OvrMetaData::InitFromFileListMergeMeta");

    InitFromFileList(fileList, fileExtensions);
    ProcessMetaData(storedMetaData, searchPaths, metaFile);
}

void OvrMetaData::InsertCategoryList(
    const int startIndex,
    const std::vector<Category>& categoryList) {
    // Merge in the remote categories
    // if any duplicates exist, their order is based on the new list
    std::vector<Category> finalCategoryList;

    std::unordered_map<std::string, bool> newCategorySet;
    for (const Category& newCategory : categoryList) {
        newCategorySet[newCategory.CategoryTag] = true;
    }

    // Remove any duplicates in the existing categories
    for (const Category& existingCategory : Categories) {
        auto iter = newCategorySet.find(existingCategory.CategoryTag);
        if (iter == newCategorySet.end()) {
            finalCategoryList.push_back(existingCategory);
        }
    }

    // Insert the new category list starting at the startIndex if possible - otherwise just append
    for (int remoteIndex = 0; remoteIndex < static_cast<int>(categoryList.size()); ++remoteIndex) {
        const Category& newCategory = categoryList[remoteIndex];

        const int targetIndex = startIndex + remoteIndex;
        ALOG(
            "OvrMetaData::InsertCategoryList merging %s into category index %d",
            newCategory.CategoryTag.c_str(),
            targetIndex);
        if (startIndex >= 0 && startIndex < static_cast<int>(Categories.size())) {
            finalCategoryList.insert(finalCategoryList.cbegin() + targetIndex, newCategory);
        } else {
            finalCategoryList.push_back(newCategory);
        }
    }

    std::swap(Categories, finalCategoryList);
}

void OvrMetaData::ProcessRemoteMetaFile(const char* metaFileString, const int startIndex) {
    char const* errorMsg = NULL;
    std::shared_ptr<JSON> remoteMetaFile = JSON::Parse(metaFileString, &errorMsg);
    if (remoteMetaFile != NULL) {
        // First grab the version
        double remoteVersion = 0.0;
        ExtractVersion(remoteMetaFile, remoteVersion);

        if (remoteVersion <=
            Version) // We already have this metadata, don't need to process further
        {
            return;
        }

        Version = remoteVersion;

        std::vector<Category> remoteCategories;
        std::unordered_map<std::string, OvrMetaDatum*> remoteMetaData;
        ExtractCategories(remoteMetaFile, remoteCategories);
        ExtractRemoteMetaData(remoteMetaFile, remoteMetaData);

        InsertCategoryList(startIndex, remoteCategories);

        // Append the remote data
        ReconcileMetaData(remoteMetaData);

        // Recreate indices which may have changed after reconciliation
        RegenerateCategoryIndices();

        // Serialize the new metadata
        std::shared_ptr<JSON> dataFile = MetaDataToJson();
        if (dataFile == NULL) {
            OVR_FAIL("OvrMetaData::ProcessMetaData failed to generate JSON meta file");
        }

        dataFile->Save(FilePath.c_str());

        ALOG("OvrMetaData::ProcessRemoteMetaFile updated %s", FilePath.c_str());
    } else {
        ALOG("Meta file parse error '%s'", errorMsg != NULL ? "<NULL>" : errorMsg);
    }
}

void OvrMetaData::ProcessMetaData(
    std::shared_ptr<JSON> dataFile,
    const std::vector<std::string>& searchPaths,
    const char* metaFile) {
    if (dataFile != NULL) {
        // Grab the version from the loaded data
        ExtractVersion(dataFile, Version);

        std::vector<Category> storedCategories;
        std::unordered_map<std::string, OvrMetaDatum*> storedMetaData;
        ExtractCategories(dataFile, storedCategories);

        // Read in package data first
        std::shared_ptr<JSON> packageMeta = LoadPackageMetaFile(metaFile);
        if (packageMeta) {
            // If we failed to find a version in the serialized data, need to set it from the assets
            // version
            if (Version < 0.0) {
                ExtractVersion(packageMeta, Version);
                if (Version < 0.0) {
                    Version = 0.0;
                }
            }
            ExtractCategories(packageMeta, storedCategories);
            ExtractMetaData(packageMeta, searchPaths, storedMetaData);
        } else {
            ALOGW("ProcessMetaData LoadPackageMetaFile failed for %s", metaFile);
        }

        // Read in the stored data - overriding any found in the package
        ExtractMetaData(dataFile, searchPaths, storedMetaData);

        // Reconcile the stored data vs the data read in
        ReconcileCategories(storedCategories);
        ReconcileMetaData(storedMetaData);

        // Recreate indices which may have changed after reconciliation
        RegenerateCategoryIndices();

        // Delete any newly empty categories except Favorites
        if (!Categories.empty()) {
            std::vector<Category> finalCategories;
            finalCategories.push_back(Categories.at(0));
            for (Category& cat : Categories) {
                if (!cat.DatumIndicies.empty()) {
                    finalCategories.push_back(cat);
                } else {
                    ALOGW(
                        "OvrMetaData::ProcessMetaData discarding empty %s",
                        cat.CategoryTag.c_str());
                }
            }
            std::swap(finalCategories, Categories);
        }
    } else {
        ALOGW("OvrMetaData::ProcessMetaData NULL dataFile");
    }

    // Rewrite new data
    dataFile = MetaDataToJson();
    if (dataFile == NULL) {
        OVR_FAIL("OvrMetaData::ProcessMetaData failed to generate JSON meta file");
    }

    dataFile->Save(FilePath.c_str());

    ALOG("OvrMetaData::ProcessMetaData created %s", FilePath.c_str());
}

void OvrMetaData::ReconcileMetaData(
    std::unordered_map<std::string, OvrMetaDatum*>& storedMetaData) {
    if (storedMetaData.empty()) {
        return;
    }
    DedupMetaData(MetaData, storedMetaData);

    // Now for any remaining stored data - check if it's remote and just add it, sorted by the
    // assigned Id
    std::vector<OvrMetaDatum*> sortedEntries;
    auto storedIter = storedMetaData.begin();
    for (; storedIter != storedMetaData.end(); ++storedIter) {
        OvrMetaDatum* storedDatum = storedIter->second;
        if (IsRemote(storedDatum)) {
            ALOG("ReconcileMetaData metadata adding remote %s", storedDatum->Url.c_str());
            sortedEntries.push_back(storedDatum);
        }
    }
    std::sort(
        sortedEntries.begin(),
        sortedEntries.end(),
        [=](const OvrMetaDatum* a, const OvrMetaDatum* b) { return a->Id < b->Id; });
    for (const auto& entry : sortedEntries) {
        MetaData.push_back(entry);
    }
    storedMetaData.clear();
}

void OvrMetaData::DedupMetaData(
    std::vector<OvrMetaDatum*>& existingData,
    std::unordered_map<std::string, OvrMetaDatum*>& newData) {
    // Fix the read in meta data using the stored
    for (int i = 0; i < static_cast<int>(existingData.size()); ++i) {
        OvrMetaDatum* metaDatum = existingData[i];

        // always use the lowercase version of the URL
        std::string lowerCaseUrl = metaDatum->Url.c_str();
        auto& loc = std::use_facet<std::ctype<char>>(std::locale());
        loc.tolower(&lowerCaseUrl[0], &lowerCaseUrl[0] + lowerCaseUrl.length());
        auto iter = newData.find(lowerCaseUrl);
        if (iter != newData.end()) {
            OvrMetaDatum* storedDatum = iter->second;
            ALOG("DedupMetaData metadata for %s", storedDatum->Url.c_str());
            std::swap(storedDatum->Tags, metaDatum->Tags);
            SwapExtendedData(storedDatum, metaDatum);
            newData.erase(iter);
        }
    }
}

void OvrMetaData::ReconcileCategories(std::vector<Category>& storedCategories) {
    if (storedCategories.empty()) {
        return;
    }

    // Reconcile categories
    // We want Favorites always at the top
    // Followed by user created categories
    // Finally we want to maintain the order of the retail categories (defined in assets/meta.json)
    std::vector<Category> finalCategories;

    Category favorites = storedCategories.at(0);
    if (favorites.CategoryTag != FAVORITES_TAG) {
        ALOGW(
            "OvrMetaData::ReconcileCategories failed to find expected category order -- missing assets/meta.json?");
    }

    finalCategories.push_back(favorites);

    std::unordered_map<std::string, bool> StoredCategoryMap; // using as set
    for (const Category& storedCategory : storedCategories) {
        ALOG(
            "OvrMetaData::ReconcileCategories storedCategory: %s",
            storedCategory.CategoryTag.c_str());
        StoredCategoryMap[storedCategory.CategoryTag] = true;
    }

    // Now add the read in categories if they differ
    for (const Category& readInCategory : Categories) {
        auto iter = StoredCategoryMap.find(readInCategory.CategoryTag);

        if (iter == StoredCategoryMap.end()) {
            ALOG("OvrMetaData::ReconcileCategories adding %s", readInCategory.CategoryTag.c_str());
            finalCategories.push_back(readInCategory);
        }
    }

    // Finally fill in the stored in categories after user made ones
    for (const Category& storedCat : storedCategories) {
        ALOG(
            "OvrMetaData::ReconcileCategories adding stored category %s",
            storedCat.CategoryTag.c_str());
        finalCategories.push_back(storedCat);
    }

    // Now replace Categories
    std::swap(Categories, finalCategories);
}

void OvrMetaData::ExtractVersion(std::shared_ptr<JSON> dataFile, double& outVersion) const {
    if (dataFile == NULL) {
        return;
    }

    const JsonReader dataReader(dataFile);
    if (dataReader.IsObject()) {
        outVersion = dataReader.GetChildDoubleByName(VERSION);
    }
}

void OvrMetaData::ExtractCategories(
    std::shared_ptr<JSON> dataFile,
    std::vector<Category>& outCategories) const {
    if (dataFile == NULL) {
        return;
    }

    const JsonReader categories(dataFile->GetItemByName(CATEGORIES));

    if (categories.IsArray()) {
        while (const std::shared_ptr<JSON> nextElement = categories.GetNextArrayElement()) {
            const JsonReader category(nextElement);
            if (category.IsObject()) {
                Category extractedCategory;
                extractedCategory.CategoryTag = category.GetChildStringByName(TAG);
                extractedCategory.LocaleKey = category.GetChildStringByName(LABEL);

                // Check if we already have this category
                bool exists = false;
                for (const Category& existingCat : outCategories) {
                    if (extractedCategory.CategoryTag == existingCat.CategoryTag) {
                        exists = true;
                        break;
                    }
                }

                if (!exists) {
                    ALOG("Extracting category: %s", extractedCategory.CategoryTag.c_str());
                    outCategories.push_back(extractedCategory);
                }
            }
        }
    }
}

void OvrMetaData::ExtractMetaData(
    std::shared_ptr<JSON> dataFile,
    const std::vector<std::string>& searchPaths,
    std::unordered_map<std::string, OvrMetaDatum*>& outMetaData) const {
    if (dataFile == NULL) {
        return;
    }

    const JsonReader data(dataFile->GetItemByName(DATA));
    if (data.IsArray()) {
        int jsonIndex = static_cast<int>(MetaData.size());
        while (const std::shared_ptr<JSON> nextElement = data.GetNextArrayElement()) {
            const JsonReader datum(nextElement);
            if (datum.IsObject()) {
                OvrMetaDatum* metaDatum = CreateMetaDatum("");
                if (!metaDatum) {
                    continue;
                }

                metaDatum->Id = jsonIndex++;
                const JsonReader tags(datum.GetChildByName(TAGS));
                if (tags.IsArray()) {
                    while (const std::shared_ptr<JSON> tagElement = tags.GetNextArrayElement()) {
                        const JsonReader tag(tagElement);
                        if (tag.IsObject()) {
                            metaDatum->Tags.push_back(tag.GetChildStringByName(CATEGORY));
                        }
                    }
                }

                assert(!metaDatum->Tags.empty());

                const std::string relativeUrl(datum.GetChildStringByName(URL_INNER));
                metaDatum->Url = relativeUrl;
                bool foundPath = false;
                const bool isRemote = IsRemote(metaDatum);

                // Get the absolute path if this is a local file
                if (!isRemote) {
                    foundPath = GetFullPath(searchPaths, relativeUrl.c_str(), metaDatum->Url);
                    if (!foundPath) {
                        // if we fail to find the file, check for encrypted extension (TODO: Might
                        // put this into a virtual function if necessary, benign for now)
                        foundPath = GetFullPath(
                            searchPaths, std::string(relativeUrl + ".x").c_str(), metaDatum->Url);
                    }
                }

                // if we fail to find the local file or it's a remote file, the Url is left as read
                // in from the stored data
                if (isRemote || !foundPath) {
                    metaDatum->Url = relativeUrl;
                }

                ExtractExtendedData(datum, *metaDatum);
                ALOG("OvrMetaData::ExtractMetaData adding datum %s", metaDatum->Url.c_str());

                // always use the lowercase version of the URL
                std::string lowerCaseUrl = metaDatum->Url.c_str();
                auto& loc = std::use_facet<std::ctype<char>>(std::locale());
                loc.tolower(&lowerCaseUrl[0], &lowerCaseUrl[0] + lowerCaseUrl.length());
                auto iter = outMetaData.find(lowerCaseUrl);
                if (iter == outMetaData.end()) {
                    // Always index by lowercase version of the URL
                    outMetaData[lowerCaseUrl] = metaDatum;
                } else {
                    iter->second = metaDatum;
                }
            }
        }
    }
}

void OvrMetaData::ExtractRemoteMetaData(
    std::shared_ptr<JSON> dataFile,
    std::unordered_map<std::string, OvrMetaDatum*>& outMetaData) const {
    if (dataFile == NULL) {
        return;
    }

    const JsonReader data(dataFile->GetItemByName(DATA));
    if (data.IsArray()) {
        int jsonIndex = static_cast<int>(MetaData.size());
        while (const std::shared_ptr<JSON> nextElement = data.GetNextArrayElement()) {
            const JsonReader jsonDatum(nextElement);
            if (jsonDatum.IsObject()) {
                OvrMetaDatum* metaDatum = CreateMetaDatum("");
                if (!metaDatum) {
                    continue;
                }
                metaDatum->Id = jsonIndex++;
                const JsonReader tags(jsonDatum.GetChildByName(TAGS));
                if (tags.IsArray()) {
                    while (const std::shared_ptr<JSON> tagElement = tags.GetNextArrayElement()) {
                        const JsonReader tag(tagElement);
                        if (tag.IsObject()) {
                            metaDatum->Tags.push_back(tag.GetChildStringByName(CATEGORY));
                        }
                    }
                }

                assert(!metaDatum->Tags.empty());

                metaDatum->Url = jsonDatum.GetChildStringByName(URL_INNER);
                ExtractExtendedData(jsonDatum, *metaDatum);

                // always use the lowercase version of the URL
                std::string lowerCaseUrl = metaDatum->Url.c_str();
                auto& loc = std::use_facet<std::ctype<char>>(std::locale());
                loc.tolower(&lowerCaseUrl[0], &lowerCaseUrl[0] + lowerCaseUrl.length());
                auto iter = outMetaData.find(lowerCaseUrl);
                if (iter == outMetaData.end()) {
                    outMetaData[lowerCaseUrl] = metaDatum;
                } else {
                    iter->second = metaDatum;
                }
            }
        }
    }
}

void OvrMetaData::Serialize() {
    // Serialize the new metadata
    std::shared_ptr<JSON> dataFile = MetaDataToJson();
    if (dataFile == NULL) {
        OVR_FAIL("OvrMetaData::Serialize failed to generate JSON meta file");
    }

    dataFile->Save(FilePath.c_str());

    ALOG("OvrMetaData::Serialize updated %s", FilePath.c_str());
}

void OvrMetaData::RegenerateCategoryIndices() {
    for (Category& cat : Categories) {
        cat.DatumIndicies.clear();
    }

    // Delete any data only tagged as "Favorite" - this is a fix for user created "Favorite" folder
    // which is a special case Not doing this will show photos already favorited that the user
    // cannot unfavorite
    for (int metaDataIndex = 0; metaDataIndex < static_cast<int>(MetaData.size());
         ++metaDataIndex) {
        OvrMetaDatum& metaDatum = *MetaData.at(metaDataIndex);
        std::vector<std::string>& tags = metaDatum.Tags;

        assert(metaDatum.Tags.size() > 0);
        if (tags.size() == 1) {
            if (tags.at(0) == FAVORITES_TAG) {
                ALOG("Removing broken metadatum %s", metaDatum.Url.c_str());
                MetaData.erase(MetaData.cbegin() + metaDataIndex);
            }
        }
    }

    // Fix the indices
    for (int metaDataIndex = 0; metaDataIndex < static_cast<int>(MetaData.size());
         ++metaDataIndex) {
        OvrMetaDatum& datum = *MetaData.at(metaDataIndex);
        std::vector<std::string>& tags = datum.Tags;

        assert(tags.size() > 0);

        if (tags.size() == 1) {
            assert(tags[0] != FAVORITES_TAG);
        }

        if (tags[0] == FAVORITES_TAG && tags.size() > 1) {
            std::swap(tags[0], tags[1]);
        }

        for (const std::string& tag : tags) {
            if (!tag.empty()) {
                if (Category* category = GetCategory(tag)) {
                    ALOG(
                        "OvrMetaData inserting index %d for datum %s to %s",
                        metaDataIndex,
                        datum.Url.c_str(),
                        category->CategoryTag.c_str());

                    // fix the metadata index itself
                    datum.Id = metaDataIndex;

                    // Update the category with the new index
                    category->DatumIndicies.push_back(metaDataIndex);
                    category->Dirty = true;
                } else {
                    ALOGW(
                        "OvrMetaData::RegenerateCategoryIndices failed to find category with tag %s for datum %s at index %d",
                        tag.c_str(),
                        datum.Url.c_str(),
                        metaDataIndex);
                }
            }
        }
    }
}

std::shared_ptr<JSON> OvrMetaData::MetaDataToJson() const {
    std::shared_ptr<JSON> DataFile = JSON::CreateObject();

    // Add version
    DataFile->AddNumberItem(VERSION, Version);

    // Add categories
    std::shared_ptr<JSON> newCategoriesObject = JSON::CreateArray();

    for (const Category& cat : Categories) {
        if (std::shared_ptr<JSON> catObject = JSON::CreateObject()) {
            catObject->AddStringItem(TAG, cat.CategoryTag.c_str());
            catObject->AddStringItem(LABEL, cat.LocaleKey.c_str());
            ALOG("OvrMetaData::MetaDataToJson adding category %s", cat.CategoryTag.c_str());
            newCategoriesObject->AddArrayElement(catObject);
        }
    }
    DataFile->AddItem(CATEGORIES, newCategoriesObject);

    // Add meta data
    std::shared_ptr<JSON> newDataObject = JSON::CreateArray();

    for (int i = 0; i < static_cast<int>(MetaData.size()); ++i) {
        const OvrMetaDatum& metaDatum = *MetaData.at(i);

        if (std::shared_ptr<JSON> datumObject = JSON::CreateObject()) {
            ExtendedDataToJson(metaDatum, datumObject);
            datumObject->AddStringItem(URL_INNER, metaDatum.Url.c_str());
            ALOG("OvrMetaData::MetaDataToJson adding datum url %s", metaDatum.Url.c_str());
            if (std::shared_ptr<JSON> newTagsObject = JSON::CreateArray()) {
                for (const auto& tag : metaDatum.Tags) {
                    if (std::shared_ptr<JSON> tagObject = JSON::CreateObject()) {
                        tagObject->AddStringItem(CATEGORY, tag.c_str());
                        newTagsObject->AddArrayElement(tagObject);
                    }
                }

                datumObject->AddItem(TAGS, newTagsObject);
            }
            newDataObject->AddArrayElement(datumObject);
        }
    }
    DataFile->AddItem(DATA, newDataObject);

    return DataFile;
}

TagAction OvrMetaData::ToggleTag(OvrMetaDatum* metaDatum, const std::string& newTag) {
    ALOG("ToggleTag tag: %s on %s", newTag.c_str(), metaDatum->Url.c_str());

    std::shared_ptr<JSON> DataFile = JSON::Load(FilePath.c_str());
    if (DataFile == nullptr) {
        ALOG("OvrMetaData failed to load JSON meta file: %s", FilePath.c_str());
        return TAG_ERROR;
    }

    assert(DataFile);
    assert(metaDatum);

    // First update the local data
    TagAction action = TAG_ERROR;
    for (int t = 0; t < static_cast<int>(metaDatum->Tags.size()); ++t) {
        if (metaDatum->Tags.at(t) == newTag) {
            // Handle case which leaves us with no tags - ie. broken state
            if (metaDatum->Tags.size() < 2) {
                ALOGW(
                    "ToggleTag attempt to remove only tag: %s on %s",
                    newTag.c_str(),
                    metaDatum->Url.c_str());
                return TAG_ERROR;
            }
            ALOG("ToggleTag TAG_REMOVED tag: %s on %s", newTag.c_str(), metaDatum->Url.c_str());
            action = TAG_REMOVED;
            metaDatum->Tags.erase(metaDatum->Tags.cbegin() + t);
            break;
        }
    }

    if (action == TAG_ERROR) {
        ALOG("ToggleTag TAG_ADDED tag: %s on %s", newTag.c_str(), metaDatum->Url.c_str());
        metaDatum->Tags.push_back(newTag);
        action = TAG_ADDED;
    }

    // Then serialize
    std::shared_ptr<JSON> newTagsObject = JSON::CreateArray();
    assert(newTagsObject);

    newTagsObject->Name = TAGS;

    for (const auto& tag : metaDatum->Tags) {
        if (std::shared_ptr<JSON> tagObject = JSON::CreateObject()) {
            tagObject->AddStringItem(CATEGORY, tag.c_str());
            newTagsObject->AddArrayElement(tagObject);
        }
    }

    if (std::shared_ptr<JSON> data = DataFile->GetItemByName(DATA)) {
        if (std::shared_ptr<JSON> datum = data->GetItemByIndex(metaDatum->Id)) {
            if (std::shared_ptr<JSON> tags = datum->GetItemByName(TAGS)) {
                ALOG(
                    "ToggleTag tag: %s on %s - found node to replace",
                    newTag.c_str(),
                    metaDatum->Url.c_str());
                datum->ReplaceNodeWith(TAGS, newTagsObject);
                ALOG(
                    "ToggleTag tag: %s on %s - node replaced",
                    newTag.c_str(),
                    metaDatum->Url.c_str());
                DataFile->Save(FilePath.c_str());
                ALOG(
                    "ToggleTag tag: %s on %s - file saved", newTag.c_str(), metaDatum->Url.c_str());
            }
        }
    }
    return action;
}

void OvrMetaData::AddCategory(const std::string& name) {
    Category cat;
    cat.CategoryTag = name;
    cat.LocaleKey = name;
    Categories.push_back(cat);
}

void OvrMetaData::InsertCategoryAt(const int index, const std::string& name) {
    if (Categories.empty()) {
        AddCategory(name);
    } else {
        Category& targetCategory = Categories.at(index);
        if (targetCategory.CategoryTag != name) {
            Category cat;
            cat.CategoryTag = name;
            cat.LocaleKey = name;
            Categories.insert(Categories.cbegin() + index, cat);
        } else {
            targetCategory.CategoryTag = name;
            targetCategory.LocaleKey = name;
        }
    }
}

OvrMetaData::Category* OvrMetaData::GetCategory(const std::string& categoryName) {
    for (Category& category : Categories) {
        if (category.CategoryTag == categoryName) {
            return &category;
        }
    }
    return NULL;
}

const OvrMetaDatum& OvrMetaData::GetMetaDatum(const int index) const {
    assert(index >= 0 && index < static_cast<int>(MetaData.size()));
    return *MetaData.at(index);
}

bool OvrMetaData::GetMetaData(
    const Category& category,
    std::vector<const OvrMetaDatum*>& outMetaData) const {
    for (const int metaDataIndex : category.DatumIndicies) {
        assert(metaDataIndex >= 0 && metaDataIndex < static_cast<int>(MetaData.size()));
        // const OvrMetaDatum * panoData = &MetaData.at( metaDataIndex );
        // ALOG( "Getting MetaData %d title %s from category %s", metaDataIndex,
        // panoData->Title.c_str(), category.CategoryName.c_str() );
        outMetaData.push_back(MetaData.at(metaDataIndex));
    }
    return true;
}

bool OvrMetaData::ShouldAddFile(
    const char* filename,
    const OvrMetaDataFileExtensions& fileExtensions) const {
    const size_t pathLen = OVR::OVR_strlen(filename);
    for (const std::string& ext : fileExtensions.BadExtensions) {
        const int extLen = ext.length();
        if (pathLen > static_cast<size_t>(extLen) &&
            OVR::OVR_stricmp(filename + pathLen - extLen, ext.c_str()) == 0) {
            return false;
        }
    }

    for (const std::string& ext : fileExtensions.GoodExtensions) {
        const int extLen = ext.length();
        if (pathLen > static_cast<size_t>(extLen) &&
            OVR::OVR_stricmp(filename + pathLen - extLen, ext.c_str()) == 0) {
            return true;
        }
    }

    return false;
}

void OvrMetaData::SetCategoryDatumIndicies(const int index, const std::vector<int>& datumIndicies) {
    assert(index < static_cast<int>(Categories.size()));

    if (index < static_cast<int>(Categories.size())) {
        Categories[index].DatumIndicies = datumIndicies;
    }
}

void OvrMetaData::DumpToLog(bool const verbose) const {
    if (verbose) {
        for (int i = 0; i < static_cast<int>(MetaData.size()); ++i) {
            ALOG("MetaData - Url: %s", MetaData[i]->Url.c_str());
        }
    }
    ALOG("MetaData - Total: %i urls", static_cast<int>(MetaData.size()));
}

} // namespace OVRFW
