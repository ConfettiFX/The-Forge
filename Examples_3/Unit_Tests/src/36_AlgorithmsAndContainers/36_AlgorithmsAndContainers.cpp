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

// NOTE: This unit test requires AUTOMATED_TESTING macro at the top of Config.h

// Interfaces
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"

#include "AlgorithmsTest.h"

// Renderer
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#ifdef AUTOMATED_TESTING
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/bstrlib/bstest.h"
#endif

#define STBDS_UNIT_TESTS
#include "../../../../Common_3/Utilities/Math/BStringHashMap.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

static const char appName[] = "36_AlgorithmsAndContainers";

#ifdef AUTOMATED_TESTING
// This variable disables actual assertions for testing purposes
// Should be initialized to true for bstrlib tests
extern "C" bool gIsBstrlibTest;
#endif

int testMatrices();

class Transformations: public IApp
{
public:
    bool Init()
    {
        // FILE PATHS
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");

        //////////////////////////////////////////////
        // Actual unit tests
        //////////////////////////////////////////////

        int ret = testStableSort();
        if (ret == 0)
            LOGF(eINFO, "Stable sort test success");
        else
        {
            LOGF(eERROR, "Stable sort test failed.");
            ASSERT(false);
            return false;
        }

        ret = testMatrices();
        if (ret == 0)
            LOGF(eINFO, "Matrices test success");
        else
        {
            LOGF(eERROR, "Matrices test failed.");
            ASSERT(false);
            return false;
        }

#ifdef AUTOMATED_TESTING
        gIsBstrlibTest = true;
        ret = runBstringTests();
        gIsBstrlibTest = false;
        if (ret == 0)
            LOGF(eINFO, "Bstring test success.");
        else
        {
            LOGF(eERROR, "Bstring test failed.");
            ASSERT(false);
            return false;
        }
#else
        LOGF(eERROR, "Bstring tests can't run without AUTOMATED_TESTING macro");
#endif

        stbds_unit_tests();
        LOGF(eINFO, "stbds test success.");
        stbds_bstring_unit_tests();
        LOGF(eINFO, "stbds bstring extension test success.");
        requestShutdown();
        return true;
    }

    void Exit() {}

    bool Load(ReloadDesc* pReloadDesc)
    {
        UNREF_PARAM(pReloadDesc);
        return true;
    }

    void Unload(ReloadDesc* pReloadDesc) { UNREF_PARAM(pReloadDesc); }

    void Update(float deltaTime) { UNREF_PARAM(deltaTime); }

    void Draw() {}

    const char* GetName() { return appName; }
};

DEFINE_APPLICATION_MAIN(Transformations)

// Tests

int testMatrices()
{
    // All projection matrices map Z into [0, 1] range.
    // That includes all LH and RH projection matrices.
    // float
    {
        Matrix4 viewLHMatrix = Matrix4::lookAtLH(Point3(0.0f, 0.0f, 0.0f), Point3(0.0f, 0.0f, 100.0f), Vector3(0.0f, 1.0f, 0.0f));
        Matrix4 viewRHMatrix = Matrix4::lookAtRH(Point3(0.0f, 0.0f, 0.0f), Point3(0.0f, 0.0f, -100.0f), Vector3(0.0f, 1.0f, 0.0f));

        Matrix4 orthoLHMatrix = Matrix4::orthographicLH(-10.0f, 10.0f, -10.0f, 10.0f, 10.0f, 1000.0f);
        Matrix4 orthoRHMatrix = Matrix4::orthographicRH(-10.0f, 10.0f, -10.0f, 10.0f, 10.0f, 1000.0f);
        Matrix4 perspectiveLHMatrix = Matrix4::perspectiveLH(1.0f, 1.0f, 10.0f, 1000.0f);
        Matrix4 perspectiveRHMatrix = Matrix4::perspectiveRH(1.0f, 1.0f, 10.0f, 1000.0f);
        Matrix4 cubeProjectionLHMatrix = Matrix4::cubeProjectionLH(10.0f, 1000.0f);
        Matrix4 cubeProjectionRHMatrix = Matrix4::cubeProjectionRH(10.0f, 1000.0f);

        Matrix4 viewOrthoLHMatrix = orthoLHMatrix * viewLHMatrix;
        Matrix4 viewOrthoRHMatrix = orthoRHMatrix * viewRHMatrix;
        Matrix4 viewPerspectiveLHMatrix = perspectiveLHMatrix * viewLHMatrix;
        Matrix4 viewPerspectiveRHMatrix = perspectiveRHMatrix * viewRHMatrix;
        Matrix4 viewCubeProjectedLHMatrix = cubeProjectionLHMatrix * viewLHMatrix;
        Matrix4 viewCubeProjectedRHMatrix = cubeProjectionRHMatrix * viewRHMatrix;

        for (int i = 0; i < 100; i++)
        {
            float x = randomFloat(-100.0f, 100.0f);
            float y = randomFloat(-100.0f, 100.0f);
            float z = randomFloat(20.0f, 100.0f);

            vec4 pLH = viewOrthoLHMatrix * vec4(x, y, z, 1.0f);
            vec4 pRH = viewOrthoRHMatrix * vec4(x, y, -z, 1.0f);
            if (pLH != pRH)
                return 1;

            pLH = viewPerspectiveLHMatrix * vec4(x, y, z, 1.0f);
            pRH = viewPerspectiveRHMatrix * vec4(x, y, -z, 1.0f);
            if (pLH != pRH)
                return 1;

            pLH = viewCubeProjectedLHMatrix * vec4(x, y, z, 1.0f);
            pRH = viewCubeProjectedRHMatrix * vec4(x, y, -z, 1.0f);
            if (pLH != pRH)
                return 1;
        }
    }

    // double
    {
        Matrix4d viewLHMatrix = Matrix4d::lookAtLH(Point3(0.0f, 0.0f, 0.0f), Point3(0.0f, 0.0f, 100.0f), Vector3d(0.0f, 1.0f, 0.0f));
        Matrix4d viewRHMatrix = Matrix4d::lookAtRH(Point3(0.0f, 0.0f, 0.0f), Point3(0.0f, 0.0f, -100.0f), Vector3d(0.0f, 1.0f, 0.0f));

        Matrix4d orthoLHMatrix = Matrix4d::orthographicLH(-10.0f, 10.0f, -10.0f, 10.0f, 10.0f, 1000.0f);
        Matrix4d orthoRHMatrix = Matrix4d::orthographicRH(-10.0f, 10.0f, -10.0f, 10.0f, 10.0f, 1000.0f);
        Matrix4d perspectiveLHMatrix = Matrix4d::perspectiveLH(1.0f, 1.0f, 10.0f, 1000.0f);
        Matrix4d perspectiveRHMatrix = Matrix4d::perspectiveRH(1.0f, 1.0f, 10.0f, 1000.0f);
        Matrix4d cubeProjectionLHMatrix = Matrix4d::cubeProjectionLH(10.0f, 1000.0f);
        Matrix4d cubeProjectionRHMatrix = Matrix4d::cubeProjectionRH(10.0f, 1000.0f);

        Matrix4d viewOrthoLHMatrix = orthoLHMatrix * viewLHMatrix;
        Matrix4d viewOrthoRHMatrix = orthoRHMatrix * viewRHMatrix;
        Matrix4d viewPerspectiveLHMatrix = perspectiveLHMatrix * viewLHMatrix;
        Matrix4d viewPerspectiveRHMatrix = perspectiveRHMatrix * viewRHMatrix;
        Matrix4d viewCubeProjectedLHMatrix = cubeProjectionLHMatrix * viewLHMatrix;
        Matrix4d viewCubeProjectedRHMatrix = cubeProjectionRHMatrix * viewRHMatrix;

        for (int i = 0; i < 100; i++)
        {
            float x = randomFloat(-100.0f, 100.0f);
            float y = randomFloat(-100.0f, 100.0f);
            float z = randomFloat(20.0f, 100.0f);

            vec4d pLH = viewOrthoLHMatrix * vec4d(x, y, z, 1.0f);
            vec4d pRH = viewOrthoRHMatrix * vec4d(x, y, -z, 1.0f);
            if (pLH.getX() != pRH.getX() || pLH.getY() != pRH.getY() || pLH.getZ() != pRH.getZ() || pLH.getW() != pRH.getW())
                return 1;

            pLH = viewPerspectiveLHMatrix * vec4d(x, y, z, 1.0f);
            pRH = viewPerspectiveRHMatrix * vec4d(x, y, -z, 1.0f);
            if (pLH.getX() != pRH.getX() || pLH.getY() != pRH.getY() || pLH.getZ() != pRH.getZ() || pLH.getW() != pRH.getW())
                return 1;

            pLH = viewCubeProjectedLHMatrix * vec4d(x, y, z, 1.0f);
            pRH = viewCubeProjectedRHMatrix * vec4d(x, y, -z, 1.0f);
            if (pLH.getX() != pRH.getX() || pLH.getY() != pRH.getY() || pLH.getZ() != pRH.getZ() || pLH.getW() != pRH.getW())
                return 1;
        }
    }

    return 0;
}
