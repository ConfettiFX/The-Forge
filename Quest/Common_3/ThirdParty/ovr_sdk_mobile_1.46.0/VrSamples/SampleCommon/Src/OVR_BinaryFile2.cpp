/************************************************************************************

Filename    :   OVR_BinaryFile2.cpp
Content     :   Simple helper class to read a binary file.
Created     :   Jun, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#include "OVR_BinaryFile2.h"
#include <fstream>

namespace OVRFW {

BinaryReader::~BinaryReader() {
    if (Allocated && nullptr != Data) {
        delete[] Data;
    }
}

BinaryReader::BinaryReader(const char* path, const char** perror)
    : Data(NULL), Size(0), Offset(0), Allocated(true) {
    std::ifstream is;
    is.open(path, std::ios::binary | std::ios::in);
    if (!is.is_open()) {
        if (perror != NULL) {
            *perror = "Failed to open file.";
        }
        return;
    }

    // get size
    is.seekg(0, is.end);
    Size = static_cast<int>(is.tellg());
    is.seekg(0, is.beg);

    // allocate buffer
    Data = (uint8_t*)new uint8_t[(Size + 1)];
    if (nullptr != Data) {
        static_assert(
            sizeof(char) == sizeof(uint8_t),
            "allocation size of char and uint8_t mismatch - BinaryReader will fail");

        // read all file
        is.read((char*)Data, Size);
        if (!is) {
            if (perror != NULL) {
                *perror = "Failed to read file.";
            }
        }
    } else {
        if (perror != NULL) {
            *perror = "Failed to allocate backing buffer.";
        }
    }

    // close
    is.close();
}

std::vector<uint8_t> MemBufferFile(const char* fileName) {
    std::vector<uint8_t> buffer;

    std::ifstream is;
    is.open(fileName, std::ios::binary | std::ios::in);
    if (is.is_open()) {
        // get size & allocate
        is.seekg(0, is.end);
        buffer.resize(static_cast<size_t>(is.tellg()));
        is.seekg(0, is.beg);

        // read all file
        is.read((char*)buffer.data(), buffer.size());

        // close
        is.close();
    }

    return buffer;
}

} // namespace OVRFW
