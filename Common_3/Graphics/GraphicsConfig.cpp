/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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

#include "GraphicsConfig.h"

#include "../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"

#include "../Utilities/Interfaces/IFileSystem.h"
#include "Interfaces/IGraphics.h"

#include "../Utilities/Interfaces/IMemory.h"

///////////////////////////////////////////////////////////
// HELPER DECLARATIONS
#define INVALID_OPTION                 UINT_MAX
#define MAXIMUM_GPU_COMPARISON_CHOICES 256
#define MAXIMUM_GPU_SETTINGS           256

#define GPUCFG_VERSION_MAJOR           0
#define GPUCFG_VERSION_MINOR           2

extern PlatformParameters gPlatformParameters;
GPUPresetLevel            gDefaultPresetLevel;

typedef uint64_t (*PropertyGetter)(const GPUSettings* pSetting);
typedef void (*PropertySetter)(GPUSettings* pSetting, uint64_t value);

#define GPU_CONFIG_PROPERTY(name, prop)                                             \
    {                                                                               \
        name, [](const GPUSettings* pSetting) { return (uint64_t)pSetting->prop; }, \
            [](GPUSettings* pSetting, uint64_t value)                               \
        {                                                                           \
            COMPILE_ASSERT(sizeof(decltype(pSetting->prop)) <= sizeof(value));      \
            pSetting->prop = (decltype(pSetting->prop))value;                       \
        }                                                                           \
    }

#define GPU_CONFIG_PROPERTY_READ_ONLY(name, prop)                                   \
    {                                                                               \
        name, [](const GPUSettings* pSetting) { return (uint64_t)pSetting->prop; }, \
            [](GPUSettings* pSetting, uint64_t value)                               \
        {                                                                           \
            UNREF_PARAM(value);                                                     \
            LOGF(eDEBUG, "GPUConfig: Unsupported setting %s from gpu.cfg", name);   \
            ASSERT(false);                                                          \
        }                                                                           \
    }

struct GPUProperty
{
    const char*    name;
    PropertyGetter getter;
    PropertySetter setter;
};

// should we enable all setter? modifying the model or vendor id for example...
const GPUProperty availableGpuProperties[] = {
    GPU_CONFIG_PROPERTY("allowbuffertextureinsameheap", mAllowBufferTextureInSameHeap),
    GPU_CONFIG_PROPERTY("amdasicfamily", mAmdAsicFamily),
    GPU_CONFIG_PROPERTY("builtindrawid", mBuiltinDrawID),
#if defined(METAL)
    GPU_CONFIG_PROPERTY("cubemaptexturearraysupported", mCubeMapTextureArraySupported),
    GPU_CONFIG_PROPERTY("tessellationindirectdrawsupported", mTessellationIndirectDrawSupported),
#if !defined(TARGET_IOS)
    GPU_CONFIG_PROPERTY("isheadless", mIsHeadLess),
#endif
#endif
    GPU_CONFIG_PROPERTY_READ_ONLY("deviceid", mGpuVendorPreset.mModelId),
#if defined(DIRECT3D11) || defined(DIRECT3D12)
    GPU_CONFIG_PROPERTY("directxfeaturelevel", mFeatureLevel),
#endif
    GPU_CONFIG_PROPERTY("geometryshadersupported", mGeometryShaderSupported),
    GPU_CONFIG_PROPERTY("gpupresetlevel", mGpuVendorPreset.mPresetLevel),
    GPU_CONFIG_PROPERTY("graphicqueuesupported", mGraphicsQueueSupported),
    GPU_CONFIG_PROPERTY("hdrsupported", mHDRSupported),
#if defined(VULKAN)
    GPU_CONFIG_PROPERTY("dynamicrenderingenabled", mDynamicRenderingSupported),
    GPU_CONFIG_PROPERTY("xclipsetransferqueueworkaroundenabled", mXclipseTransferQueueWorkaround),
#endif
    GPU_CONFIG_PROPERTY("indirectcommandbuffer", mIndirectCommandBuffer),
    GPU_CONFIG_PROPERTY("indirectrootconstant", mIndirectRootConstant),
    GPU_CONFIG_PROPERTY("maxboundtextures", mMaxBoundTextures),
#if defined(DIRECT3D12)
    GPU_CONFIG_PROPERTY("maxrootsignaturedwords", mMaxRootSignatureDWORDS),
#endif
    GPU_CONFIG_PROPERTY("maxvertexinputbindings", mMaxVertexInputBindings),
    GPU_CONFIG_PROPERTY("multidrawindirect", mMultiDrawIndirect),
    GPU_CONFIG_PROPERTY("occlusionqueries", mOcclusionQueries),
    GPU_CONFIG_PROPERTY("pipelinestatsqueries", mPipelineStatsQueries),
    GPU_CONFIG_PROPERTY("primitiveidsupported", mPrimitiveIdSupported),
    GPU_CONFIG_PROPERTY("rasterorderviewsupport", mROVsSupported),
    GPU_CONFIG_PROPERTY("raytracingsupported", mRaytracingSupported),
    GPU_CONFIG_PROPERTY("rayquerysupported", mRayQuerySupported),
    GPU_CONFIG_PROPERTY("raypipelinesupported", mRayPipelineSupported),
    GPU_CONFIG_PROPERTY("softwarevrssupported", mSoftwareVRSSupported),
    GPU_CONFIG_PROPERTY("tessellationsupported", mTessellationSupported),
    GPU_CONFIG_PROPERTY("timestampqueries", mTimestampQueries),
    GPU_CONFIG_PROPERTY("uniformbufferalignment", mUniformBufferAlignment),
    GPU_CONFIG_PROPERTY("uploadbuffertexturealignment", mUploadBufferTextureAlignment),
    GPU_CONFIG_PROPERTY("uploadbuffertexturerowalignment", mUploadBufferTextureRowAlignment),
    GPU_CONFIG_PROPERTY_READ_ONLY("vendorid", mGpuVendorPreset.mVendorId),
    GPU_CONFIG_PROPERTY("vram", mVRAM),
    GPU_CONFIG_PROPERTY("wavelanecount", mWaveLaneCount),
    GPU_CONFIG_PROPERTY("waveopssupport", mWaveOpsSupportFlags),
};

void setDefaultGPUSettings(GPUSettings* pGpuSettings)
{
    memset(pGpuSettings, 0, sizeof(GPUSettings));

    pGpuSettings->mSamplerAnisotropySupported = 1;
    pGpuSettings->mGraphicsQueueSupported = 1;
    pGpuSettings->mPrimitiveIdSupported = 1;
}

/* ------------------------ gpu.data ------------------------ */
// intel use 3 identifiers 0x163C, 0x8086, 0x8087;
#define MAX_GPU_VENDOR_COUNT                 64
#define MAX_GPU_VENDOR_IDENTIFIER_LENGTH     16
#define MAX_INDENTIFIER_PER_GPU_VENDOR_COUNT 8
struct GPUVendorDefinition
{
    char     vendorName[MAX_GPU_VENDOR_STRING_LENGTH] = {};
    uint32_t identifierArray[MAX_INDENTIFIER_PER_GPU_VENDOR_COUNT] = {};
    uint32_t identifierCount = 0;
};

struct GPUModelDefinition
{
    uint32_t       mVendorId;
    uint32_t       mDeviceId;
    GPUPresetLevel mPreset;
    char           mModelName[MAX_GPU_VENDOR_STRING_LENGTH];
};

static GPUModelDefinition* gGPUModels = NULL;

/* ------------------------ gpu.cfg ------------------------ */
struct ConfigurationRule
{
    const GPUProperty* pGpuProperty = NULL;
    char               comparator[3] = ""; // optional
    uint64_t           comparatorValue = INVALID_OPTION;
};

struct GPUComparisonChoice
{
    ConfigurationRule* pGpuComparisonRules = NULL;
    uint32_t           comparisonRulesCount = 0;
};

struct ConfigurationSetting
{
    const GPUProperty* pUpdateProperty = NULL;
    ConfigurationRule* pConfigurationRules = NULL;
    uint32_t           comparisonRulesCount = 0;
    uint64_t           assignmentValue = 0;
};

struct UserSetting
{
    const char*        name = NULL;
    uint32_t*          pSettingValue = NULL;
    ConfigurationRule* pConfigurationRules = NULL;
    uint32_t           comparisonRulesCount = 0;
    uint32_t           assignmentValue = 0;
};

struct DriverVersion
{
    uint32_t versionNumbers[4] = {};
    uint32_t versionNumbersCount = 0;
};

struct DriverRejectionRule
{
    uint32_t      vendorId = 0;
    char          comparator[3] = "";
    DriverVersion driverComparisonValue = {};
    char          reasonStr[MAX_GPU_VENDOR_STRING_LENGTH] = {};
};
// ------ Interpreting Helpers ------ //
bool               isValidGPUVendorId(uint32_t vendorId);
bool               isValidGPUVendorId(uint32_t vendorId);
const char*        getGPUVendorName(uint32_t modelId);
const GPUProperty* propertyNameToGpuProperty(const char* str);
bool               compare(const char* cmp, uint64_t, uint64_t);
bool               compare(const char* cmp, const char*, const char*);
uint32_t           getSettingIndex(const char* string, uint32_t numSettings, const char** gameSettingNames);
bool               parseDriverVersion(const char* driverStr, DriverVersion* pDriverVersionOut);

// -------- Parsing Helpers ------- //
char*          stringToLower(char* str);
bool           stringToInteger(char* str, uint32_t* pOutResult, uint32_t base);
bool           stringToLargeInteger(char* str, uint64_t* pOutResult, uint32_t base);
bool           contains(char* str, const char* sub_str);
void           tokenizeLine(const char* pLine, const char* pLineEnd, uint32_t numTokens, char** pTokens);
GPUPresetLevel getSinglePresetLevel(const char* line, const char* inVendorName, const char* inModelName, const char* inModelId,
                                    const char* inRevId);
bool           parseConfigLine(const char* pLine, const char* pInVendorName, const char* pInModelName, const char* pInModelId,
                               const char* pInRevisionId, char pOutVendorName[MAX_GPU_VENDOR_STRING_LENGTH],
                               char pOutModelName[MAX_GPU_VENDOR_STRING_LENGTH], char pOutModelId[MAX_GPU_VENDOR_STRING_LENGTH],
                               char pOutRevisionId[MAX_GPU_VENDOR_STRING_LENGTH], GPUPresetLevel* pOutPresetLevel);
void           parseGPUDataFile();
void           parseGPUConfigFile(ExtendedSettings* pExtendedSettings);
void           parseDefaultDataConfigurationLine(char* currentLine);
void           parseGPUVendorLine(char* currentLine);
void           parseGPUSelectionLine(char* currentLine);
void           parseDriverRejectionLine(char* currentLine);
void           parseConfigurationSettingLine(char* currentLine);
void           parseUserExtendedSettingLine(char* currentLine, ExtendedSettings* pExtendedSettings);
void           parseConfigurationRules(ConfigurationRule** ppConfigurationRules, uint32_t* pRulesCount, char* ruleStr);
void           printConfigureRules(ConfigurationRule* pRules, uint32_t rulesCount, char* result);

// -------- Buffered file reader ------- //
bool bufferedGetLine(char* lineStrOut, char** bufferCursorInOut, const char* bufferEnd);

///////////////////////////////////////////////////////////
// CONFIG INTERFACE

typedef enum DataParsingStatus
{
    DATA_PARSE_NONE,
    DATA_PARSE_DEFAULT_CONFIGURATION,
    DATA_PARSE_GPU_VENDOR,
    DATA_PARSE_GPU_MODEL,
} DataParsingStatus;

typedef enum ConfigParsingStatus
{
    CONFIG_PARSE_NONE,
    CONFIG_PARSE_SELECTION_RULE,
    CONFIG_PARSE_DRIVER_REJECTION,
    CONFIG_PARSE_GPU_CONFIGURATION,
    CONFIG_PARSE_USER_EXTENDED_SETTINGS,
} ConfigParsingStatus;

// ------ gpu.data
static GPUVendorDefinition  gGPUVendorDefinitions[MAX_GPU_VENDOR_COUNT] = {};
static uint32_t             gGPUVendorCount = 0;
static char*                gGpuDataFileBuffer = nullptr;
// ------ gpu.cfg
static GPUComparisonChoice  gGPUComparisonChoices[MAXIMUM_GPU_COMPARISON_CHOICES] = {};
static ConfigurationSetting gConfigurationSettings[MAXIMUM_GPU_SETTINGS];
static UserSetting          gUserSettings[MAXIMUM_GPU_SETTINGS];
// gDriverRejectionRules[MAXIMUM_GPU_COMPARISON_CHOICES]; 72776 bytes moves it on the heap instead
static DriverRejectionRule* gDriverRejectionRules = NULL;
static uint32_t             gGPUComparisonChoiceCount = 0;
static uint32_t             gDriverRejectionRulesCount = 0;
static uint32_t             gConfigurationSettingsCount = 0;
static uint32_t             gUserExtendedSettingsCount = 0;

void addGPUConfigurationRules(ExtendedSettings* pExtendedSettings)
{
    fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
    parseGPUDataFile();
    parseGPUConfigFile(pExtendedSettings);
}

void parseGPUDataFile()
{
    gDefaultPresetLevel = GPUPresetLevel::GPU_PRESET_LOW;

    FileStream fh = {};
    if (!fsOpenStreamFromPath(RD_GPU_CONFIG, "gpu.data", FM_READ, &fh))
    {
        LOGF(LogLevel::eWARNING, "gpu.data could not be found, setting preset will be set to Low as a default.");
        return;
    }
    DataParsingStatus parsingStatus = DataParsingStatus::DATA_PARSE_NONE;
    char              currentLineStr[1024] = {};
    gGPUVendorCount = 0;
    char* gpuListBeginFileCursor = nullptr;
    char* gpuListEndFileCursor = nullptr;

    size_t fileSize = fsGetStreamFileSize(&fh);
    gGpuDataFileBuffer = (char*)tf_malloc(fileSize * sizeof(char));
    fsReadFromStream(&fh, (void*)gGpuDataFileBuffer, fileSize);
    char* fileCursor = gGpuDataFileBuffer;
    char* gGpuDataFileEnd = gGpuDataFileBuffer + fileSize;
    char* previousLineCursor = fileCursor;

    if (bufferedGetLine(currentLineStr, &fileCursor, gGpuDataFileEnd))
    {
        uint32_t versionMajor = 0;
        uint32_t versionMinor = 0;
        int      read = sscanf(currentLineStr, "version:%u.%u", &versionMajor, &versionMinor);
        if (read != 2)
        {
            LOGF(eINFO, "Ill-formatted gpu.data file. Missing version at beginning of file");
            fsCloseStream(&fh);
            return;
        }
        else if (versionMajor != GPUCFG_VERSION_MAJOR || versionMinor != GPUCFG_VERSION_MINOR)
        {
            LOGF(eINFO, "gpu.data version mismatch. Expected version %u.%u but got %u.%u", GPUCFG_VERSION_MAJOR, GPUCFG_VERSION_MINOR,
                 versionMajor, versionMinor);
            fsCloseStream(&fh);
            return;
        }
    }

    while (bufferedGetLine(currentLineStr, &fileCursor, gGpuDataFileEnd))
    {
        char*       lineCursor = currentLineStr;
        // skip spaces, empty line and comments
        size_t      ruleLength = strcspn(lineCursor, "#");
        const char* pLineEnd = lineCursor + ruleLength;
        while (currentLineStr != pLineEnd && isspace(*lineCursor))
        {
            ++lineCursor;
        }
        if (lineCursor == pLineEnd)
        {
            continue;
        }

        switch (parsingStatus)
        {
        case DataParsingStatus::DATA_PARSE_NONE:
            if (strcmp(currentLineStr, "BEGIN_DEFAULT_CONFIGURATION;") == 0)
            {
                parsingStatus = DataParsingStatus::DATA_PARSE_DEFAULT_CONFIGURATION;
            }
            else if (strcmp(currentLineStr, "BEGIN_VENDOR_LIST;") == 0)
            {
                parsingStatus = DataParsingStatus::DATA_PARSE_GPU_VENDOR;
            }
            else if (strcmp(currentLineStr, "BEGIN_GPU_LIST;") == 0)
            {
                // mark the first line after BEGIN_GPU_SELECTION as the beginingCursor
                parsingStatus = DataParsingStatus::DATA_PARSE_GPU_MODEL;
                gpuListBeginFileCursor = fileCursor;
            }
            break;
        case DataParsingStatus::DATA_PARSE_GPU_VENDOR:
            if (strcmp(currentLineStr, "END_VENDOR_LIST;") == 0)
            {
                parsingStatus = DataParsingStatus::DATA_PARSE_NONE;
            }
            else
            {
                parseGPUVendorLine(lineCursor);
            }
            break;
        case DataParsingStatus::DATA_PARSE_GPU_MODEL:
            if (strcmp(currentLineStr, "END_GPU_LIST;") == 0)
            {
                // in an ideal world it would need to be before END_VENDOR_LIST; but we can live with this
                gpuListEndFileCursor = previousLineCursor;
                parsingStatus = DataParsingStatus::DATA_PARSE_NONE;
            }
            else
            {
                char  vendorIdStr[256] = {};
                char  modelIdStr[256] = {};
                char  presetStr[256] = {};
                char  vendorNameStr[256] = {};
                char  modelNameStr[256] = {};
                char* tokens[] = { vendorIdStr, modelIdStr, presetStr, vendorNameStr, modelNameStr };
                tokenizeLine(currentLineStr, pLineEnd, TF_ARRAY_COUNT(tokens), tokens);
                GPUModelDefinition model = {};
                model.mVendorId = (uint32_t)strtoul(vendorIdStr + 2, NULL, 16);
                model.mDeviceId = (uint32_t)strtoul(modelIdStr + 2, NULL, 16);
                model.mPreset = stringToPresetLevel(presetStr);
                if (modelNameStr[0] != '\0')
                {
                    strncpy(model.mModelName, modelNameStr, TF_ARRAY_COUNT(modelNameStr));
                }
                if (model.mVendorId && model.mDeviceId)
                {
                    arrpush(gGPUModels, model);
                }
            }
            break;
        case DataParsingStatus::DATA_PARSE_DEFAULT_CONFIGURATION:
        {
            if (strcmp(currentLineStr, "END_DEFAULT_CONFIGURATION;") == 0)
            {
                parsingStatus = DataParsingStatus::DATA_PARSE_NONE;
            }
            else
            {
                parseDefaultDataConfigurationLine(lineCursor);
            }
            break;
        }
        default:
            break;
        }
        previousLineCursor = fileCursor;
    }

    // log default configuration
    LOGF(eINFO, "Default GPU Data:");
    LOGF(eINFO, "    DefaultGPUPresetLevel set to %s", presetLevelToString(gDefaultPresetLevel));
    // log gpu vendors
    LOGF(eINFO, "GPU vendors:");
    for (uint32_t vendorIndex = 0; vendorIndex < gGPUVendorCount; vendorIndex++)
    {
        char                 vendorStr[1024] = { '\0' };
        GPUVendorDefinition* currentGPUVendor = &gGPUVendorDefinitions[vendorIndex];
        int                  success = snprintf(vendorStr, 1024, "%s: ", currentGPUVendor->vendorName);
        if (success > 0)
        {
            const char* separator = "";
            for (uint32_t identifierIndex = 0; identifierIndex < currentGPUVendor->identifierCount; identifierIndex++)
            {
                char versionNumber[MAX_GPU_VENDOR_IDENTIFIER_LENGTH] = {};
                snprintf(versionNumber, MAX_GPU_VENDOR_IDENTIFIER_LENGTH, "%s%#x", separator,
                         currentGPUVendor->identifierArray[identifierIndex]);
                strcat(vendorStr, versionNumber);
                separator = ", ";
            }
        }
        LOGF(eINFO, "    %s", vendorStr);
    }

    if (!gpuListBeginFileCursor || !gpuListEndFileCursor)
    {
        LOGF(eINFO, "Could not find a valid list of gpu in gpu.data please check BEGIN_GPU_LIST; END_GPU_LIST; is properly defined");
    }

    fsCloseStream(&fh);
}

void parseGPUConfigFile(ExtendedSettings* pExtendedSettings)
{
    FileStream fh = {};
    if (!fsOpenStreamFromPath(RD_GPU_CONFIG, "gpu.cfg", FM_READ, &fh))
    {
        LOGF(LogLevel::eWARNING, "gpu.cfg could not be found, first gpu found set as active gpu.");
        return;
    }
    ConfigParsingStatus parsingStatus = ConfigParsingStatus::CONFIG_PARSE_NONE;
    char                currentLineStr[1024] = {};
    gDriverRejectionRules = (DriverRejectionRule*)tf_malloc(MAXIMUM_GPU_SETTINGS * sizeof(DriverRejectionRule));
    gConfigurationSettingsCount = 0;
    gDriverRejectionRulesCount = 0;
    gGPUComparisonChoiceCount = 0;
    gUserExtendedSettingsCount = 0;

    size_t fileSize = fsGetStreamFileSize(&fh);
    char*  fileBuffer = (char*)tf_malloc(fileSize * sizeof(char));
    fsReadFromStream(&fh, fileBuffer, fileSize);
    char* fileCursor = fileBuffer;
    char* fileEnd = fileBuffer + fileSize;
    while (bufferedGetLine(currentLineStr, &fileCursor, fileEnd))
    {
        char*       lineCursor = currentLineStr;
        // skip spaces, empty line and comments
        size_t      ruleLength = strcspn(lineCursor, "#");
        const char* pLineEnd = lineCursor + ruleLength;
        while (currentLineStr != pLineEnd && isspace(*lineCursor))
            ++lineCursor;
        if (lineCursor == pLineEnd)
            continue;

        // parse configuration rules and settings
        switch (parsingStatus)
        {
        case ConfigParsingStatus::CONFIG_PARSE_NONE:
            if (strcmp(currentLineStr, "BEGIN_GPU_SELECTION;") == 0)
            {
                parsingStatus = ConfigParsingStatus::CONFIG_PARSE_SELECTION_RULE;
            }
            else if (strcmp(currentLineStr, "BEGIN_DRIVER_REJECTION;") == 0)
            {
                parsingStatus = ConfigParsingStatus::CONFIG_PARSE_DRIVER_REJECTION;
            }
            else if (strcmp(currentLineStr, "BEGIN_GPU_SETTINGS;") == 0)
            {
                parsingStatus = ConfigParsingStatus::CONFIG_PARSE_GPU_CONFIGURATION;
            }
            else if (strcmp(currentLineStr, "BEGIN_USER_SETTINGS;") == 0)
            {
                if (pExtendedSettings != NULL)
                    parsingStatus = ConfigParsingStatus::CONFIG_PARSE_USER_EXTENDED_SETTINGS;
            }
            break;
        case ConfigParsingStatus::CONFIG_PARSE_SELECTION_RULE:
            if (strcmp(currentLineStr, "END_GPU_SELECTION;") == 0)
            {
                parsingStatus = ConfigParsingStatus::CONFIG_PARSE_NONE;
            }
            else
            {
                parseGPUSelectionLine(lineCursor);
            }
            break;
        case ConfigParsingStatus::CONFIG_PARSE_DRIVER_REJECTION:
            if (strcmp(currentLineStr, "END_DRIVER_REJECTION;") == 0)
            {
                parsingStatus = ConfigParsingStatus::CONFIG_PARSE_NONE;
            }
            else
            {
                parseDriverRejectionLine(lineCursor);
            }
            break;
        case ConfigParsingStatus::CONFIG_PARSE_GPU_CONFIGURATION:
            if (strcmp(currentLineStr, "END_GPU_SETTINGS;") == 0)
            {
                parsingStatus = ConfigParsingStatus::CONFIG_PARSE_NONE;
            }
            else
            {
                parseConfigurationSettingLine(lineCursor);
            }
            break;
        case ConfigParsingStatus::CONFIG_PARSE_USER_EXTENDED_SETTINGS:
            if (strcmp(currentLineStr, "END_USER_SETTINGS;") == 0)
            {
                parsingStatus = ConfigParsingStatus::CONFIG_PARSE_NONE;
            }
            else
            {
                ASSERT(pExtendedSettings);
                parseUserExtendedSettingLine(lineCursor, pExtendedSettings);
            }
            break;
        default:
            break;
        }
    }

    // log configuration rules and settings
    LOGF(eINFO, "GPU selection settings:");
    for (uint32_t choiceIndex = 0; choiceIndex < gGPUComparisonChoiceCount; choiceIndex++)
    {
        char                 choiceStr[1024] = { '\0' };
        GPUComparisonChoice* currentGPUChoice = &gGPUComparisonChoices[choiceIndex];
        printConfigureRules(currentGPUChoice->pGpuComparisonRules, currentGPUChoice->comparisonRulesCount, choiceStr);
        LOGF(eINFO, "    Rule: %s", choiceStr);
    }
    // log driver rejection rules
    LOGF(eINFO, "Driver rejections:");
    for (uint32_t driverIndex = 0; driverIndex < gDriverRejectionRulesCount; driverIndex++)
    {
        char                 ruleStr[1024] = {};
        DriverRejectionRule* currentRule = &gDriverRejectionRules[driverIndex];
        snprintf(ruleStr, 1024, "%s: DriverVersion %s ", getGPUVendorName(currentRule->vendorId), currentRule->comparator);
        const char* separator = "";
        for (uint32_t numberIndex = 0; numberIndex < currentRule->driverComparisonValue.versionNumbersCount; numberIndex++)
        {
            char versionNumber[32] = {};
            snprintf(versionNumber, 32, "%s%u", separator, currentRule->driverComparisonValue.versionNumbers[numberIndex]);
            strcat(ruleStr, versionNumber);
            separator = ".";
        }
        strcat(ruleStr, "; ");
        strcat(ruleStr, currentRule->reasonStr);
        LOGF(eINFO, "    %s", ruleStr);
    }
    LOGF(eINFO, "GPU configuration settings:");
    for (uint32_t settingIndex = 0; settingIndex < gConfigurationSettingsCount; settingIndex++)
    {
        char                  settingStr[1024] = {};
        char                  assignmentValueStr[32] = {};
        ConfigurationSetting* currentConfigurationSetting = &gConfigurationSettings[settingIndex];
        snprintf(settingStr, 1024, "%s: ", currentConfigurationSetting->pUpdateProperty->name);
        printConfigureRules(currentConfigurationSetting->pConfigurationRules, currentConfigurationSetting->comparisonRulesCount,
                            settingStr);
        snprintf(assignmentValueStr, 32, ": %lld", (long long)currentConfigurationSetting->assignmentValue);
        strcat(settingStr, assignmentValueStr);
        LOGF(eINFO, "    %s", settingStr);
    }
    LOGF(eINFO, "User settings:");
    for (uint32_t settingIndex = 0; settingIndex < gUserExtendedSettingsCount; settingIndex++)
    {
        char         settingStr[1024] = {};
        char         assignmentValueStr[32] = {};
        UserSetting* currentUserSetting = &gUserSettings[settingIndex];
        snprintf(settingStr, 1024, "%s: ", currentUserSetting->name);
        printConfigureRules(currentUserSetting->pConfigurationRules, currentUserSetting->comparisonRulesCount, settingStr);
        snprintf(assignmentValueStr, 32, ": %u", currentUserSetting->assignmentValue);
        strcat(settingStr, assignmentValueStr);
        LOGF(eINFO, "    %s", settingStr);
    }

    tf_free(fileBuffer);
    fsCloseStream(&fh);
}

void removeGPUConfigurationRules()
{
    tf_free(gGpuDataFileBuffer);
    tf_free(gDriverRejectionRules);
    arrfree(gGPUModels);
    gDriverRejectionRules = NULL;

    for (uint32_t i = 0; i < gGPUComparisonChoiceCount; i++)
    {
        tf_free(gGPUComparisonChoices[i].pGpuComparisonRules);
    }

    for (uint32_t i = 0; i < gConfigurationSettingsCount; i++)
    {
        tf_free(gConfigurationSettings[i].pConfigurationRules);
    }

    for (uint32_t i = 0; i < gUserExtendedSettingsCount; i++)
    {
        tf_free(gUserSettings[i].pConfigurationRules);
    }
    gGPUComparisonChoiceCount = 0;
    gDriverRejectionRulesCount = 0;
    gConfigurationSettingsCount = 0;
    gUserExtendedSettingsCount = 0;
}

void parseDefaultDataConfigurationLine(char* currentLine)
{
    size_t      ruleLength = strcspn(currentLine, "#");
    const char* pLineEnd = currentLine + ruleLength;
    char        ruleName[MAX_GPU_VENDOR_STRING_LENGTH] = {};
    char        assignmentValue[MAX_GPU_VENDOR_STRING_LENGTH] = {};
    char*       tokens[] = { ruleName, assignmentValue };
    tokenizeLine(currentLine, pLineEnd, TF_ARRAY_COUNT(tokens), tokens);

    if (strcmp(ruleName, "DefaultPresetLevel") == 0)
    {
        stringToLower(assignmentValue);
        GPUPresetLevel defaultLevel = stringToPresetLevel(assignmentValue);
        if (defaultLevel != GPUPresetLevel::GPU_PRESET_NONE)
        {
            gDefaultPresetLevel = defaultLevel;
        }
        else
        {
            LOGF(eDEBUG, "Error invalid preset level in GPU Default Data Configuration value '%s' in '%s'.", assignmentValue, currentLine);
        }
    }
    else
    {
        LOGF(eDEBUG, "Error could not parse GPU Default Data Configuration rule '%s' in '%s'.", ruleName, currentLine);
    }
}

void parseGPUVendorLine(char* currentLine)
{
    char                 gpuIdentifierList[MAX_GPU_VENDOR_STRING_LENGTH] = {};
    char                 gpuIdentifier[MAX_GPU_VENDOR_IDENTIFIER_LENGTH] = {};
    size_t               ruleLength = strcspn(currentLine, "#");
    const char*          pLineEnd = currentLine + ruleLength;
    GPUVendorDefinition* currentVendor = &gGPUVendorDefinitions[gGPUVendorCount];
    *currentVendor = {};
    char* tokens[] = { currentVendor->vendorName, gpuIdentifierList };
    tokenizeLine(currentLine, pLineEnd, TF_ARRAY_COUNT(tokens), tokens);

    char* currentIdentifier = gpuIdentifierList;
    char* identifierEnd = gpuIdentifierList + strlen(gpuIdentifierList);
    while (currentIdentifier < identifierEnd)
    {
        //  read in the field name
        size_t optionLength = strcspn(currentIdentifier, ",");
        optionLength = TF_MIN(optionLength, MAX_GPU_VENDOR_IDENTIFIER_LENGTH - 1);
        strncpy(gpuIdentifier, currentIdentifier, optionLength);
        gpuIdentifier[optionLength] = '\0';
        bool validConversion = stringToInteger(gpuIdentifier, &currentVendor->identifierArray[currentVendor->identifierCount], 16);
        if (validConversion)
        {
            currentVendor->identifierCount++;
        }
        else
        {
            LOGF(eDEBUG, "Invalid GPU vendor identifier %s from line: '%s'.", gpuIdentifier, currentLine);
            break;
        }
        currentIdentifier += optionLength;
        currentIdentifier += strspn(currentIdentifier, " ,");
    }

    if (currentVendor->identifierCount > 0)
    {
        gGPUVendorCount++;
    }
    else
    {
        LOGF(eDEBUG, "Error could not parse GPU vendor in '%s'.", currentLine);
    }
}

void parseGPUSelectionLine(char* currentLine)
{
    // parse selection rule
    char                 ruleParameters[MAX_GPU_VENDOR_STRING_LENGTH] = {};
    size_t               ruleLength = strcspn(currentLine, "#");
    const char*          pLineEnd = currentLine + ruleLength;
    GPUComparisonChoice* currentChoice = &gGPUComparisonChoices[gGPUComparisonChoiceCount];
    *currentChoice = {};
    char* tokens[] = { ruleParameters };
    tokenizeLine(currentLine, pLineEnd, TF_ARRAY_COUNT(tokens), tokens);

    // parse comparison rules separated by ","
    parseConfigurationRules(&currentChoice->pGpuComparisonRules, &currentChoice->comparisonRulesCount, ruleParameters);
    if (currentChoice->pGpuComparisonRules == NULL)
    {
        LOGF(eDEBUG, "Invalid GPU Selection rule: '%s'.", currentLine);
    }
    else
    {
        gGPUComparisonChoiceCount++;
    }
}

void parseDriverRejectionLine(char* currentLine)
{
    char vendorStr[MAX_GPU_VENDOR_STRING_LENGTH] = {};
    char ruleParameters[MAX_GPU_VENDOR_STRING_LENGTH] = {};
    char reasonStr[MAX_GPU_VENDOR_STRING_LENGTH] = {};

    // parse driver rule
    size_t      ruleLength = strcspn(currentLine, "#");
    const char* pLineEnd = currentLine + ruleLength;
    char*       tokens[] = { vendorStr, ruleParameters, reasonStr };
    tokenizeLine(currentLine, pLineEnd, TF_ARRAY_COUNT(tokens), tokens);
    uint32_t vendorId;
    stringToInteger(vendorStr, &vendorId, 16);
    if (isValidGPUVendorId(vendorId))
    {
        DriverRejectionRule* currentRule = &gDriverRejectionRules[gDriverRejectionRulesCount];
        *currentRule = {};
        DriverVersion* currentVersion = &currentRule->driverComparisonValue;
        currentRule->vendorId = vendorId;
        char* comparisonRule = ruleParameters;
        comparisonRule += strspn(ruleParameters, " ");
        size_t optionLength = strcspn(comparisonRule, " <=>!,");
        char   fieldName[MAX_GPU_VENDOR_STRING_LENGTH] = {};
        strncpy(fieldName, comparisonRule, optionLength);
        if (strcmp(stringToLower(fieldName), "driverversion") == 0)
        {
            comparisonRule += optionLength;
            comparisonRule += strspn(comparisonRule, " ");
            optionLength = strspn(comparisonRule, "<=>!");
            strncpy(currentRule->comparator, comparisonRule, TF_MIN(optionLength, 3));
            comparisonRule += optionLength;
            comparisonRule += strspn(comparisonRule, " ");
            bool validConversion = parseDriverVersion(comparisonRule, currentVersion);
            // optional, could add check on comparator validity
            if (validConversion)
            {
                strncpy(currentRule->reasonStr, reasonStr, MAX_GPU_VENDOR_STRING_LENGTH);
                gDriverRejectionRulesCount++;
            }
            else
            {
                LOGF(eDEBUG, "Driver Rejection| error parsing this line: '%s', invalid version number", currentLine);
            }
        }
        else
        {
            LOGF(eDEBUG, "Driver Rejection| missing DriverVersion literal in '%s'", currentLine);
        }
    }
    else
    {
        LOGF(eDEBUG, "Driver Rejection| unknown gpu vendor %s in '%s'", vendorStr, currentLine);
    }
}

void parseConfigurationSettingLine(char* currentLine)
{
    char propertyName[MAX_GPU_VENDOR_STRING_LENGTH] = {};
    char ruleParameters[MAX_GPU_VENDOR_STRING_LENGTH] = {};
    char assignmentValue[MAX_GPU_VENDOR_STRING_LENGTH] = {};

    // parse selection rule
    size_t                ruleLength = strcspn(currentLine, "#");
    const char*           pLineEnd = currentLine + ruleLength;
    ConfigurationSetting* currentSettings = &gConfigurationSettings[gConfigurationSettingsCount];
    *currentSettings = {};
    char* tokens[] = { propertyName, ruleParameters, assignmentValue };
    tokenizeLine(currentLine, pLineEnd, TF_ARRAY_COUNT(tokens), tokens);
    char* rulesBegin = ruleParameters;
    currentSettings->pUpdateProperty = propertyNameToGpuProperty(stringToLower(propertyName));
    if (currentSettings->pUpdateProperty)
    {
        // check assignment value
        int  base = ((strcmp(currentSettings->pUpdateProperty->name, "vendorid") == 0) ||
                    (strcmp(currentSettings->pUpdateProperty->name, "deviceid") == 0))
                        ? 16
                        : 10;
        bool validConversion = stringToLargeInteger(assignmentValue, &currentSettings->assignmentValue, base);
        // parse comparison rules separated by ","
        parseConfigurationRules(&currentSettings->pConfigurationRules, &currentSettings->comparisonRulesCount, rulesBegin);
        if (currentSettings->pConfigurationRules == NULL)
        {
            LOGF(eDEBUG, "parseGPUConfigurationSetting: invalid rules for Field name: '%s'.", currentSettings->pUpdateProperty->name);
            tf_free(currentSettings->pConfigurationRules);
        }
        else if (!validConversion)
        {
            LOGF(eDEBUG, "parseGPUConfigurationSetting: cannot convert %s for Field name: '%s'.", assignmentValue,
                 currentSettings->pUpdateProperty->name);
            tf_free(currentSettings->pConfigurationRules);
        }
        else
        {
            gConfigurationSettingsCount++;
        }
    }
    else
    {
        LOGF(eDEBUG, "parseGPUConfigurationSetting: invalid property: '%s'.", propertyName);
        tf_free(currentSettings->pConfigurationRules);
    }
}

void parseUserExtendedSettingLine(char* currentLine, ExtendedSettings* pExtendedSettings)
{
    char settingName[MAX_GPU_VENDOR_STRING_LENGTH] = {};
    char ruleParameters[MAX_GPU_VENDOR_STRING_LENGTH] = {};
    char assignmentValue[MAX_GPU_VENDOR_STRING_LENGTH] = {};

    // parse selection rule
    size_t      ruleLength = strcspn(currentLine, "#");
    const char* pLineEnd = currentLine + ruleLength;
    char*       tokens[] = { settingName, ruleParameters, assignmentValue };
    tokenizeLine(currentLine, pLineEnd, TF_ARRAY_COUNT(tokens), tokens);

    uint32_t settingIndex = getSettingIndex(settingName, pExtendedSettings->mNumSettings, pExtendedSettings->ppSettingNames);
    if (settingIndex != INVALID_OPTION)
    {
        char*        rulesBegin = ruleParameters;
        UserSetting* currentSetting = &gUserSettings[gUserExtendedSettingsCount];
        *currentSetting = {};
        currentSetting->name = pExtendedSettings->ppSettingNames[settingIndex];
        currentSetting->pSettingValue = &pExtendedSettings->pSettings[settingIndex];
        parseConfigurationRules(&currentSetting->pConfigurationRules, &currentSetting->comparisonRulesCount, rulesBegin);
        bool validConversion = stringToInteger(assignmentValue, &currentSetting->assignmentValue, 10);
        if (!validConversion)
        {
            LOGF(eDEBUG, "parseUserSettings: cannot convert %s for setting name: '%s'.", assignmentValue, currentSetting->name);
            tf_free(currentSetting->pConfigurationRules);
        }
        else
        {
            gUserExtendedSettingsCount++;
        }
    }
    else
    {
        LOGF(eDEBUG, "User Settings| Unknown ExtendedSetting: '%s' from line: '%s'", settingName, currentLine);
    }
}

void parseConfigurationRules(ConfigurationRule** ppConfigurationRules, uint32_t* pRulesCount, char* ruleStr)
{
    char* rulesEnd = ruleStr + strlen(ruleStr);
    while (ruleStr < rulesEnd)
    {
        (*pRulesCount)++;
        char* currentRule = ruleStr;
        *ppConfigurationRules = (ConfigurationRule*)tf_realloc(*ppConfigurationRules, *pRulesCount * sizeof(ConfigurationRule));
        ConfigurationRule* currentComparisonRule = &((*ppConfigurationRules)[(*pRulesCount) - 1]);
        *currentComparisonRule = {};

        //  read in the field name
        size_t optionLength = strcspn(currentRule, " <=>!,");
        ASSERT(optionLength < MAX_GPU_VENDOR_STRING_LENGTH - 1);
        char fieldName[MAX_GPU_VENDOR_STRING_LENGTH] = {};
        strncpy(fieldName, currentRule, optionLength);
        currentComparisonRule->pGpuProperty = propertyNameToGpuProperty(stringToLower(fieldName));

        currentRule += optionLength;
        currentRule += strspn(currentRule, " ");
        optionLength = strspn(currentRule, "<=>!");

        // this will be triggered for any platform specific rule, isheadless, graphicqueuesupported, ...
        if (currentComparisonRule->pGpuProperty == NULL)
        {
            // discard current set of rules
            LOGF(eDEBUG, "Extended Config| GPU Config invalid Field name: '%s'.", fieldName);
            tf_free(*ppConfigurationRules);
            *ppConfigurationRules = NULL;
            *pRulesCount = 0;
            break;
        }
        // store the comparator and comparatorValue if any
        else if (optionLength > 0)
        {
            ASSERT(optionLength <= 3 && optionLength > 0);
            // optional, could add check on comparator validity
            strncpy(currentComparisonRule->comparator, currentRule, TF_MIN(optionLength, 3));
            //  read in the value relative to the field
            currentRule += optionLength;
            currentRule += strspn(currentRule, " ");
            char parsedValue[MAX_GPU_VENDOR_STRING_LENGTH];
            optionLength = strcspn(currentRule, ",");
            strncpy(parsedValue, currentRule, optionLength);
            parsedValue[optionLength] = '\0';
            // hack for preferred gpu
            if (strstr(stringToLower(parsedValue), "preferredgpu") != 0)
            {
                currentComparisonRule->comparatorValue = gPlatformParameters.mPreferedGpuId;
            }
            else
            {
                int  base = ((strcmp(currentComparisonRule->pGpuProperty->name, "vendorid") == 0) ||
                            (strcmp(currentComparisonRule->pGpuProperty->name, "deviceid") == 0))
                                ? 16
                                : 10;
                bool validConversion = stringToLargeInteger(parsedValue, &currentComparisonRule->comparatorValue, base);
                if (!validConversion)
                {
                    // discard current set of rules
                    LOGF(eDEBUG, "Extended Config| GPU Config invalid comparison value: '%s' in rule: %s", parsedValue, ruleStr);
                    tf_free(*ppConfigurationRules);
                    *ppConfigurationRules = NULL;
                    *pRulesCount = 0;
                    break;
                }
            }
            currentRule += optionLength;
        }
        // skip spaces and comma,
        currentRule += strspn(currentRule, " <=>!,");
        ruleStr = currentRule;
    }
}

void printConfigureRules(ConfigurationRule* pRules, uint32_t rulesCount, char* result)
{
    for (uint32_t ruleIndex = 0; ruleIndex < rulesCount; ruleIndex++)
    {
        char               ruleStr[128] = {};
        ConfigurationRule* currentRule = &pRules[ruleIndex];
        if (currentRule->comparatorValue != INVALID_OPTION)
        {
            snprintf(ruleStr, 128, "%s %s %u", currentRule->pGpuProperty->name, currentRule->comparator,
                     uint32_t(currentRule->comparatorValue));
        }
        else
        {
            snprintf(ruleStr, 128, "%s", currentRule->pGpuProperty->name);
        }
        if (ruleIndex > 0)
        {
            strcat(result, " && ");
        }
        strcat(result, ruleStr);
    }
}

uint32_t util_select_best_gpu(GPUSettings* availableSettings, uint32_t gpuCount)
{
    uint32_t gpuIndex = gpuCount > 0 ? 0 : UINT32_MAX;

    typedef bool (*DeviceBetterFn)(GPUSettings * testSettings, GPUSettings * refSettings, GPUComparisonChoice * choices,
                                   uint32_t choicesCount);
    DeviceBetterFn isDeviceBetterThan = [](GPUSettings* testSettings, GPUSettings* refSettings, GPUComparisonChoice* choices,
                                           uint32_t choicesCount) -> bool
    {
        for (uint32_t choiceIndex = 0; choiceIndex < choicesCount; choiceIndex++)
        {
            GPUComparisonChoice* currentGPUChoice = &choices[choiceIndex];
            for (uint32_t ruleIndex = 0; ruleIndex < currentGPUChoice->comparisonRulesCount; ruleIndex++)
            {
                ConfigurationRule* currentRule = &currentGPUChoice->pGpuComparisonRules[ruleIndex];
                if (currentRule != NULL)
                {
                    bool     refPass = true;
                    bool     testPass = true;
                    uint64_t refValue = currentRule->pGpuProperty->getter(refSettings);
                    uint64_t testValue = currentRule->pGpuProperty->getter(testSettings);
                    if (refValue != INVALID_OPTION && testValue != INVALID_OPTION)
                    {
                        if (currentRule->comparatorValue != INVALID_OPTION)
                        {
                            refPass &= compare(currentRule->comparator, refValue, currentRule->comparatorValue);
                            testPass &= compare(currentRule->comparator, testValue, currentRule->comparatorValue);
                        }
                        else
                        {
                            testPass &= testValue >= refValue;
                            refPass &= refValue >= testValue;
                        }
                    }

                    if (testPass != refPass)
                    {
                        // log rule selection
                        GPUSettings* chosenSettings = testPass ? testSettings : refSettings;
                        GPUSettings* nonChosenSettings = testPass ? refSettings : testSettings;
                        uint64_t     chosenValue = testPass ? testValue : refValue;
                        uint64_t     nonChosenValue = testPass ? refValue : testValue;
                        LOGF(eINFO, "Choosing GPU: %s", chosenSettings->mGpuVendorPreset.mGpuName);
                        if (currentRule->comparatorValue != INVALID_OPTION)
                        {
                            LOGF(eINFO, "%s::%s: %llu %s %llu return true", chosenSettings->mGpuVendorPreset.mGpuName,
                                 currentRule->pGpuProperty->name, chosenValue, currentRule->comparator, currentRule->comparatorValue);
                            LOGF(eINFO, "%s::%s: %llu %s %llu return false", nonChosenSettings->mGpuVendorPreset.mGpuName,
                                 currentRule->pGpuProperty->name, nonChosenValue, currentRule->comparator, currentRule->comparatorValue);
                        }
                        else
                        {
                            LOGF(eINFO, "%s::%s: %llu >= %s::%s: %llu return true", chosenSettings->mGpuVendorPreset.mGpuName,
                                 currentRule->pGpuProperty->name, chosenValue, nonChosenSettings->mGpuVendorPreset.mGpuName,
                                 currentRule->pGpuProperty->name, nonChosenValue);
                        }
                        // return if test is better than ref
                        return testPass && !refPass;
                    }
                }
            }
        }

        return false;
    };

    // perform gpu selection based on gpu.cfg rules
    for (uint32_t i = 1; i < gpuCount; ++i)
    {
        if (isDeviceBetterThan(&availableSettings[i], &availableSettings[gpuIndex], gGPUComparisonChoices, gGPUComparisonChoiceCount))
        {
            gpuIndex = i;
        }
    }

    // if there are no rules, we select the preffered gpu
    if (gGPUComparisonChoiceCount == 0)
    {
        for (uint32_t i = 0; i < gpuCount; ++i)
        {
            if (availableSettings[i].mGpuVendorPreset.mModelId == gPlatformParameters.mPreferedGpuId)
            {
                gpuIndex = i;
                break;
            }
        }
    }

    // Last hard coded rule checking, should we get rid of this one ?
    if (!availableSettings[gpuIndex].mGraphicsQueueSupported)
    {
        gpuIndex = UINT32_MAX;
    }

    return gpuIndex;
}

void applyGPUConfigurationRules(GPUSettings* pGpuSettings, GPUCapBits* pCapBits)
{
    for (uint32_t i = 0; i < gConfigurationSettingsCount; i++)
    {
        ConfigurationSetting* currentSetting = &gConfigurationSettings[i];
        bool                  hasValidatedComparisonRules = true;
        for (uint32_t j = 0; j < currentSetting->comparisonRulesCount; j++)
        {
            ConfigurationRule* currentRule = currentSetting->pConfigurationRules;
            uint64_t           refValue = currentRule->pGpuProperty->getter(pGpuSettings);
            if (currentRule->comparatorValue != INVALID_OPTION)
            {
                hasValidatedComparisonRules &= compare(currentRule->comparator, refValue, currentRule->comparatorValue);
            }
            else
            {
                hasValidatedComparisonRules &= (refValue > 0);
            }
        }

        if (hasValidatedComparisonRules)
        {
            LOGF(eINFO, "GPU: %s, setting %s to %llu", pGpuSettings->mGpuVendorPreset.mGpuName, currentSetting->pUpdateProperty->name,
                 currentSetting->assignmentValue);
            currentSetting->pUpdateProperty->setter(pGpuSettings, currentSetting->assignmentValue);
        }
    }
}

void setupExtendedSettings(ExtendedSettings* pExtendedSettings, const GPUSettings* pGpuSettings)
{
    ASSERT(pExtendedSettings && pExtendedSettings->pSettings);

    // apply rules to ExtendedSettings
    for (uint32_t i = 0; i < gUserExtendedSettingsCount; i++)
    {
        UserSetting* currentSetting = &gUserSettings[i];
        bool         hasValidatedComparisonRules = true;
        for (uint32_t j = 0; j < currentSetting->comparisonRulesCount; j++)
        {
            ConfigurationRule* currentRule = currentSetting->pConfigurationRules;
            uint64_t           refValue = currentRule->pGpuProperty->getter(pGpuSettings);
            if (currentRule->comparatorValue != INVALID_OPTION)
            {
                hasValidatedComparisonRules &= compare(currentRule->comparator, refValue, currentRule->comparatorValue);
            }
            else
            {
                hasValidatedComparisonRules &= (refValue > 0);
            }
        }

        if (hasValidatedComparisonRules)
        {
            LOGF(eINFO, "Extended setting: setting %s to %u", currentSetting->name, currentSetting->assignmentValue);
            *currentSetting->pSettingValue = currentSetting->assignmentValue;
        }
    }
}

FORGE_API bool checkDriverRejectionSettings(const GPUSettings* pGpuSettings)
{
    DriverVersion driverVersion = {};
    bool          hasValidDriverStr = parseDriverVersion(pGpuSettings->mGpuVendorPreset.mGpuDriverVersion, &driverVersion);
    if (hasValidDriverStr)
    {
        for (uint32_t i = 0; i < gDriverRejectionRulesCount; i++)
        {
            if (pGpuSettings->mGpuVendorPreset.mVendorId == gDriverRejectionRules[i].vendorId)
            {
                DriverVersion* comparisonVersion = &gDriverRejectionRules[i].driverComparisonValue;
                uint32_t       tokenLength = TF_MAX(comparisonVersion->versionNumbersCount, driverVersion.versionNumbersCount);
                bool           shouldCheckEqualityFirst = strcmp(gDriverRejectionRules[i].comparator, "<=") == 0 ||
                                                strcmp(gDriverRejectionRules[i].comparator, ">=") == 0 ||
                                                strcmp(gDriverRejectionRules[i].comparator, "==") == 0;
                // first check for equality 30.0.12 <= 30.0.12.0
                if (shouldCheckEqualityFirst)
                {
                    bool isEqual = true;
                    for (uint32_t j = 0; j < tokenLength; j++)
                    {
                        isEqual &= driverVersion.versionNumbers[j] == comparisonVersion->versionNumbers[j];
                    }

                    if (isEqual)
                    {
                        LOGF(eINFO, "Driver rejection: %s %s %u.%u.%u.%u", pGpuSettings->mGpuVendorPreset.mGpuDriverVersion,
                             gDriverRejectionRules[i].comparator, comparisonVersion->versionNumbers[0],
                             comparisonVersion->versionNumbers[1], comparisonVersion->versionNumbers[2],
                             comparisonVersion->versionNumbers[3]);
                        LOGF(eINFO, "Driver rejection reason: %s ", gDriverRejectionRules[i].reasonStr);
                        return false;
                    }
                }

                // then return after the first non equal value 30.2.12 < 30.3.12.3
                for (uint32_t j = 0; j < comparisonVersion->versionNumbersCount; j++)
                {
                    if (driverVersion.versionNumbers[j] != comparisonVersion->versionNumbers[j])
                    {
                        bool shouldBeRejected = compare(gDriverRejectionRules[i].comparator, driverVersion.versionNumbers[j],
                                                        comparisonVersion->versionNumbers[j]);
                        if (shouldBeRejected)
                        {
                            LOGF(eINFO, "Driver rejection: %s %s %u.%u.%u.%u", pGpuSettings->mGpuVendorPreset.mGpuDriverVersion,
                                 gDriverRejectionRules[i].comparator, comparisonVersion->versionNumbers[0],
                                 comparisonVersion->versionNumbers[1], comparisonVersion->versionNumbers[2],
                                 comparisonVersion->versionNumbers[3]);
                            LOGF(eINFO, "Driver rejection reason: %s ", gDriverRejectionRules[i].reasonStr);
                            return false;
                        }
                        break;
                    }
                }
            }
        }
    }
    return true;
}

GPUPresetLevel getDefaultPresetLevel() { return gDefaultPresetLevel; }

GPUPresetLevel getGPUPresetLevel(uint32_t vendorId, uint32_t modelId, const char* vendorName, const char* modelName)
{
    GPUPresetLevel presetLevel = GPU_PRESET_NONE;

    if (arrlenu(gGPUModels))
    {
        for (uint32_t gpuModelIndex = 0; gpuModelIndex < arrlenu(gGPUModels); ++gpuModelIndex)
        {
            GPUModelDefinition model = gGPUModels[gpuModelIndex];
            if (model.mVendorId == vendorId && model.mDeviceId == modelId && model.mDeviceId)
            {
                presetLevel = model.mPreset;
                break;
            }
        }
    }

#if defined(ENABLE_GRAPHICS_DEBUG)
    if (presetLevel != GPU_PRESET_NONE)
    {
        LOGF(eINFO, "Setting preset level %s for gpu vendor:%s model:%s", presetLevelToString(presetLevel), vendorName, modelName);
    }
    else
    {
        presetLevel = gDefaultPresetLevel;
        LOGF(eWARNING, "Couldn't find gpu %s model: %s in gpu.data. Setting preset to %s as a default.", vendorName, modelName,
             presetLevelToString(presetLevel));
    }
#endif

    return presetLevel;
}

const char* presetLevelToString(GPUPresetLevel preset)
{
    switch (preset)
    {
    case GPU_PRESET_NONE:
        return "";
    case GPU_PRESET_OFFICE:
        return "office";
    case GPU_PRESET_VERYLOW:
        return "verylow";
    case GPU_PRESET_LOW:
        return "low";
    case GPU_PRESET_MEDIUM:
        return "medium";
    case GPU_PRESET_HIGH:
        return "high";
    case GPU_PRESET_ULTRA:
        return "ultra";
    default:
        return NULL;
    }
}

GPUPresetLevel stringToPresetLevel(const char* presetLevel)
{
    if (!stricmp(presetLevel, "office"))
        return GPU_PRESET_OFFICE;
    if (!stricmp(presetLevel, "verylow"))
        return GPU_PRESET_VERYLOW;
    if (!stricmp(presetLevel, "low"))
        return GPU_PRESET_LOW;
    if (!stricmp(presetLevel, "medium"))
        return GPU_PRESET_MEDIUM;
    if (!stricmp(presetLevel, "high"))
        return GPU_PRESET_HIGH;
    if (!stricmp(presetLevel, "ultra"))
        return GPU_PRESET_ULTRA;

    return GPU_PRESET_NONE;
}
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
// HELPER DEFINITIONS

// --- Interpreting Helpers --- //

const GPUProperty* propertyNameToGpuProperty(const char* str)
{
    for (uint32_t i = 0; i < TF_ARRAY_COUNT(availableGpuProperties); i++)
    {
        if (strcmp(str, availableGpuProperties[i].name) == 0)
        {
            return &availableGpuProperties[i];
        }
    }
    return NULL;
}

bool isValidGPUVendorId(uint32_t vendorId)
{
    for (uint32_t i = 0; i < gGPUVendorCount; i++)
    {
        GPUVendorDefinition* currentGPUVendorDefinition = &gGPUVendorDefinitions[i];
        for (uint32_t j = 0; j < currentGPUVendorDefinition->identifierCount; j++)
            if (currentGPUVendorDefinition->identifierArray[j] == vendorId)
            {
                return true;
            }
    }
    return false;
}

const char* getGPUVendorName(uint32_t vendorId)
{
    for (uint32_t i = 0; i < gGPUVendorCount; i++)
    {
        GPUVendorDefinition* currentGPUVendorDefinition = &gGPUVendorDefinitions[i];
        for (uint32_t j = 0; j < currentGPUVendorDefinition->identifierCount; j++)
            if (currentGPUVendorDefinition->identifierArray[j] == vendorId)
            {
                return currentGPUVendorDefinition->vendorName;
            }
    }
    return "UNKNOWN_GPU_VENDOR";
}

uint32_t getGPUVendorID(const char* vendorName)
{
    for (uint32_t i = 0; i < gGPUVendorCount; ++i)
    {
        GPUVendorDefinition* currentGPUVendorDefinition = &gGPUVendorDefinitions[i];
        if (strcmp(vendorName, currentGPUVendorDefinition->vendorName) == 0)
        {
            // Return the first vendor found in the vendor list
            if (currentGPUVendorDefinition->identifierCount > 0)
            {
                return currentGPUVendorDefinition->identifierArray[0];
            }
        }
    }
    return 0;
}

bool gpuVendorEquals(uint32_t vendorId, const char* vendorName)
{
    char currentVendorNameLower[MAX_GPU_VENDOR_STRING_LENGTH] = {};
    char comparisonValueLower[MAX_GPU_VENDOR_STRING_LENGTH] = {};
    strncpy(currentVendorNameLower, getGPUVendorName(vendorId), MAX_GPU_VENDOR_STRING_LENGTH - 1);
    strncpy(comparisonValueLower, vendorName, MAX_GPU_VENDOR_STRING_LENGTH - 1);
    stringToLower(currentVendorNameLower);
    stringToLower(comparisonValueLower);
    return strcmp(currentVendorNameLower, comparisonValueLower) == 0;
}

#if defined(__APPLE__)
uint32_t getGPUModelID(const char* modelName)
{
    for (uint32_t i = 0; i < arrlenu(gGPUModels); ++i)
    {
        GPUModelDefinition* model = &gGPUModels[i];
        if (!strncmp(modelName, model->mModelName, TF_ARRAY_COUNT(model->mModelName)))
        {
            return model->mDeviceId;
        }
    }
    return UINT32_MAX;
}
#endif

bool compare(const char* cmp, uint64_t a, uint64_t b)
{
    if (strcmp(cmp, "<=") == 0)
    {
        return a <= b;
    }
    if (strcmp(cmp, "<") == 0)
    {
        return a < b;
    }
    if (strcmp(cmp, ">=") == 0)
    {
        return a >= b;
    }
    if (strcmp(cmp, ">") == 0)
    {
        return a > b;
    }
    if (strcmp(cmp, "==") == 0)
    {
        return a == b;
    }
    if (strcmp(cmp, "!=") == 0)
    {
        return a != b;
    }

    return false;
}

bool compare(const char* cmp, const char* a, const char* b)
{
    if (strcmp(cmp, "==") == 0)
    {
        return (strcmp(a, b) == 0);
    }
    if (strcmp(cmp, "!=") == 0)
    {
        return (strcmp(a, b) != 0);
    }

    return false;
}

uint32_t getSettingIndex(const char* settingName, uint32_t numSettings, const char** gameSettingNames)
{
    for (uint32_t i = 0; i < numSettings; ++i)
        if (strcmp(settingName, gameSettingNames[i]) == 0)
            return i;

    return INVALID_OPTION;
}

bool parseDriverVersion(const char* driverStr, DriverVersion* pDriverVersionOut)
{
    const char* driverStrEnd = driverStr + strlen(driverStr);
    if (driverStr[0] == '\0')
    {
        return false;
    }
    size_t tokenLength = strcspn(driverStr, " .-\r\n");
    bool   validConversion = true;
    while (driverStr != driverStrEnd)
    {
        if (pDriverVersionOut->versionNumbersCount > 3)
        {
            validConversion = false;
            break;
        }
        char currentNumber[32] = {};
        strncpy(currentNumber, driverStr, tokenLength);
        // 32 characters is large enough to store a version number, the warning can be suppressed
        validConversion &= stringToInteger(currentNumber, &pDriverVersionOut->versionNumbers[pDriverVersionOut->versionNumbersCount], 10);
        driverStr += tokenLength;
        driverStr += strspn(driverStr, " .-\r\n");
        tokenLength = strcspn(driverStr, " .-\r\n");
        pDriverVersionOut->versionNumbersCount++;
    }
    return validConversion;
}

// --- Parsing Helpers --- //
char* stringToLower(char* str)
{
    for (char* p = str; *p != '\0'; ++p)
        *p = tolower(*p);
    return str;
}

bool stringToInteger(char* str, uint32_t* pOutResult, uint32_t base)
{
    char* endConversionPtr = NULL;
    // return end of string or index of the first space encountered
    char* endStr = str + strcspn(str, " ");
    *pOutResult = (uint32_t)strtoll(str, &endConversionPtr, base);
    // check emptiness
    bool validConversion = str != endStr;
    // check if successfully parsed the full string
    validConversion &= endStr == endConversionPtr;
    return validConversion;
}

bool stringToLargeInteger(char* str, uint64_t* pOutResult, uint32_t base)
{
    char* endConversionPtr = NULL;
    // return end of string or index of the first space encountered
    char* endStr = str + strcspn(str, " ");
    *pOutResult = strtoll(str, &endConversionPtr, base);
    // check emptiness
    bool validConversion = str != endStr;
    // check if successfully parsed the full string
    validConversion &= endStr == endConversionPtr;
    return validConversion;
}

bool contains(char* str, const char* substr) { return strstr(str, substr) != nullptr; }

bool bufferedGetLine(char* lineStrOut, char** bufferCursorInOut, const char* bufferEnd)
{
    if (*bufferCursorInOut < bufferEnd)
    {
        size_t lineIndex = 0;
        // copy current line, memcpy seems to make it slower
        while (*(*bufferCursorInOut) != '\n' && *(*bufferCursorInOut) != '\r' && *bufferCursorInOut != bufferEnd)
        {
            lineStrOut[lineIndex] = *(*bufferCursorInOut);
            lineIndex++;
            (*bufferCursorInOut)++;
        }
        lineStrOut[lineIndex] = '\0';
        // skip /r/n
        if (*(*bufferCursorInOut) == '\r' && *bufferCursorInOut != bufferEnd && *((*bufferCursorInOut) + 1) == '\n')
        {
            (*bufferCursorInOut)++;
        }
        (*bufferCursorInOut)++;
        return true;
    }
    else
    {
        return false;
    }
}

void tokenizeLine(const char* pLine, const char* pLineEnd, uint32_t numTokens, char** pTokens)
{
    //  initialize all tokens to empty string
    for (uint32_t i = 0; i < numTokens; ++i)
        pTokens[i][0] = '\0';

    for (uint32_t i = 0; pLine < pLineEnd && i < numTokens; ++i)
    {
        const char* begin = pLine;
        const char* end = pLine + strcspn(pLine, ";");

        if (end > pLineEnd)
            end = pLineEnd;

        pLine = end + 1;

        //  Remove whitespace from the front and end
        while (begin != end && isspace(*begin))
            ++begin;
        while (begin != end && isspace(end[-1]))
            --end;

        ptrdiff_t length = end - begin;
        //  NOTE: the last character must be NULL, this assert makes sure the string isnt a
        //  full 256 characters, as that would leave no room for a null-terminator
        ASSERT(length < MAX_GPU_VENDOR_STRING_LENGTH - 1);

        strncpy(pTokens[i], begin, length);
        //  set token to be NUL-terminated
        pTokens[i][length] = '\0';
    }
}

GPUPresetLevel getSinglePresetLevel(const char* line, const char* inVendorName, const char* inModelName, const char* inModelId,
                                    const char* inRevId)
{
    char           vendorName[MAX_GPU_VENDOR_STRING_LENGTH] = {};
    char           deviceName[MAX_GPU_VENDOR_STRING_LENGTH] = {};
    char           deviceId[MAX_GPU_VENDOR_STRING_LENGTH] = {};
    char           revisionId[MAX_GPU_VENDOR_STRING_LENGTH] = {};
    GPUPresetLevel presetLevel = {};

    // check if current vendor line is one of the selected gpu's
    if (!parseConfigLine(line, inVendorName, inModelName, inModelId, inRevId, vendorName, deviceName, deviceId, revisionId, &presetLevel))
        return GPU_PRESET_NONE;

    return presetLevel;
}

bool parseConfigLine(const char* pLine, const char* pInVendorName, const char* pInModelName, const char* pInModelId,
                     const char* pInRevisionId, char pOutVendorName[MAX_GPU_VENDOR_STRING_LENGTH],
                     char pOutModelName[MAX_GPU_VENDOR_STRING_LENGTH], char pOutModelId[MAX_GPU_VENDOR_STRING_LENGTH],
                     char pOutRevisionId[MAX_GPU_VENDOR_STRING_LENGTH], GPUPresetLevel* pOutPresetLevel)
{
    const char* pOrigLine = pLine;
    ASSERT(pLine && pOutPresetLevel);
    ASSERT(pInVendorName);
    ASSERT(pInModelName);
    *pOutPresetLevel = GPU_PRESET_LOW;

    // exclude comments from line
    size_t      lineSize = strcspn(pLine, "#");
    const char* pLineEnd = pLine + lineSize;

    // Exclude whitespace in the beginning (for early exit)
    while (pLine != pLineEnd && isspace(*pLine))
        ++pLine;

    if (pLine == pLineEnd)
        return false;

    char presetLevel[MAX_GPU_VENDOR_STRING_LENGTH];

    char* tokens[] = {
        pOutVendorName, pOutModelName, presetLevel, pOutModelId, pOutRevisionId,
        // codename is not used
    };
    tokenizeLine(pLine, pLineEnd, TF_ARRAY_COUNT(tokens), tokens);

    // validate required fields
    if (pOutVendorName[0] == '\0' || pOutModelName[0] == '\0' || presetLevel[0] == '\0')
    {
        LOGF(eWARNING,
             "GPU config requires VendorName, DeviceName and PresetLevel. "
             "Following line has invalid format:\n'%s'",
             pOrigLine);
        return false;
    }

    // convert ids to lower case
    stringToLower(presetLevel);
    stringToLower(pOutModelId);
    stringToLower(pOutRevisionId); // RevisionID is no longer used for comparision. Just compare name and modelID if provided in gpu.data

    // Parsing logic

    *pOutPresetLevel = stringToPresetLevel(presetLevel);

    bool success = true;

    success = success && strcmp(pInVendorName, pOutVendorName) == 0;
    success = success && strcmp(pInModelName, pOutModelName) == 0;

    if (success && pInModelId && pOutModelId[0] != '\0')
    {
        if (strcmp(pInModelId, pOutModelId) != 0)
        {
            LOGF(LogLevel::eWARNING, "Entry with matching GPUName found in gpu.data, however there was a mismatch in device IDs. \
                                      Entry has ID: %s | Device has ID: %s",
                 pInModelId, pInModelName);
            success = false;
        }
    }

    return success;
}
///////////////////////////////////////////////////////////