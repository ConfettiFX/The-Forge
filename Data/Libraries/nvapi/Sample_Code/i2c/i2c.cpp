/************************************************************************************************************************************\
|*                                                                                                                                    *|
|*     Copyright © 2012 NVIDIA Corporation.  All rights reserved.                                                                     *|
|*                                                                                                                                    *|
|*  NOTICE TO USER:                                                                                                                   *|
|*                                                                                                                                    *|
|*  This software is subject to NVIDIA ownership rights under U.S. and international Copyright laws.                                  *|
|*                                                                                                                                    *|
|*  This software and the information contained herein are PROPRIETARY and CONFIDENTIAL to NVIDIA                                     *|
|*  and are being provided solely under the terms and conditions of an NVIDIA software license agreement                              *|
|*  and / or non-disclosure agreement.  Otherwise, you have no rights to use or access this software in any manner.                   *|
|*                                                                                                                                    *|
|*  If not covered by the applicable NVIDIA software license agreement:                                                               *|
|*  NVIDIA MAKES NO REPRESENTATION ABOUT THE SUITABILITY OF THIS SOFTWARE FOR ANY PURPOSE.                                            *|
|*  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.                                                           *|
|*  NVIDIA DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,                                                                     *|
|*  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.                       *|
|*  IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES,                               *|
|*  OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,  WHETHER IN AN ACTION OF CONTRACT,                         *|
|*  NEGLIGENCE OR OTHER TORTIOUS ACTION,  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOURCE CODE.            *|
|*                                                                                                                                    *|
|*  U.S. Government End Users.                                                                                                        *|
|*  This software is a "commercial item" as that term is defined at 48 C.F.R. 2.101 (OCT 1995),                                       *|
|*  consisting  of "commercial computer  software"  and "commercial computer software documentation"                                  *|
|*  as such terms are  used in 48 C.F.R. 12.212 (SEPT 1995) and is provided to the U.S. Government only as a commercial end item.     *|
|*  Consistent with 48 C.F.R.12.212 and 48 C.F.R. 227.7202-1 through 227.7202-4 (JUNE 1995),                                          *|
|*  all U.S. Government End Users acquire the software with only those rights set forth herein.                                       *|
|*                                                                                                                                    *|
|*  Any use of this software in individual and commercial software must include,                                                      *|
|*  in the user documentation and internal comments to the code,                                                                      *|
|*  the above Disclaimer (as applicable) and U.S. Government End Users Notice.                                                        *|
|*                                                                                                                                    *|
\************************************************************************************************************************************/
////////////////////////////////////////////////////////////////////////////////////
// @brief: This sample changes the brightness of a monitor over the i2c bus.
//		   It checks whether the monitor connected to the system is
//		   DDC/CI compatible or not. On success, it shall reduce its brightness to 20
//		   and after a wait of 5 seconds, restores the value to
//		   its original
//
// @assumptions: This code is designed for WinVista+.It assumes that the system has
//				 atleast one active display.This sample demonstrates the use of the
//				 i2c APIs for read and write.
//
// @driver support: R304+
////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <windows.h>
#include <tchar.h>
#include "nvapi.h"
#include "targetver.h"

const int DELAY_IN_SEC = 5; // Delay between brightness changes
const int BRIGHTNESS_TEST_VALUE = 20; // Test value to set brightness to

BOOL ReadAndChangeBrightness(NvPhysicalGpuHandle hPhysicalGpu, NvU32 displayId, bool bSimulateBarcoTest);
BOOL CheckForDDCCICompliance(NvPhysicalGpuHandle hPhysicalGpu, NvU32 displayId);

int main()
{
    NvAPI_Status nvapiStatus = NVAPI_OK;

    // Initialize NVAPI.
    if ((nvapiStatus = NvAPI_Initialize()) != NVAPI_OK )
    {
        printf("NvAPI_Initialize() failed with status %d\n", nvapiStatus);
        printf("\n");
        printf("I2C Read/Write brightness test FAILED");
        return 1;
    }
    //
    // Enumerate display handles
    //
    NvDisplayHandle hDisplay_a[NVAPI_MAX_PHYSICAL_GPUS * NVAPI_MAX_DISPLAY_HEADS] = {0};
    NvU32 nvDisplayCount = 0;
    for (unsigned int i = 0; nvapiStatus == NVAPI_OK; i++)
    {
        nvapiStatus = NvAPI_EnumNvidiaDisplayHandle(i, &hDisplay_a[i]);

        if (nvapiStatus == NVAPI_OK)
        {
            nvDisplayCount++;
        }
        else if (nvapiStatus != NVAPI_END_ENUMERATION)
        {
            printf("NvAPI_EnumNvidiaDisplayHandle() failed with status %d\n", nvapiStatus);
            printf("\n");
            printf("I2C Read/Write brightness test FAILED");
            return 1;
        }
    }

    printf("No of displays = %u\n", nvDisplayCount);
    printf("Display handles: ");
    for (unsigned int i = 0; i < nvDisplayCount; i++)
    {
        printf(" %08p", (void*)hDisplay_a[i]);
    }
    printf("\n");

    //
    // Enumerate physical GPU handles
    //
    NvPhysicalGpuHandle hGPU_a[NVAPI_MAX_PHYSICAL_GPUS] = {0}; // handle to GPUs
    NvU32 gpuCount = 0;
    nvapiStatus = NvAPI_EnumPhysicalGPUs(hGPU_a, &gpuCount);
    if (nvapiStatus != NVAPI_OK)
    {
        printf("NvAPI_EnumPhysicalGPUs() failed with status %d\n", nvapiStatus);
        printf("\n");
        printf("I2C Read/Write brightness test FAILED");
        return 1;
    }
    printf("Total number of GPU's = %u\n", gpuCount);

    BOOL isTestPassed = TRUE;

    //
    // Cycle through all attached displays to read & change brightness
    //
    for (unsigned int i = 0; i < nvDisplayCount; i++)
    {
        // Get GPU id assiciated with display ID
        NvPhysicalGpuHandle hGpu = NULL;
		NvU32 pGpuCount=0;
        nvapiStatus = NvAPI_GetPhysicalGPUsFromDisplay(hDisplay_a[i], &hGpu, &pGpuCount);
        if (nvapiStatus != NVAPI_OK)
        {
            printf("NvAPI_GetPhysicalGPUFromDisplay() failed with status %d\n", nvapiStatus);
            isTestPassed = FALSE;
            break;
        }

        // Get the display id for subsequent I2C calls via NVAPI:
        NvU32 outputID = 0;
        nvapiStatus=NvAPI_GetAssociatedDisplayOutputId(hDisplay_a[i], &outputID);
        if (nvapiStatus != NVAPI_OK)
        {
            printf("NvAPI_GetAssociatedDisplayOutputId() failed with status %d\n", nvapiStatus);
            isTestPassed = FALSE;
            break;
        }

        printf
            (
                "Testing GPU handle=%08p, Output ID=%d, Display no=%d, Display handle=%08p...\n", 
                hGpu, outputID, i, hDisplay_a[i]
            );

        printf( "- Regular I2C read operation test mode\n" );

        BOOL result = CheckForDDCCICompliance(hGpu, outputID);
        if (!result)
        {
            printf("  The display 0x%X is not DDC/CI capable so skipping the test \n \n", outputID);
            printf("  The Failure is expected on notebooks \n");
            return FALSE;
        }
        
        result = ReadAndChangeBrightness(hGpu, outputID, TRUE);
        if (!result)
        {
            printf("...test FAILED!\n");
            isTestPassed = FALSE;
        }
        printf("\n");

        printf( "- Barco DDC test I2C read operation test mode\n" );
        result = ReadAndChangeBrightness(hGpu, outputID, FALSE);
        if (!result)
        {
            printf("...test FAILED!\n");
            isTestPassed = FALSE;
        }
        printf("\n");
    } 

    printf("\n");
    printf("I2C Read/Write brightness test %s\n", isTestPassed ? "PASSED" : "FAILED");

    return isTestPassed ? 0 : 1;
}


// This function calculates the (XOR) checksum of the I2C register
void CalculateI2cChecksum(const NV_I2C_INFO &i2cInfo)
{
    // Calculate the i2c packet checksum and place the 
    // value into the packet

    // i2c checksum is the result of xor'ing all the bytes in the 
    // packet (except for the last data byte, which is the checksum 
    // itself)

    // Packet consists of:

    // The device address...
    BYTE checksum = i2cInfo.i2cDevAddress;

    // Register address...
    for (unsigned int i = 0; i < i2cInfo.regAddrSize; ++i)
    {
        checksum ^= i2cInfo.pbI2cRegAddress[ i ];
    }

    // And data bytes less last byte for checksum...
    for (unsigned int i = 0; i < i2cInfo.cbSize - 1; ++i)
    {
        checksum ^= i2cInfo.pbData[ i ];
    }

    // Store calculated checksum in the last byte of i2c packet
    i2cInfo.pbData[ i2cInfo.cbSize - 1 ] = checksum;
}


// This macro initializes the i2cinfo structure
#define  INIT_I2CINFO(i2cInfo, i2cVersion, displayId, isDDCPort,   \
        i2cDevAddr, regAddr, regSize, dataBuf, bufSize, speed)     \
do {                                                               \
    i2cInfo.version         = i2cVersion;                          \
    i2cInfo.displayMask     = displayId;                           \
    i2cInfo.bIsDDCPort      = isDDCPort;                           \
    i2cInfo.i2cDevAddress   = i2cDevAddr;                          \
    i2cInfo.pbI2cRegAddress = (BYTE*) &regAddr;                    \
    i2cInfo.regAddrSize     = regSize;                             \
    i2cInfo.pbData          = (BYTE*) &dataBuf;                    \
    i2cInfo.cbSize          = bufSize;                             \
    i2cInfo.i2cSpeed        = speed;                               \
}while (0)

// Checks whether the Display Device is compliant with DDC(Display Data Channel) standard and its CI(Command Interface)
BOOL CheckForDDCCICompliance(NvPhysicalGpuHandle hPhysicalGpu, NvU32 displayId)
{
    NvAPI_Status nvapiStatus = NVAPI_OK;

    //Check if the monitor is DDC/CI compliant or not

    NV_I2C_INFO i2cInfo = {0};
    //
    // The 7-bit I2C address for display = Ox37
    // Since we always use 8bits to address, this 7-bit addr (0x37) is placed on
    // the upper 7 bits, and the LSB contains the Read/Write flag:
    // Write = 0 and Read =1;
    //
    NvU8 i2cDeviceAddr = 0x37;
    NvU8 i2cWriteDeviceAddr = i2cDeviceAddr    << 1; //0x6E
    NvU8 i2cReadDeviceAddr = i2cWriteDeviceAddr | 1; //0x6F

    //
    // 1. Send a write packet to request current brightness value
    // The packet consists of the following bytes
    // 0x6E - i2cWriteDeviceAddr
    // Ox51 - Register address
    // 0x82 - Length
    // 0x01 - Read operation flag
    // 0x10 - MCCS brightness code for samsung
    // OxAC - Check sum, xor'ing all the above bytes
    //
    BYTE destAddr[ ] = {0x51};
    BYTE comBytes[ ] = {0x82, 0x01, 0x10, 0xAC};

    INIT_I2CINFO(i2cInfo, NV_I2C_INFO_VER, displayId, TRUE, i2cWriteDeviceAddr, 
                 destAddr, sizeof(destAddr), comBytes, sizeof(comBytes), 27);
    CalculateI2cChecksum( i2cInfo );

    nvapiStatus = NvAPI_I2CWrite(hPhysicalGpu, &i2cInfo);
    if (nvapiStatus != NVAPI_OK)
    {
        printf("  NvAPI_I2CWrite failed with status %d\n", nvapiStatus);
        return FALSE;
    } 

    // Short delay of 100ms, min time should be 30ms for the display to sync up:
    Sleep( 100 );

    // 2. read the response from the display 
    BYTE readBytes[11] = {0}; 
    BYTE i2cRegAddress[] = {0x6E}; // reg address to simulate Barco tool's read op

    INIT_I2CINFO(i2cInfo, NV_I2C_INFO_VER, displayId, TRUE, i2cReadDeviceAddr, 
            i2cRegAddress, sizeof(i2cRegAddress), readBytes, sizeof(readBytes), 27);

    CalculateI2cChecksum( i2cInfo );
    nvapiStatus = NvAPI_I2CRead(hPhysicalGpu, &i2cInfo);
    if (nvapiStatus != NVAPI_OK)
    {
        printf("  NvAPI_I2CRead failed with status %d\n", nvapiStatus);
        return FALSE;
    }
    printf("%X %X %X %X %X %X %X %X %X %X %X \n\n ",  readBytes[0], readBytes[1], 
            readBytes[2], readBytes[3], readBytes[4], readBytes[5], readBytes[6], 
            readBytes[7], readBytes[8], readBytes[9], readBytes[10]);
    if (readBytes[3] != 0x00)
    {
        printf("  ResultCode = %d\n", readBytes[3]);
        return FALSE;
    }
    printf("  The display 0x%x is DDC/CI capable \n\n", displayId);

    // Short delay of 100ms, min time should be 30ms for the display to sync up:
    Sleep( 1000 );

    return TRUE; 
}
// This function actually changes the brightness over the I2C bus by issuing commands and data
BOOL ReadAndChangeBrightness(NvPhysicalGpuHandle hPhysicalGpu, NvU32 displayId, bool bSimulateBarcoTest)
{
    NvAPI_Status nvapiStatus = NVAPI_OK;

    NV_I2C_INFO i2cInfo = {0};
    i2cInfo.version = NV_I2C_INFO_VER;
    //
    // The 7-bit I2C address for display = Ox37
    // Since we always use 8bits to address, this 7-bit addr (0x37) is placed on
    // the upper 7 bits, and the LSB contains the Read/Write flag:
    // Write = 0 and Read =1;
    //
    NvU8 i2cDeviceAddr = 0x37;
    NvU8 i2cWriteDeviceAddr = i2cDeviceAddr    << 1; //0x6E
    NvU8 i2cReadDeviceAddr = i2cWriteDeviceAddr | 1; //0x6F

    //
    // 1. Send a write packet to request current brightness value
    // The packet consists of the following bytes
    // 0x6E - i2cWriteDeviceAddr
    // Ox51 - Register address
    // 0x82 - 0x80 OR n where n = 2 bytes for "read a value" request
    // 0x01 - Read operation flag
    // 0x10 - MCCS brightness code for samsung
    // OxAC - Check sum, xor'ing all the above bytes
    //
    BYTE registerAddr[ ] = {0x51};
    BYTE queryBytes[ ] = { 0x82, 0x01, 0x10, 0xAC}; 

    INIT_I2CINFO(i2cInfo, NV_I2C_INFO_VER, displayId, TRUE, i2cWriteDeviceAddr,
            registerAddr, sizeof(registerAddr), queryBytes, sizeof(queryBytes), 27);

    CalculateI2cChecksum( i2cInfo );

    nvapiStatus = NvAPI_I2CWrite(hPhysicalGpu, &i2cInfo);
    if (nvapiStatus != NVAPI_OK)
    {
        printf("  NvAPI_I2CWrite (request brightness) failed with status %d\n", nvapiStatus);
        return FALSE;
    }
    // Short delay of 100ms, min time should be 30ms for the display to sync up:
    Sleep( 100 );

    //
    // 2. Do a read to get the answer to our brightness value request
    //
    BYTE readBytes[11] = {0}; 
    BYTE i2cRegAddress[] = {0}; // reg address to simulate Barco tool's read op

    INIT_I2CINFO(i2cInfo, NV_I2C_INFO_VER, displayId, TRUE, i2cReadDeviceAddr, 
            i2cRegAddress, sizeof(i2cRegAddress), readBytes, sizeof(readBytes), 27);
    if (bSimulateBarcoTest)
    {
        // Barco test tool (as well as various 3rd party applications, such as CFS from Canvys)
        // do the direct I2C read operation (LSB of device address == 1), but still specify
        // non-zero regAddrSize. 
        // 
        // See bug http://nvbugs/572079 for reference on customer complain; http://nvbugs/534510
        // for CFS tool problem; NvAPI_I2cReadInternal() for reference on direct vs standard read
        // 
        i2cInfo.pbI2cRegAddress = i2cRegAddress;
        i2cInfo.regAddrSize = 1;
    }
    else
    {
        i2cInfo.pbI2cRegAddress = (BYTE *)~0; //not required, ignore
        i2cInfo.regAddrSize = 0;
    }

    nvapiStatus = NvAPI_I2CRead(hPhysicalGpu, &i2cInfo);
    if (nvapiStatus != NVAPI_OK)
    {
        printf("  NvAPI_I2CRead (read brightness) failed with status %d\n", nvapiStatus);
        return FALSE;
    }

    Sleep( 500 );

    //
    // The data packet, Readbytes that is returned by the monitor is ordered as follows
    // Readbytes[0] = Ox6E , ACK to the read device address that was sent earlier
    // Readbytes[1] = 0x88 , 0x80 OR n where n = 8 bytes for answer to read request as follows
    // Readbytes[2] = 0x02 , Fixed value
    // Readbytes[3] = 0x00 , Result Code (00h = NoError, 01h = Unsupported VCP Code)
    // Readbytes[4] = 0x10 , MCCS brightness code value that we want to read
    // Readbytes[5] = 0x00 , VCP type code (00h = Set parameter, 01h = Momentary)
    // Readbytes[6] = 0x?? , Max value (High byte) of your monitor
    // Readbytes[7] = 0x?? , Max value (Low byte) of your monitor
    // Readbytes[8] = 0x?? , Current brightness value (High byte)
    // Readbytes[9] = 0x?? , Current brightness value (Low byte)
    // Readbytes[10] = checksum
    //
    printf("  Current brightness value = %d and max brightness value = %d\n",
           (readBytes[8] << 8 | readBytes[9]), (readBytes[6] << 8 |readBytes[7]));
    //
    // Now Send a write packet to modify current brightness value to 20 (0x14)
    // The packet consists of the following bytes
    // 0x6E - i2cWriteDeviceAddr
    // Ox51 - Register address
    // 0x84 - 0x80 OR n where n = 4 bytes for "modify a value" request
    // 0x03 - change a value flag
    // 0x10 - MCCS brightness code for samsung
    // Ox00 - new brightness value high byte
    // 0x14 - new brightness value low byte (=20)
    // 0xBC - checksum, , xor'ing all the above bytes
    //
    BYTE modifyBytes[ ] = { 0x84, 0x03, 0x10, 0x00, BRIGHTNESS_TEST_VALUE, 0xBC}; 

    INIT_I2CINFO(i2cInfo, NV_I2C_INFO_VER, displayId, TRUE, i2cWriteDeviceAddr,
            registerAddr, sizeof(registerAddr), modifyBytes, sizeof(modifyBytes), 27);
    CalculateI2cChecksum( i2cInfo );

    nvapiStatus = NvAPI_I2CWrite(hPhysicalGpu, &i2cInfo);
    if (nvapiStatus != NVAPI_OK)
    {
        printf("  NvAPI_I2CWrite (revise brightness) failed with status %d\n", nvapiStatus);
        return FALSE;
    }

    Sleep( 100 );
    printf("\n  Succesfully modified brightness value to %d!\n", BRIGHTNESS_TEST_VALUE);

    printf("  Pausing for %d seconds...\n", DELAY_IN_SEC);
    Sleep( DELAY_IN_SEC * 1000 ); // Wait for the user to notice the brightness change

    //
    // restore the brightness, so the user doesn't have to fiddle
    // around with the monitor front panel controls:
    //
    modifyBytes[3] = readBytes[8];   // Set brightness back to what it was before
    modifyBytes[4] = readBytes[9];

    CalculateI2cChecksum( i2cInfo );

    nvapiStatus = NvAPI_I2CWrite(hPhysicalGpu, &i2cInfo);
    if (nvapiStatus != NVAPI_OK)
    {
        printf("  NvAPI_I2CWrite (restore brightness) failed with status %d\n", nvapiStatus);
        return FALSE;
    }
    Sleep( 100 );
    printf("\n  Succesfully restored the brightness value to %d.\n", readBytes[9]);

    printf("  Pausing for %d more seconds...\n", DELAY_IN_SEC);
    Sleep( DELAY_IN_SEC * 1000 );

    return TRUE;
}
