/************************************************************************************

Filename    :   MetaDataManager.h
Content     :   A class to manage metadata used by FolderBrowser
Created     :   January 26, 2015
Authors     :   Jonathan E. Wright, Warsam Osman, Madhu Kalva

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#pragma once

#include <vector>
#include <string>
#include <unordered_map>

#include "OVR_JSON.h"

namespace OVRFW {

//==============================================================
// OvrMetaData
struct OvrMetaDatum {
    mutable int FolderIndex; // index of the folder this meta data appears in (not serialized!)
    mutable int PanelId; // panel id associated with this meta data (not serialized!)
    int Id; // index into the array read from the JSON (not serialized!)
    std::vector<std::string> Tags;
    std::string Url;

   protected:
    OvrMetaDatum() {}
};

enum TagAction { TAG_ADDED, TAG_REMOVED, TAG_ERROR };

struct OvrMetaDataFileExtensions {
    std::vector<std::string> GoodExtensions;
    std::vector<std::string> BadExtensions;
};

class OvrMetaData {
   public:
    struct Category {
        Category() : Dirty(true) {}
        std::string CategoryTag;
        std::string LocaleKey;
        std::vector<int> DatumIndicies;
        bool Dirty;
    };

    OvrMetaData() : Version(-1.0) {}

    virtual ~OvrMetaData() {}

    // Init meta data from contents on disk
    void InitFromDirectory(
        const char* relativePath,
        const std::vector<std::string>& searchPaths,
        const OvrMetaDataFileExtensions& fileExtensions);

    // Init meta data from a passed in list of files
    void InitFromFileList(
        const std::vector<std::string>& fileList,
        const OvrMetaDataFileExtensions& fileExtensions);

    // Check specific paths for media and reconcile against stored/new metadata (Maintained for SDK)
    void InitFromDirectoryMergeMeta(
        const char* relativePath,
        const std::vector<std::string>& searchPaths,
        const OvrMetaDataFileExtensions& fileExtensions,
        const char* metaFile,
        const char* packageName);

    // File list passed in and we reconcile against stored/new metadata
    void InitFromFileListMergeMeta(
        const std::vector<std::string>& fileList,
        const std::vector<std::string>& searchPaths,
        const OvrMetaDataFileExtensions& fileExtensions,
        const char* appFileStoragePath,
        const char* metaFile,
        std::shared_ptr<OVR::JSON> storedMetaData);

    void ProcessRemoteMetaFile(
        const char* metaFileString,
        const int startInsertionIndex /* index to insert remote categories*/);

    // Extracts metadata from passed in JSON dataFile and merges it with the base one in assets if
    // needed
    void ProcessMetaData(
        std::shared_ptr<OVR::JSON> dataFile,
        const std::vector<std::string>& searchPaths,
        const char* metaFile);

    // Rename a category after construction
    void RenameCategory(const char* currentTag, const char* newName);

    // Rename a category tag after construction
    bool RenameCategoryTag(const char* currentTag, const char* newName);

    // Adds or removes tag and returns action taken
    TagAction ToggleTag(OvrMetaDatum* data, const std::string& tag);

    // Returns metaData file if one is found, otherwise creates one using the default meta.json in
    // the assets folder
    std::shared_ptr<OVR::JSON> CreateOrGetStoredMetaFile(
        const char* appFileStoragePath,
        const char* metaFile);
    void AddCategory(const std::string& name);
    void InsertCategoryAt(const int index, const std::string& name);

    const std::vector<Category>& GetCategories() const {
        return Categories;
    }
    const std::vector<OvrMetaDatum*>& GetMetaData() const {
        return MetaData;
    }
    const Category& GetCategory(const int index) const {
        return Categories[index];
    }
    Category& GetCategory(const int index) {
        return Categories[index];
    }
    const OvrMetaDatum& GetMetaDatum(const int index) const;
    bool GetMetaData(const Category& category, std::vector<const OvrMetaDatum*>& outMetaData) const;
    void SetCategoryDatumIndicies(const int index, const std::vector<int>& datumIndicies);
    void DumpToLog(bool const verbose) const;
    void PrintCategories() const;
    void RegenerateCategoryIndices();

   protected:
    // Overload to fill extended data during initialization
    virtual OvrMetaDatum* CreateMetaDatum(const char* fileName) const = 0;
    virtual void ExtractExtendedData(const OVR::JsonReader& jsonDatum, OvrMetaDatum& outDatum)
        const = 0;
    virtual void ExtendedDataToJson(
        const OvrMetaDatum& datum,
        std::shared_ptr<OVR::JSON> outDatumObject) const = 0;
    virtual void SwapExtendedData(OvrMetaDatum* left, OvrMetaDatum* right) const = 0;

    // Optional protected interface
    virtual bool IsRemote(const OvrMetaDatum* /*datum*/) const {
        return true;
    }

    // Removes duplicate entries from newData
    virtual void DedupMetaData(
        std::vector<OvrMetaDatum*>& existingData,
        std::unordered_map<std::string, OvrMetaDatum*>& newData);

    void InsertCategoryList(const int startIndex, const std::vector<Category>& categoryList);

    double GetVersion() {
        return Version;
    }
    void SetVersion(const double val) {
        Version = val;
    }
    std::vector<OvrMetaDatum*>& GetMetaData() {
        return MetaData;
    }
    void SetMetaData(const std::vector<OvrMetaDatum*>& newData) {
        MetaData = newData;
    }

    Category* GetCategory(const std::string& categoryName);

    void ReconcileMetaData(std::unordered_map<std::string, OvrMetaDatum*>& storedMetaData);
    void ReconcileCategories(std::vector<Category>& storedCategories);

    std::shared_ptr<OVR::JSON> MetaDataToJson() const;
    void WriteMetaFile(const char* metaFile) const;
    bool ShouldAddFile(const char* filename, const OvrMetaDataFileExtensions& fileExtensions) const;
    void ExtractVersion(std::shared_ptr<OVR::JSON> dataFile, double& outVersion) const;
    void ExtractCategories(
        std::shared_ptr<OVR::JSON> dataFile,
        std::vector<Category>& outCategories) const;
    void ExtractMetaData(
        std::shared_ptr<OVR::JSON> dataFile,
        const std::vector<std::string>& searchPaths,
        std::unordered_map<std::string, OvrMetaDatum*>& outMetaData) const;
    void ExtractRemoteMetaData(
        std::shared_ptr<OVR::JSON> dataFile,
        std::unordered_map<std::string, OvrMetaDatum*>& outMetaData) const;
    void Serialize();

   private:
    std::string FilePath;
    std::vector<Category> Categories;
    std::vector<OvrMetaDatum*> MetaData;
    std::unordered_map<std::string, int> UrlToIndex;
    double Version;
};

} // namespace OVRFW
