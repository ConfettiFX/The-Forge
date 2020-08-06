//========================================= #TheForgeMathExtensionsBegin ================================================
//========================================= #TheForgeAnimationMathExtensionsBegin =======================================

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

#ifndef VECTORMATH_SOA_TRANSFORM_HPP
#define VECTORMATH_SOA_TRANSFORM_HPP

namespace Vectormath
{
namespace Soa
{

//----------------------------------------------------------------------------
// SOA Transform
//----------------------------------------------------------------------------

inline SoaTransform SoaTransform::identity() {
  const SoaTransform ret = {SoaFloat3::zero(), SoaQuaternion::identity(),
                            SoaFloat3::one()};
  return ret;
}

} // namespace Soa
} // namespace Vectormath

#endif // VECTORMATH_SOA_TRANSFORM_HPP

//========================================= #TheForgeAnimationMathExtensionsEnd =======================================
//========================================= #TheForgeMathExtensionsEnd ================================================