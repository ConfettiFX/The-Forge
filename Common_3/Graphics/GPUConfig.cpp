/*
 * Copyright (c) 2018-2022 The Forge Interactive Inc.
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

#include "GPUConfig.h"
#include "../Utilities/Interfaces/IFileSystem.h"

///////////////////////////////////////////////////////////
// HELPER DECLARATIONS
#define INVALID_OPTION  UINT_MAX

typedef enum Field
{
	Invalid = -1,
	VRAM,
	VendorID, DeviceID,
	TessellationSupport,
	ArgBufferMaxTextures,
	Metal,
	iOS, XBox
} Field;

// ------ Interpreting Helpers ------ //
Field stringToField(const char* str);
uint64_t getFieldValue(Field, GPUSettings const*);
bool compare(const char* cmp, uint64_t, uint64_t);
bool compare(const char* cmp, const char*, const char*);
uint32_t getSettingIndex(const char* string, uint32_t numSettings, const char** gameSettingNames);

// -------- Parsing Helpers ------- //
char* stringToLower(char* str);
void fsReadFromStreamLine(FileStream* stream, char* pOutLine);
void tokenizeLine(const char* pLine, const char* pLineEnd, uint32_t numTokens, char** pTokens);
GPUPresetLevel getSinglePresetLevel(const char* line, const char* inVendorId, const char* inModelId, const char* inRevId);
bool parseConfigLine(
	const char* pLine,
	const char* pInVendorId,
	const char* pInModelId,
	const char* pInModelName,
	char pOutVendorId[MAX_GPU_VENDOR_STRING_LENGTH],
	char pOutModelId[MAX_GPU_VENDOR_STRING_LENGTH],
	char pOutRevisionId[MAX_GPU_VENDOR_STRING_LENGTH],
	char pOutModelName[MAX_GPU_VENDOR_STRING_LENGTH],
	GPUPresetLevel* pOutPresetLevel);
void parseExtendedConfigLine(const char* pLine, ExtendedSettings* pExtendedSettings, GPUSettings const* pGpuSettings);
void parseOption(uint32_t* pGameSetting, char* gameSettingValues, GPUSettings const*);
void parseRule(uint32_t* pGameSetting,char* pGameSettingConstraints, uint32_t gameSettingOverride, GPUSettings const*);
///////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////
// CONFIG INTERFACE

//Reads the gpu config and sets the preset level of all available gpu's
GPUPresetLevel getGPUPresetLevel(const char* vendorId, const char* modelId, const char* revId)
{
	FileStream fh = {};
	if (!fsOpenStreamFromPath(RD_GPU_CONFIG, "gpu.cfg", FM_READ, NULL, &fh))
	{
		LOGF(LogLevel::eWARNING, "gpu.cfg could not be found, setting preset to Low as a default.");
		return GPU_PRESET_LOW;
	}

	GPUPresetLevel foundLevel = GPU_PRESET_LOW;

	char gpuCfgString[1024] = {};
	while (!fsStreamAtEnd(&fh))
	{
		fsReadFromStreamLine(&fh, gpuCfgString);

		// excluse lines that are not related to GPU presets
		size_t length = strcspn(gpuCfgString, ";");
		if (strncmp(gpuCfgString, "OPT", length) == 0 || strncmp(gpuCfgString, "RULE", length) == 0)
			continue;

		GPUPresetLevel level = getSinglePresetLevel(gpuCfgString, vendorId, modelId, revId);
		// Do something with the tok
		if (level != GPU_PRESET_NONE)
		{
			foundLevel = level;
			break;
		}
	}

	fsCloseStream(&fh);
	return foundLevel;
}

void setExtendedSettings(ExtendedSettings* pExtendedSettings, GPUSettings* pGpuSettings)
{
	FileStream fh = {};
	if (!fsOpenStreamFromPath(RD_GPU_CONFIG, "gpu.cfg", FM_READ, nullptr, &fh))
	{
	  LOGF(LogLevel::eWARNING, "gpu.cfg file could not be found, no settings loaded.");
	  return;
	}
	
	char configStr[1024] = {};
	while (!fsStreamAtEnd(&fh))
	{
	  fsReadFromStreamLine(&fh, configStr);
		// only parse lines that start with 'OPT' or 'RULE'
		size_t length = strcspn(configStr, ";");
		if (strncmp(configStr, "OPT", length) != 0 && strncmp(configStr, "RULE", length) != 0)
			continue;

		parseExtendedConfigLine(configStr, pExtendedSettings, pGpuSettings);
	}
	
	fsCloseStream(&fh);
}

const char* presetLevelToString(GPUPresetLevel preset)
{
	switch (preset)
	{
	case GPU_PRESET_NONE: return "";
	case GPU_PRESET_OFFICE: return "office";
	case GPU_PRESET_LOW: return "low";
	case GPU_PRESET_MEDIUM: return "medium";
	case GPU_PRESET_HIGH: return "high";
	case GPU_PRESET_ULTRA: return "ultra";
	default: return NULL;
	}
}

GPUPresetLevel stringToPresetLevel(const char* presetLevel)
{
	if (!stricmp(presetLevel, "office"))
		return GPU_PRESET_OFFICE;
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
Field stringToField(const char* str)
{
	if (strcmp(str, "vram") == 0)                 return Field::VRAM;
	if (strcmp(str, "vendorid") == 0)             return Field::VendorID;
	if (strcmp(str, "deviceid") == 0)             return Field::DeviceID;
	if (strcmp(str, "tessellationsupport") == 0)  return Field::TessellationSupport;
	if (strcmp(str, "argbuffermaxtextures") == 0) return Field::ArgBufferMaxTextures;
	if (strcmp(str, "metal") == 0)                return Field::Metal;
	if (strcmp(str, "ios") == 0)                  return Field::iOS;
	if (strcmp(str, "xbox") == 0)                 return Field::XBox;

	return Field::Invalid;
}

uint64_t getFieldValue(Field field, GPUSettings const* pSettings)
{
	switch (field)
	{
	case Field::VRAM:
		return pSettings->mVRAM;

	case Field::VendorID:
		return (uint32_t)strtol(pSettings->mGpuVendorPreset.mVendorId, NULL, 16);

	case Field::DeviceID:
		return (uint32_t)strtol(pSettings->mGpuVendorPreset.mModelId, NULL, 16);

	case Field::TessellationSupport:
		return pSettings->mTessellationSupported;

#ifdef METAL
	case Field::Metal:
		return 1;

	case Field::ArgBufferMaxTextures:
		return pSettings->mArgumentBufferMaxTextures;
#endif

#ifdef TARGET_IOS
	case Field::iOS:
		return 1;
#endif

#ifdef XBOX
	case Field::XBox:
		return 1;
#endif

	default:
		return INVALID_OPTION;
	}
}

bool compare(const char* cmp, uint64_t a, uint64_t b)
{
	if (strcmp(cmp, "<=") == 0) { return a <= b; }
	if (strcmp(cmp, "<") == 0) { return a < b; }
	if (strcmp(cmp, ">=") == 0) { return a >= b; }
	if (strcmp(cmp, ">") == 0) { return a > b; }
	if (strcmp(cmp, "==") == 0) { return a == b; }
	if (strcmp(cmp, "!=") == 0) { return a != b; }

	return false;
}

bool compare(const char* cmp, const char* a, const char* b)
{
	if (strcmp(cmp, "==") == 0) { return (strcmp(a, b) == 0); }
	if (strcmp(cmp, "!=") == 0) { return (strcmp(a, b) != 0); }

	return false;
}

uint32_t getSettingIndex(const char* settingName, uint32_t numSettings, const char** gameSettingNames)
{
	for (uint32_t i = 0; i < numSettings; ++i)
		if (strcmp(settingName, gameSettingNames[i]) == 0)
			return i;

	return INVALID_OPTION;
}

// --- Parsing Helpers --- //
char* stringToLower(char* str)
{
	for (char* p = str; *p != '\0'; ++p)
		*p = tolower(*p);
	return str;
}

void fsReadFromStreamLine(FileStream* stream, char* pOutLine)
{
	uint32_t charIndex = 0;
	while (!fsStreamAtEnd(stream))
	{
		char nextChar = 0;
		fsReadFromStream(stream, &nextChar, sizeof(nextChar));
		if (nextChar == 0 || nextChar == '\n')
		{
			break;
		}
		if (nextChar == '\r')
		{
			char newLine = 0;
			fsReadFromStream(stream, &newLine, sizeof(newLine));
			if (newLine == '\n')
			{
				break;
			}
			else
			{
				// We're not looking at a "\r\n" sequence, so add the '\r' to the buffer.
				fsSeekStream(stream, SBO_CURRENT_POSITION, -1);
			}
		}
		pOutLine[charIndex++] = nextChar;
	}

	pOutLine[charIndex] = 0;
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

		if (end > pLineEnd) end = pLineEnd;

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

GPUPresetLevel getSinglePresetLevel(const char* line, const char* inVendorId, const char* inModelId, const char* inRevId)
{
	char vendorId[MAX_GPU_VENDOR_STRING_LENGTH] = {};
	char deviceId[MAX_GPU_VENDOR_STRING_LENGTH] = {};
	GPUPresetLevel presetLevel = {};
	char gpuName[MAX_GPU_VENDOR_STRING_LENGTH] = {};
	char revisionId[MAX_GPU_VENDOR_STRING_LENGTH] = {};

	//check if current vendor line is one of the selected gpu's
	if (!parseConfigLine(line, inVendorId, inModelId, NULL, vendorId, deviceId, revisionId, gpuName, &presetLevel))
		return GPU_PRESET_NONE;

	//if we have a revision Id then we want to match it as well
	if (stricmp(inRevId, "0x00") != 0 && strlen(revisionId) && stricmp(revisionId, "0x00") != 0 && stricmp(inRevId, revisionId) != 0)
		return GPU_PRESET_NONE;

	return presetLevel;
}

bool parseConfigLine(
	const char* pLine,
	const char* pInVendorId,
	const char* pInModelId,
	const char* pInModelName,
	char pOutVendorId[MAX_GPU_VENDOR_STRING_LENGTH],
	char pOutModelId[MAX_GPU_VENDOR_STRING_LENGTH],
	char pOutRevisionId[MAX_GPU_VENDOR_STRING_LENGTH],
	char pOutModelName[MAX_GPU_VENDOR_STRING_LENGTH],
	GPUPresetLevel* pOutPresetLevel)
{
	const char* pOrigLine = pLine;
	ASSERT(pLine && pOutPresetLevel);
	*pOutPresetLevel = GPU_PRESET_LOW;

	// exclude comments from line
	size_t lineSize = strcspn(pLine, "#");
	const char* pLineEnd = pLine + lineSize;

	// Exclude whitespace in the beginning (for early exit)
	while (pLine != pLineEnd && isspace(*pLine))
		++pLine;

	if (pLine == pLineEnd)
		return false;

	char presetLevel[MAX_GPU_VENDOR_STRING_LENGTH];

	char* tokens[] = {
		pOutVendorId,
		pOutModelId,
		presetLevel,
		pOutModelName,
		pOutRevisionId,
		// codename is not used
	};

	uint32_t numTokens = sizeof(tokens) / sizeof(tokens[0]);
	tokenizeLine(pLine, pLineEnd, numTokens, tokens);

	// validate required fields
	if (pOutVendorId[0] == '\0' ||
		pOutModelId[0] == '\0' ||
		presetLevel[0] == '\0' ||
		pOutModelName[0] == '\0')
	{
		LOGF(eWARNING, "GPU config requires VendorId, DeviceId, Classification and Name. "
			"Following line has invalid format:\n'%s'", pOrigLine);
		return false;
	}

	// convert ids to lower case
	stringToLower(pOutVendorId);
	stringToLower(pOutModelId);
	stringToLower(presetLevel);

	// Parsing logic

	*pOutPresetLevel = stringToPresetLevel(presetLevel);

	bool success = true;

	if (pInVendorId)
		success = success && strcmp(pInVendorId, pOutVendorId) == 0;

	if (pInModelId)
		success = success && strcmp(pInModelId, pOutModelId) == 0;

	if (pInModelName)
		success = success && strcmp(pInModelName, pOutModelName) == 0;

	return success;
}

void parseExtendedConfigLine(
  const char*					pLine, 
	ExtendedSettings*   pExtendedSettings,
  GPUSettings const*	pGpuSettings)
{
  const char* origLine = pLine;
	ASSERT(pExtendedSettings && pExtendedSettings->pSettings);

	// exclude comments from the line
  size_t lineSize = strcspn(pLine, "#"); 
  const char* pLineEnd = pLine + lineSize;

	while (pLine != pLineEnd && isspace(*pLine))
		++pLine;
  if (pLine == pLineEnd) return;

  char action[MAX_GPU_VENDOR_STRING_LENGTH];
  char presetName[MAX_GPU_VENDOR_STRING_LENGTH];
  char presetValues[MAX_GPU_VENDOR_STRING_LENGTH];
  char presetOverride[MAX_GPU_VENDOR_STRING_LENGTH];

  char* tokens[] =
  {
    action,
    presetName,
    presetValues,
    presetOverride // null on PRESET lines
  };
  uint32_t numTokens = sizeof(tokens) / sizeof(tokens[0]);
  tokenizeLine(pLine, pLineEnd, numTokens, tokens);

  uint32_t index = getSettingIndex(presetName, pExtendedSettings->numSettings, pExtendedSettings->ppSettingNames);
  if (index == INVALID_OPTION || index >= pExtendedSettings->numSettings)
  {
    LOGF(eDEBUG, "Extended Config| Setting name ignored: '%s' from line: '%s'", presetName, origLine);
    return;
  }

  uint32_t* pRenderSetting = &pExtendedSettings->pSettings[index];

  if (strcmp(action, "OPT") == 0)
  {
    parseOption(pRenderSetting, presetValues, pGpuSettings);
  }
  else if (strcmp(action, "RULE") == 0)
  {
    parseRule(pRenderSetting, presetValues, atoi(presetOverride), pGpuSettings);
  }
  else
  {
    LOGF(eDEBUG, "Extended Config| Unknown action requested: '%s'", action);
  }
}

void parseOption(
	uint32_t*						pRenderSetting,
	char*								gameSettingValues,
	GPUSettings const*	pGpuSettings)
{
  GPUPresetLevel currentPresetLevel = pGpuSettings->mGpuVendorPreset.mPresetLevel;

  char* pLine = gameSettingValues;
  char* pLineEnd = pLine + strlen(pLine);

  while (pLine < pLineEnd)  // Each iteration reads in a preset level and a value
  {
    char* begin = pLine;

    size_t length = strcspn(begin, " ,");
    ASSERT(length < MAX_GPU_VENDOR_STRING_LENGTH - 1);

    char presetLevel[MAX_GPU_VENDOR_STRING_LENGTH];
    strncpy(presetLevel, begin, length);
    presetLevel[length] = '\0';
    GPUPresetLevel parsedPresetLevel = stringToPresetLevel(stringToLower(presetLevel));

    //  skip past the preset level, read in the preset value
    begin += length + strspn(begin + length, " ,");
    //  NOTE: if the preset value is omitted this is undefined behavior. 
    //  There is no way to detect errnous state; strtol is suggested for detecting such errors.
    uint32_t value = atoi(begin);

    //  skip past the preset value and delimiter
    pLine = begin + strspn(begin, " ,0123456789");

    if (parsedPresetLevel >= GPU_PRESET_LOW && currentPresetLevel >= parsedPresetLevel)
    {
      *pRenderSetting = value;
    }
  }
}

void parseRule(
  uint32_t *          pGameSetting, 
  char *              pGameSettingConstraints, 
  uint32_t            gameSettingOverride, 
	GPUSettings const*	pGpuSettings)
{
  char* pLine = pGameSettingConstraints;
  char* pLineEnd = pLine + strlen(pLine);

  while (pLine < pLineEnd)
  {
    char* begin = pLine;

    //  read in the field name
    size_t length = strcspn(begin, " <=>!");
    ASSERT(length < MAX_GPU_VENDOR_STRING_LENGTH - 1);

    char fieldName[MAX_GPU_VENDOR_STRING_LENGTH];
    strncpy(fieldName, begin, length);
    fieldName[length] = '\0';

    begin += length + strspn(begin + length, " ,");
    //  read in the comparator
    length = strspn(begin, "<=>!");
    char comparator[3] = { 0 }; // support <, <=, >, >=, ==, !=, and a NUL terminator
    strncpy(comparator, begin, length);

    begin += strspn(begin, " <=>!");
    //  read in the value relative to the field
    char parsedValue[MAX_GPU_VENDOR_STRING_LENGTH];
    length = strcspn(begin, ",");
    strncpy(parsedValue, begin, length);
    parsedValue[length] = '\0';

    begin += length;
    pLine = begin + strspn(begin, " ,");
	
		Field field = stringToField(stringToLower(fieldName));
		uint64_t fieldValue = getFieldValue(field, pGpuSettings);
		if (fieldValue == INVALID_OPTION)
		{
			LOGF(eDEBUG, "Extended Config| Field not used: '%s'.", fieldName);
			return;
		}

		//  Interpret hexadecimal fields as hex, and all others as decimal
		int base = (field == VendorID || field == DeviceID) ? 16 : 10;
		//  if the value read in fails the comparison check, this rule does not apply
		if (!compare(comparator, fieldValue, (uint64_t)strtoll(parsedValue, NULL, base)))
			return;
  }

  //  if all constraints pass, override the preset value
  *pGameSetting = gameSettingOverride;
}
///////////////////////////////////////////////////////////
