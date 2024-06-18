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

#include "SanMiguel.h"

#include "../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../Common_3/Utilities/Interfaces/ILog.h"

#include "../../../Common_3/Utilities/Interfaces/IMemory.h"

#define DEFAULT_ALBEDO           "Default"
#define DEFAULT_NORMAL           "Default_NRM"
#define DEFAULT_SPEC             "Default_SPEC"
#define DEFAULT_SPEC_TRANSPARENT "Default_SPEC_TRANS"

static void setTextures(Scene* pScene, int index, const char* albedo, const char* specular, const char* normal, uint32_t matFlags)
{
    pScene->materialFlags[index] = matFlags;

    // default textures
    pScene->textures[index] = (char*)tf_calloc(strlen(albedo) + 5, sizeof(char));
    pScene->specularMaps[index] = (char*)tf_calloc(strlen(specular) + 5, sizeof(char));
    pScene->normalMaps[index] = (char*)tf_calloc(strlen(normal) + 5, sizeof(char));

    snprintf(pScene->textures[index], strlen(albedo) + 5, "%s.tex", albedo);
    snprintf(pScene->specularMaps[index], strlen(specular) + 5, "%s.tex", specular);
    snprintf(pScene->normalMaps[index], strlen(normal) + 5, "%s.tex", normal);
}

static void setDefaultTextures(Scene* pScene, int index)
{
    setTextures(pScene, index, DEFAULT_ALBEDO, DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);
}

static void SetMaterials(Scene* pScene, bool transparentFlags)
{
    int index = 0;

    // 0	flags
    if (transparentFlags)
    {
        setTextures(pScene, index++, "ForgeFlags", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_TRANSPARENT);
    }
    else
    {
        setTextures(pScene, index++, "ForgeFlags", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);
    }

    // 0	arc034
    setTextures(pScene, index++, "arco_frente", "arco_frente_SPEC", "arco_frente_NRM", MATERIAL_FLAG_NONE);

    // 1	hugeBackWall00
    setDefaultTextures(pScene, index++);

    // 2	leaves0379
    setTextures(pScene, index++, "citrus_limon_leaf", "citrus_limon_leaf_SPEC", "citrus_limon_leaf_NRM",
                MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 3	hugeWallFront00
    setTextures(pScene, index++, "Barro_2", "Barro_2_SPEC", "Barro_2_NRM", MATERIAL_FLAG_NONE);

    // 4	floor2nd00
    setDefaultTextures(pScene, index++);

    // 5	leaves013
    setTextures(pScene, index++, "HP01lef2", "HP01lef2_SPEC", DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 6	leaves014
    setTextures(pScene, index++, "FL13lef4", "FL13lef4_SPEC", DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 7	metalHat
    setTextures(pScene, index++, "metal_viejo_2", "metal_viejo_2_SPEC", "metal_viejo_2_NRM", MATERIAL_FLAG_NONE);

    // 8	metal010
    setTextures(pScene, index++, "Fierro_A", "Fierro_A_SPEC", "Fierro_A_NRM", MATERIAL_FLAG_NONE);

    // 9	floor03
    setTextures(pScene, index++, "piso_rustico", "piso_rustico_SPEC", "piso_rustico_NRM", MATERIAL_FLAG_NONE);

    // 10	wall03
    setTextures(pScene, index++, "muros_b2", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 11	flower0342
    setTextures(pScene, index++, "FL13pet1", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 12	stem015
    setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 13	stem014
    setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 14	stem016
    setTextures(pScene, index++, "FL19stm", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 15	leaves0377
    setTextures(pScene, index++, "FL17lef2", "FL17lef2_SPEC", "FL17lef2_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 16	plate00
    setTextures(pScene, index++, "plato_a", "plato_a_SPEC", "plato_a_NRM", MATERIAL_FLAG_NONE);

    // 17	sideLeaves1:group146
    setTextures(pScene, index++, "l37-upper", "l33-upper_SPEC", "l37-upper_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 18	leaves0378
    setTextures(pScene, index++, "FL17lef2", "FL17lef2_SPEC", "FL17lef2_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 19	treeBranch
    setTextures(pScene, index++, "bark06mi", DEFAULT_SPEC, "bark06mi_NRM", MATERIAL_FLAG_NONE);

    // 20	basket02
    setTextures(pScene, index++, "Maceta_A_Color", "Maceta_A_Color_SPEC", "Maceta_A_Color_NRM", MATERIAL_FLAG_NONE);

    // 21	leaves026
    setTextures(pScene, index++, "FL19lef2", "FL19lef2_SPEC", "FL19lef2_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 22	chainMetal00
    setTextures(pScene, index++, "metal_viejo_2", "metal_viejo_2_SPEC", "metal_viejo_2_NRM", MATERIAL_FLAG_NONE);

    // 23	cloth01
    setTextures(pScene, index++, "tela_mesa_b", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 24	wood01
    setTextures(pScene, index++, "WOOD08", "WOOD08_SPEC", "WOOD08_NRM", MATERIAL_FLAG_NONE);

    // 25	picture07
    setTextures(pScene, index++, "D30_Smiguel_2003_7833", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 26	smallBackWall00
    setTextures(pScene, index++, "muros_d", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 27	treeLeaves00
    setTextures(pScene, index++, "sm_leaf_02a", "sm_leaf_02a_SPEC", "sm_leaf_02_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 28	stem0328
    setTextures(pScene, index++, "ampelopsis_brevipedunculata_bark", DEFAULT_SPEC, DEFAULT_NORMAL,
                MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 29	salt00
    setTextures(pScene, index++, "brushedMetal", "brushedMetal_SPEC", "brushedMetal_NRM", MATERIAL_FLAG_NONE);

    // 30	leaves00
    setTextures(pScene, index++, "FL16lef3", "FL16lef1_SPEC", "FL16lef3_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 31	metal07
    setTextures(pScene, index++, "brushedMetal", "brushedMetal_SPEC", "brushedMetal_NRM", MATERIAL_FLAG_NONE);

    // 32	underLeaves02
    setTextures(pScene, index++, "l37-upper", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 33	rose00
    setTextures(pScene, index++, "FL13pet2", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 34	cup
    setTextures(pScene, index++, "tela_mesa_b", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 35	leaves012
    setTextures(pScene, index++, "HP17lef2", "HP17lef2_SPEC", DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 36	cloth00
    setTextures(pScene, index++, "tela_mesa_d", "tela_mesa_d_SPEC", DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 37	cloth02
    setTextures(pScene, index++, "tela_blanca", "tela_blanca_SPEC", "tela_blanca_NRM", MATERIAL_FLAG_NONE);

    // 38	underLeaves01
    setTextures(pScene, index++, "l04-upper", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 39	stem06
    setTextures(pScene, index++, "HP19stm1", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 40	leaves02
    setTextures(pScene, index++, "HP17lef2", "HP17lef2_SPEC", "HP17lef2_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 41	leaves015
    setTextures(pScene, index++, "HP13lef1", "HP13lef1_SPEC", "hp13lef1_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 42	flower01
    setTextures(pScene, index++, "FL19pe15", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 43	stem010
    setTextures(pScene, index++, "FL29twg", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 44	leaves022
    setTextures(pScene, index++, "FL16lef3", "FL16lef1_SPEC", "FL16lef3_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 45	leaves024
    setTextures(pScene, index++, "HP17lef2", "HP17lef2_SPEC", "HP17lef2_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 46	leaves021
    setTextures(pScene, index++, "FL16pet1", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 47	basket04
    setTextures(pScene, index++, "Maceta_C_Color", "Maceta_C_Color_SPEC", "Maceta_C_NRM", MATERIAL_FLAG_NONE);

    // 48	door00
    setTextures(pScene, index++, "WOOD08", "WOOD08_SPEC", "WOOD08_NRM", MATERIAL_FLAG_NONE);

    // 49	leaves023
    setTextures(pScene, index++, "HP13lef1", "HP13lef1_SPEC", "hp13lef1_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 50	underCeil
    setTextures(pScene, index++, "techo", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 51	leaves08
    setTextures(pScene, index++, "TR02lef5", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 52	wood02
    setTextures(pScene, index++, "wood.3.Bubinga", "WOOD08_SPEC", "WOOD08_NRM", MATERIAL_FLAG_NONE);

    // 53	wall02
    setTextures(pScene, index++, "muros_b1", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 54	leaves09
    setTextures(pScene, index++, "HP19lef2", "HP19lef2_SPEC", "HP19lef2_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 55	stem013
    setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 56	metal07
    setTextures(pScene, index++, "Fierro_A", "Fierro_A_SPEC", "Fierro_A_NRM", MATERIAL_FLAG_NONE);

    // 57	paper
    setTextures(pScene, index++, "individual_b", "individual_b_SPEC", "individual_b_NRM", MATERIAL_FLAG_NONE);

    // 58	lightBulb
    setTextures(pScene, index++, DEFAULT_ALBEDO, DEFAULT_SPEC_TRANSPARENT, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 59	giwa
    setTextures(pScene, index++, "Barro_2", "Barro_2_SPEC", "Barro_2_NRM", MATERIAL_FLAG_NONE);

    // 60	flower0304
    setTextures(pScene, index++, "HP01pet1", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 61	picture02
    setTextures(pScene, index++, "D30_Smiguel_2003_7815", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 62	picture01
    setTextures(pScene, index++, "D30_Smiguel_2003_7785", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 63	metal09
    setTextures(pScene, index++, "rust_a1", "rust_a1_SPEC", "rust_a1_NRM", MATERIAL_FLAG_NONE);

    // 64	arc_floor00
    setTextures(pScene, index++, "arcos_lisos_3_color_1", "arcos_lisos_3_color_1_SPEC", "arcos_lisos_3_color_1_NRM", MATERIAL_FLAG_NONE);

    // 65	hugeWallLeft
    setTextures(pScene, index++, "muros_n", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 66	cap
    setTextures(pScene, index++, "muros_q_patio2", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 67	flower00
    setTextures(pScene, index++, "HP01pet3", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 68	stem0369
    setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 69	ceil_arc_cap
    setTextures(pScene, index++, "moldura_techo", "moldura_techo_SPEC", "moldura_techo_NRM", MATERIAL_FLAG_NONE);

    // 70	stem08
    setTextures(pScene, index++, "FL29twg", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 71	wood03
    setTextures(pScene, index++, "WOOD08", "WOOD08_SPEC", "WOOD08_NRM", MATERIAL_FLAG_NONE);

    // 72	wall01
    setTextures(pScene, index++, "muros_l", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 73	hugeBackWall01
    setTextures(pScene, index++, "muros_h", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 74	picture06
    setTextures(pScene, index++, "0001_carros", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 75	leaves01
    setTextures(pScene, index++, "aglaonema_leaf", "aglaonema_leaf_SPEC", DEFAULT_NORMAL,
                MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 76	sool00
    setTextures(pScene, index++, "FL29cnt2", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 77	capital
    setTextures(pScene, index++, "detmoldura_06_color", "detmoldura_06_color_SPEC", "detmoldura_06_color_NRM", MATERIAL_FLAG_NONE);

    // 78	underCeil00
    setTextures(pScene, index++, "cantera_naranja_liso", "cantera_naranja_liso_SPEC", "cantera_naranja_liso_NRM", MATERIAL_FLAG_NONE);

    // 79	leaves06
    setTextures(pScene, index++, "citrus_limon_leaf", "citrus_limon_leaf_SPEC", "citrus_limon_leaf_NRM",
                MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 80	group21
    setTextures(pScene, index++, "muros_c2", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 81	leaves07
    setTextures(pScene, index++, "FL29lef", "FL29lef_SPEC", "FL29lef_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 83	underLeaves00
    setTextures(pScene, index++, "FL13pet2", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 84	flower0339
    setTextures(pScene, index++, "FL19pe15", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 85	sool01
    setTextures(pScene, index++, "FL29cnt2", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 86	picture05
    setTextures(pScene, index++, "06-30-1997", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 87	stem05
    setTextures(pScene, index++, "FL29twg", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 88	stem04
    setTextures(pScene, index++, "FL19stm", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 89	foundtain02
    setTextures(pScene, index++, "techo_01", "techo_SPEC", "techo_NRM", MATERIAL_FLAG_NONE);

    // 91	foundtain01
    setTextures(pScene, index++, "techo_01", "techo_SPEC", "techo_NRM", MATERIAL_FLAG_NONE);

    // 92	pictureEdge
    setTextures(pScene, index++, "wood.3.Bubinga", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 93	SOY
    setTextures(pScene, index++, "PITTED", "madera_marcos_SPEC", DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 94	metal06
    setTextures(pScene, index++, "rust_a1", "rust_a1_SPEC", "rust_a1_NRM", MATERIAL_FLAG_NONE);

    // 95	picture15
    setTextures(pScene, index++, "D30_Smiguel_2003_7758", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 96	floor00
    setTextures(pScene, index++, "piso_rustico", "piso_rustico_SPEC", "piso_rustico_NRM", MATERIAL_FLAG_NONE);

    // 97	leaves017
    setTextures(pScene, index++, "FL29lef", "FL29lef_SPEC", "FL29pet1_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 98	hugeWall00
    setTextures(pScene, index++, "muros_a", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 99	picture14
    setTextures(pScene, index++, "tapa_talabera", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 100	stem012
    setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 102	stem003
    setTextures(pScene, index++, "FL19stm", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 103	leaves019
    setTextures(pScene, index++, "HP11lef2", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 104	leaves018
    setTextures(pScene, index++, "FL19lef4", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 105	metal00
    setTextures(pScene, index++, "rust_a1", "rust_a1_SPEC", "rust_a1_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 106	stem02
    setTextures(pScene, index++, "FL19stm", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 107	floorOutside00
    setTextures(pScene, index++, "052terresable", "052terresable_SPEC", "052terresable_NRM", MATERIAL_FLAG_NONE);

    // 108	capitel
    setTextures(pScene, index++, "detmoldura_06_color", "detmoldura_06_color_SPEC", "detmoldura_06_color_NRM", MATERIAL_FLAG_NONE);

    // 109	pillar00
    setTextures(pScene, index++, "concrete", "concrete_SPEC", "concrete_NRM", MATERIAL_FLAG_NONE);

    // 110	windowStairCase
    setTextures(pScene, index++, "concreto_02", "concreto_02_SPEC", "concreto_02_NRM", MATERIAL_FLAG_NONE);

    // 111	stem01
    setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 112	doorCover
    setTextures(pScene, index++, "marco_puerta_1", "marco_puerta_1_SPEC", "marco_puerta_1_NRM", MATERIAL_FLAG_NONE);

    // 113	Wall00
    setTextures(pScene, index++, "muros_g", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 114	stair00
    setTextures(pScene, index++, "escalera_color", "escalera_color_SPEC", "escalera_color_NRM", MATERIAL_FLAG_NONE);

    // 115	treeLeaves03
    setTextures(pScene, index++, "sm_leaf_02b", "sm_leaf_02b_SPEC", "sm_leaf_02b_NRM",
                MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 116	trans00
    setTextures(pScene, index++, DEFAULT_ALBEDO, DEFAULT_SPEC_TRANSPARENT, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 117	gagoyle
    setTextures(pScene, index++, "concreto_02", "concreto_02_SPEC", "concreto_02_NRM", MATERIAL_FLAG_NONE);

    // 118	picture00
    setTextures(pScene, index++, "D30_Smiguel_2003_7843", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 119	ceils
    setTextures(pScene, index++, "techo", "techo_SPEC", "techo_NRM", MATERIAL_FLAG_NONE);

    // 120	flower0338
    setTextures(pScene, index++, "HP01pet3", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 121	stem09
    setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 122	basket01
    setTextures(pScene, index++, "Maceta_B_Color", "Maceta_C_Color_SPEC", "Maceta_B_NRM", MATERIAL_FLAG_NONE);

    // 123	basket00
    setTextures(pScene, index++, "Maceta_D2_Color_0", "Maceta_C_Color_SPEC", "Maceta_B_NRM", MATERIAL_FLAG_NONE);

    // 124	metal01
    setTextures(pScene, index++, "Fierro_A", "Fierro_A_SPEC", "Fierro_A_NRM", MATERIAL_FLAG_NONE);

    // 125	sideFloor2nd
    setTextures(pScene, index++, "piso_rustico", "piso_rustico_SPEC", "piso_rustico_NRM", MATERIAL_FLAG_NONE);

    // 126	wall00
    setTextures(pScene, index++, "muros_q_patio2", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 127	picture12
    setTextures(pScene, index++, "D30_Smiguel_2003_7843", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 128	flower0343
    setTextures(pScene, index++, "FL19pe16", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 129	leaves0300
    setTextures(pScene, index++, "FL29lef", "FL29lef_SPEC", "FL29lef_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 130	sool0337
    setTextures(pScene, index++, "FL29cnt2", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 131	arc01
    setTextures(pScene, index++, "arcos_lisos_2_color", "arcos_lisos_2_color_SPEC", "arcos_lisos_2_color_NRM", MATERIAL_FLAG_NONE);

    // 132	arc00
    // setTextures(pScene, index++, "techo", "techo_SPEC", "techo_NRM", MATERIAL_FLAG_NONE);

    // 133	outsideWall
    setTextures(pScene, index++, "newWall", DEFAULT_SPEC, "newWall_NRM", MATERIAL_FLAG_NONE);

    // 134	metal03
    setTextures(pScene, index++, "rust_detalle", "rust_detalle_SPEC", "rust_detalle_NRM", MATERIAL_FLAG_NONE);

    // 135	muros01
    setTextures(pScene, index++, "muros_k", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 136	wall05
    setTextures(pScene, index++, "muros_e", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 137	metal04
    setTextures(pScene, index++, "Fierro_A", "Fierro_A_SPEC", "Fierro_A_NRM", MATERIAL_FLAG_NONE);

    // 138	metal02 mmm
    setTextures(pScene, index++, "rust_detalle", "rust_detalle_SPEC", "rust_detalle_NRM", MATERIAL_FLAG_NONE);

    // 139	arc03
    // setTextures(pScene, index++, "techo", "techo_SPEC", "techo_NRM", MATERIAL_FLAG_NONE);

    // 140	metal05
    setTextures(pScene, index++, "rust_detalle", "rust_detalle_SPEC", "rust_detalle_NRM", MATERIAL_FLAG_NONE);

    // 141	metal0104
    setTextures(pScene, index++, "Fierro_A", "Fierro_A_SPEC", "Fierro_A_NRM", MATERIAL_FLAG_NONE);

    // 142	sool02199
    setTextures(pScene, index++, "FL29cnt2", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 143	stem01
    setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 143	leaves0380
    setTextures(pScene, index++, "HP01lef2", "HP01lef2_SPEC", DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 143	stem0330
    setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 144	stem0368
    setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 145	leaves025
    setTextures(pScene, index++, "FL29pet2", "FL29lef_SPEC", "FL29pet2_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 146	utencil
    setTextures(pScene, index++, "brushedMetal", "brushedMetal_SPEC", "brushedMetal_NRM", MATERIAL_FLAG_NONE);

    // 147	door01
    setTextures(pScene, index++, "wood.3.Bubinga", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 148	treeLeaves02
    setTextures(pScene, index++, "sm_leaf_seca_02b", "sm_leaf_02b_SPEC", "sm_leaf_02b_NRM",
                MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 149	stem0330
    setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 150	leaves027
    setTextures(pScene, index++, "FL29pet2", "FL29lef_SPEC", "FL29pet2_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 151	treeLeaves01
    setTextures(pScene, index++, "sm_leaf_02a", "sm_leaf_02b_SPEC", "sm_leaf_02b_NRM",
                MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 152	stem0388
    setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 153	stem0291
    setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 154	flower0344
    setTextures(pScene, index++, "FL17pet1", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 155	treeLeaves06
    setTextures(pScene, index++, "FL29pet2", "FL29pet2_SPEC", "FL29pet2_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 156	treeLeaves05
    setTextures(pScene, index++, "l37-upper", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 157	stem0386
    setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 158	picture04
    setTextures(pScene, index++, "06-30-1997", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 159	sool0361
    setTextures(pScene, index++, "FL29cnt1", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 160	foundtain00
    setTextures(pScene, index++, "techo_01", "techo_SPEC", "techo_NRM", MATERIAL_FLAG_NONE);

    // 161	leaves011
    setTextures(pScene, index++, "FL17lef2", "FL17lef2_SPEC", "FL17lef2_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 162	underBalconi
    setTextures(pScene, index++, "techo", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 163	leaves0381
    setTextures(pScene, index++, "TR02lef5", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 164	leaves010
    setTextures(pScene, index++, "TR14lef1", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 165	group35
    setTextures(pScene, index++, "muros_q4", "muros_q4_SPEC", "muros_q4_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 166	treeLeaves04
    setTextures(pScene, index++, "sm_leaf_02a", "sm_leaf_02a_SPEC", "sm_leaf_02_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 167	picture08
    setTextures(pScene, index++, "D30_Smiguel_2003_7833", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 168	treeLeaves08
    setTextures(pScene, index++, "l04-upper", "l04-upper_SPEC", "l04-upper_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 169	floor02
    setTextures(pScene, index++, DEFAULT_ALBEDO, DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 170	leaves04
    setTextures(pScene, index++, "HP19lef3", "HP19lef3_SPEC", "HP19lef3_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 171	bat00
    setTextures(pScene, index++, "madera_marcos", "madera_marcos_SPEC", "madera_marcos_NRM", MATERIAL_FLAG_NONE);

    // 172	ceil00
    setTextures(pScene, index++, "WOOD08", "WOOD08_SPEC", "WOOD08_NRM", MATERIAL_FLAG_NONE);

    // 173	leaves016
    setTextures(pScene, index++, "HP19lef3", "HP19lef3_SPEC", DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 174	sideStair01
    setTextures(pScene, index++, "muros_m", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 175	Wall01
    setTextures(pScene, index++, "Barro_2", "Barro_2_SPEC", "Barro_2_NRM", MATERIAL_FLAG_NONE);

    // 176	sideStair00
    setTextures(pScene, index++, "muros_f", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 177	TreeBasket
    setTextures(pScene, index++, "techo_01", "techo_SPEC", "techo_NRM", MATERIAL_FLAG_NONE);

    // 178	ceil01
    setTextures(pScene, index++, "WOOD08", "WOOD08_SPEC", "WOOD08_NRM", MATERIAL_FLAG_NONE);

    // 179	leaves020
    setTextures(pScene, index++, "TR14lef1", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 174	basket03
    setTextures(pScene, index++, "Maceta_A_Color", "Maceta_A_Color_SPEC", "Maceta_A_Color_NRM", MATERIAL_FLAG_NONE);

    // 175	underBasket
    setTextures(pScene, index++, "techo_01", "techo_SPEC", "techo_NRM", MATERIAL_FLAG_NONE);

    // 176	sideLeaves02:group147
    setTextures(pScene, index++, "FL29pet2", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 177	flower0341
    setTextures(pScene, index++, "FL29pet1", "FL29pet1_SPEC", "FL29pet1_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 178	stem0327
    setTextures(pScene, index++, "HP11stm", "HP11stm", "HP11stm_NRM", MATERIAL_FLAG_NONE);

    // 179	stem0329
    setTextures(pScene, index++, "HP11stm", "HP11stm", "HP11stm_NRM", MATERIAL_FLAG_NONE);

    // 180	water
    // setTextures(pScene, index++, "tela_silla_b", "tela_silla_b_SPEC", "tela_silla_b_NRM", MATERIAL_FLAG_NONE);

    // 181	sool00
    setTextures(pScene, index++, "FL29cnt2", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 182	picture09
    // setTextures(pScene, index++, "D30_Smiguel_2003_7833", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 183	picture010
    setTextures(pScene, index++, "0001_carros", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 184	treeLeaves07
    setTextures(pScene, index++, "sm_leaf_02a", "sm_leaf_02a_SPEC", "sm_leaf_02_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 185	sideStem1:group145
    setTextures(pScene, index++, "bark06mi", DEFAULT_SPEC, "bark06mi_NRM", MATERIAL_FLAG_NONE);

    // 186	chainMetal01
    setTextures(pScene, index++, "metal_viejo_2", "metal_viejo_2_SPEC", "metal_viejo_2_NRM", MATERIAL_FLAG_NONE);

    // 187	leaves05
    setTextures(pScene, index++, "FL29pet1", "FL29pet1_SPEC", "FL29pet1_NRM", MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 188	stem07
    setTextures(pScene, index++, "HP11stm", "HP11stm", "HP11stm_NRM", MATERIAL_FLAG_NONE);

    // 189	picture013
    setTextures(pScene, index++, "D30_Smiguel_2003_7768", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 190	door
    setTextures(pScene, index++, "puerta", "puerta_SPEC", "puerta_NRM", MATERIAL_FLAG_NONE);

    // 191	doorHandle
    setTextures(pScene, index++, "brushedMetal", "brushedMetal_SPEC", "brushedMetal_NRM", MATERIAL_FLAG_NONE);

    // 192	muros00
    setTextures(pScene, index++, "muros_j", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 193	picture011
    setTextures(pScene, index++, "D30_Smiguel_2003_7833", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 194	stem0326
    setTextures(pScene, index++, "HP11stm", "HP11stm", "HP11stm_NRM", MATERIAL_FLAG_NONE);

    // 195	ceilEdgeMid
    setTextures(pScene, index++, "postes_barandal_color", "postes_barandal_color_SPEC", "postes_barandal_color_NRM", MATERIAL_FLAG_NONE);

    // 196	flower0340
    setTextures(pScene, index++, "FL13pet1", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 197	sool02198
    setTextures(pScene, index++, "FL29cnt2", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 198	balconi2nd
    setTextures(pScene, index++, "balconi2nd", "balconi2nd_SPEC", "balconi2nd_NRM", MATERIAL_FLAG_NONE);

    // 199	ceilEdge
    setTextures(pScene, index++, "concrete", "concrete_SPEC", "concrete_NRM", MATERIAL_FLAG_NONE);

    // 200	floor01 flower
    setTextures(pScene, index++, "FL29pet2", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED);

    // 201	floor_Main00
    setTextures(pScene, index++, "piso_patio_exterior", "piso_patio_exterior_SPEC", "piso_patio_exterior_NRM", MATERIAL_FLAG_NONE);

    // 202	picture03
    setTextures(pScene, index++, "D30_Smiguel_2003_7833", DEFAULT_SPEC, DEFAULT_NORMAL, MATERIAL_FLAG_NONE);

    // 203	gargoyles
    setTextures(pScene, index++, "concreto_02", "concreto_02_SPEC", "concreto_02_NRM", MATERIAL_FLAG_NONE);

    // 205	letherSeat
    setTextures(pScene, index++, "silla_d_piel", "silla_d_piel_SPEC", "silla_d_piel_NRM", MATERIAL_FLAG_NONE);

    // 208	dirt
    setTextures(pScene, index++, "052terresable", "052terresable_SPEC", "052terresable_NRM", MATERIAL_FLAG_NONE);

    // 204	woodTable
    setTextures(pScene, index++, "Vigas_B", "Vigas_B_SPEC", "Vigas_B_NRM", MATERIAL_FLAG_NONE);

    // 207	woodChair
    setTextures(pScene, index++, "WOOD08", "WOOD08_SPEC", "WOOD08_NRM", MATERIAL_FLAG_NONE);

    // 90	seat00
    setTextures(pScene, index++, "Finishes.Flooring.Carpet.Loop.5", "Finishes.Flooring.Carpet.Loop.5_SPEC",
                "Finishes.Flooring.Carpet.Loop.5_NRM", MATERIAL_FLAG_NONE);

    // 206	fountainFloor
    setTextures(pScene, index++, "piso_rustico", "piso_rustico_SPEC", "piso_rustico_NRM", MATERIAL_FLAG_NONE);

    // 206	seat : polySurface5
    setTextures(pScene, index++, "tela_silla_b", "tela_silla_b_SPEC", "tela_silla_b_NRM", MATERIAL_FLAG_NONE);

    // 107	floorOutside00
    setTextures(pScene, index++, "052terresable", "052terresable_SPEC", "052terresable_NRM", MATERIAL_FLAG_NONE);

    // 133	outsideWall
    setTextures(pScene, index++, "newWall", DEFAULT_SPEC, "newWall_NRM", MATERIAL_FLAG_NONE);

    // 101	arc00
    setTextures(pScene, index++, "techo", "techo_SPEC", "techo_NRM", MATERIAL_FLAG_NONE);

    // 101	arc03
    setTextures(pScene, index++, "techo", "techo_SPEC", "techo_NRM", MATERIAL_FLAG_NONE);

    // 101	wall04
    setTextures(pScene, index++, "techo", "techo_SPEC", "techo_NRM", MATERIAL_FLAG_NONE);

    const size_t defaultAlbedoSize = strlen(DEFAULT_ALBEDO) + 1;
    const size_t defaultNormalSize = strlen(DEFAULT_NORMAL) + 1;
    const size_t defaultSpecSize = strlen(DEFAULT_SPEC) + 1;
    for (uint32_t i = index; i < pScene->geom->mDrawArgCount; i++)
    {
        pScene->materialFlags[i] = MATERIAL_FLAG_TWO_SIDED | MATERIAL_FLAG_ALPHA_TESTED;

        /*Material& m = pScene->materials[i];
        m.twoSided = true;
        m.alphaTested = true;*/

        // default textures
        pScene->textures[i] = (char*)tf_calloc(defaultAlbedoSize, sizeof(char));
        strcpy(pScene->textures[i], DEFAULT_ALBEDO);

        pScene->normalMaps[i] = (char*)tf_calloc(defaultNormalSize, sizeof(char));
        strcpy(pScene->normalMaps[i], DEFAULT_NORMAL);

        pScene->specularMaps[i] = (char*)tf_calloc(defaultSpecSize, sizeof(char));
        strcpy(pScene->specularMaps[i], DEFAULT_SPEC);
    }
}

// Loads a scene and returns a Scene object with scene information
Scene* initSanMiguel(const GeometryLoadDesc* pTemplate, SyncToken& token, bool transparentFlags)
{
    Scene* scene = (Scene*)tf_calloc(1, sizeof(Scene));

    GeometryLoadDesc loadDesc = *pTemplate;

    VertexLayout vertexLayout = {};
    if (!loadDesc.pVertexLayout)
    {
        vertexLayout.mAttribCount = 3;
        vertexLayout.mBindingCount = 3;
        vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        vertexLayout.mAttribs[0].mBinding = 0;
        vertexLayout.mAttribs[0].mLocation = 0;
        vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
        vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R16G16_SFLOAT;
        vertexLayout.mAttribs[1].mBinding = 1;
        vertexLayout.mAttribs[1].mLocation = 1;
        vertexLayout.mAttribs[2].mSemantic = SEMANTIC_NORMAL;
        vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R16G16_UNORM;
        vertexLayout.mAttribs[2].mBinding = 2;
        vertexLayout.mAttribs[2].mLocation = 2;
        loadDesc.pVertexLayout = &vertexLayout;
    }

    loadDesc.pFileName = "SanMiguel.bin";
    loadDesc.ppGeometryData = &scene->geomData;
    loadDesc.ppGeometry = &scene->geom;

    addResource(&loadDesc, &token);

    waitForToken(&token);

    scene->materialCount = scene->geom->mDrawArgCount;
    scene->materialFlags = (MaterialFlags*)tf_calloc(scene->geom->mDrawArgCount, sizeof(MaterialFlags));
    scene->textures = (char**)tf_calloc(scene->geom->mDrawArgCount, sizeof(char*));
    scene->normalMaps = (char**)tf_calloc(scene->geom->mDrawArgCount, sizeof(char*));
    scene->specularMaps = (char**)tf_calloc(scene->geom->mDrawArgCount, sizeof(char*));

    SetMaterials(scene, transparentFlags);

    return scene;
}

void exitSanMiguel(Scene* scene)
{
    for (uint32_t i = 0; i < scene->materialCount; ++i)
    {
        if (scene->textures[i])
        {
            tf_free(scene->textures[i]);
            scene->textures[i] = NULL;
        }

        if (scene->normalMaps[i])
        {
            tf_free(scene->normalMaps[i]);
            scene->normalMaps[i] = NULL;
        }

        if (scene->specularMaps[i])
        {
            tf_free(scene->specularMaps[i]);
            scene->specularMaps[i] = NULL;
        }
    }

    tf_free(scene->textures);
    tf_free(scene->normalMaps);
    tf_free(scene->specularMaps);
    tf_free(scene->materialFlags);
    tf_free(scene);
}

void createCubeBuffers(Renderer* pRenderer, Buffer** ppVertexBuffer, Buffer** ppIndexBuffer)
{
    UNREF_PARAM(pRenderer);
    // Create vertex buffer
    static float vertexData[] = {
        -1, -1, -1, 1, 1, -1, -1, 1, 1, 1, -1, 1, -1, 1, -1, 1, -1, -1, 1, 1, 1, -1, 1, 1, 1, 1, 1, 1, -1, 1, 1, 1,
    };

    BufferLoadDesc vbDesc = {};
    vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    vbDesc.mDesc.mSize = sizeof(vertexData);
    vbDesc.pData = vertexData;
    vbDesc.ppBuffer = ppVertexBuffer;
    vbDesc.mDesc.pName = "VB Desc";
    addResource(&vbDesc, NULL);

    // Create index buffer
    static constexpr uint16_t indices[6 * 6] = { 0, 1, 3, 3, 1, 2, 1, 5, 2, 2, 5, 6, 5, 4, 6, 6, 4, 7,
                                                 4, 0, 7, 7, 0, 3, 3, 2, 7, 7, 2, 6, 4, 5, 0, 0, 5, 1 };

    BufferLoadDesc ibDesc = {};
    ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
    ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    ibDesc.mDesc.mSize = sizeof(indices);
    ibDesc.pData = indices;
    ibDesc.ppBuffer = ppIndexBuffer;
    ibDesc.mDesc.pName = "IB Desc";
    addResource(&ibDesc, NULL);
}
