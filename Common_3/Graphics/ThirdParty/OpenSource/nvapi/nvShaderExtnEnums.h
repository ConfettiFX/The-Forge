 /************************************************************************************************************************************\
|*                                                                                                                                    *|
|*     Copyright © 2012 NVIDIA Corporation.  All rights reserved.                                                                     *|
|*                                                                                                                                    *|
|*  NOTICE TO USER:                                                                                                                   *|
|*                                                                                                                                    *|
|*  This software is subject to NVIDIA ownership rights under U.S. and international Copyright laws.                                  *|
|*                                                                                                                                    *|
|*  This software and the information contained herein are PROPRIETARY and CONFIDENTIAL to NVIDIA                                     *|
|*  and are being provided solely under the terms and conditions of an NVIDIA software license agreement.                             *|
|*  Otherwise, you have no rights to use or access this software in any manner.                                                       *|
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

////////////////////////////////////////////////////////////////////////////////
////////////////////////// NVIDIA SHADER EXTENSIONS ////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// This file can be included both from HLSL shader code as well as C++ code.
// The app should call NvAPI_D3D11_IsNvShaderExtnOpCodeSupported() / NvAPI_D3D12_IsNvShaderExtnOpCodeSupported()
// to check for support for every nv shader extension opcode it plans to use



//----------------------------------------------------------------------------//
//---------------------------- NV Shader Extn Version  -----------------------//
//----------------------------------------------------------------------------//
#define NV_SHADER_EXTN_VERSION                              1

//----------------------------------------------------------------------------//
//---------------------------- Misc constants --------------------------------//
//----------------------------------------------------------------------------//
#define NV_WARP_SIZE                                       32
#define NV_WARP_SIZE_LOG2                                   5

//----------------------------------------------------------------------------//
//---------------------------- opCode constants ------------------------------//
//----------------------------------------------------------------------------//


#define NV_EXTN_OP_SHFL                                     1
#define NV_EXTN_OP_SHFL_UP                                  2
#define NV_EXTN_OP_SHFL_DOWN                                3
#define NV_EXTN_OP_SHFL_XOR                                 4

#define NV_EXTN_OP_VOTE_ALL                                 5
#define NV_EXTN_OP_VOTE_ANY                                 6
#define NV_EXTN_OP_VOTE_BALLOT                              7

#define NV_EXTN_OP_GET_LANE_ID                              8
#define NV_EXTN_OP_FP16_ATOMIC                             12
#define NV_EXTN_OP_FP32_ATOMIC                             13

#define NV_EXTN_OP_GET_SPECIAL                             19

#define NV_EXTN_OP_UINT64_ATOMIC                           20

#define NV_EXTN_OP_MATCH_ANY                               21 

// FOOTPRINT - For Sample and SampleBias
#define NV_EXTN_OP_FOOTPRINT                               28
#define NV_EXTN_OP_FOOTPRINT_BIAS                          29

#define NV_EXTN_OP_GET_SHADING_RATE                        30

// FOOTPRINT - For SampleLevel and SampleGrad
#define NV_EXTN_OP_FOOTPRINT_LEVEL                         31
#define NV_EXTN_OP_FOOTPRINT_GRAD                          32

// SHFL Generic
#define NV_EXTN_OP_SHFL_GENERIC                            33

#define NV_EXTN_OP_VPRS_EVAL_ATTRIB_AT_SAMPLE              51
#define NV_EXTN_OP_VPRS_EVAL_ATTRIB_SNAPPED                52



//----------------------------------------------------------------------------//
//-------------------- GET_SPECIAL subOpCode constants -----------------------//
//----------------------------------------------------------------------------//
#define NV_SPECIALOP_THREADLTMASK                           4
#define NV_SPECIALOP_FOOTPRINT_SINGLELOD_PRED               5
#define NV_SPECIALOP_GLOBAL_TIMER_LO                        9
#define NV_SPECIALOP_GLOBAL_TIMER_HI                       10

//----------------------------------------------------------------------------//
//----------------------------- Texture Types  -------------------------------//
//----------------------------------------------------------------------------//
#define NV_EXTN_TEXTURE_1D                                  2
#define NV_EXTN_TEXTURE_1D_ARRAY                            3
#define NV_EXTN_TEXTURE_2D                                  4
#define NV_EXTN_TEXTURE_2D_ARRAY                            5
#define NV_EXTN_TEXTURE_3D                                  6
#define NV_EXTN_TEXTURE_CUBE                                7
#define NV_EXTN_TEXTURE_CUBE_ARRAY                          8


//---------------------------------------------------------------------------//
//----------------FOOTPRINT Enums for NvFootprint* extns---------------------//
//---------------------------------------------------------------------------//
#define NV_EXTN_FOOTPRINT_MODE_FINE                         0
#define NV_EXTN_FOOTPRINT_MODE_COARSE                       1
