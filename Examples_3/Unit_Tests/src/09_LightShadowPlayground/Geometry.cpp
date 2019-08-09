/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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

#include "Geometry.h"

#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/unordered_set.h"

//#include "../../../../Common_3/ThirdParty/OpenSource/assimp/4.1.0/include/assimp/cimport.h"
//#include "../../../../Common_3/ThirdParty/OpenSource/assimp/4.1.0/include/assimp/scene.h"
//#include "../../../../Common_3/ThirdParty/OpenSource/assimp/4.1.0/include/assimp/postprocess.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Core/Compiler.h"


#include "../../../../Common_3/OS/Interfaces/IMemory.h"

#define DEFAULT_ALBEDO "Default"
#define DEFAULT_NORMAL "Default_NRM"
#define DEFAULT_SPEC "Default_SPEC"
#define DEFAULT_SPEC_TRANSPARENT "Default_SPEC_TRANS"

static void SetAlphaTestMaterials(eastl::unordered_set<eastl::string>& mats)
{
	// San Miguel
	mats.insert("lambert3");
	mats.insert("aglaonema_Leaf");
	mats.insert("brevipedunculata_Leaf1");
	mats.insert("Azalea_1_blattg1");
	mats.insert("Azalea_1_Blutenb");
	mats.insert("Azalea_1_leafcal");
	mats.insert("Azalea_2_blattg1");
	mats.insert("Azalea_2_Blutenb");
	mats.insert("Chusan_Palm_1_Leaf");
	mats.insert("Fern_1_Fan");
	mats.insert("Fern_3_Leaf");
	mats.insert("Ficus_1_Leaf1");
	mats.insert("Geranium_1_Leaf");
	mats.insert("Geranium_1_blbl1");
	mats.insert("Geranium_1_blbl2");
	mats.insert("Geranium_1_Kelchbl");
	mats.insert("Hoja_Seca_2A");
	mats.insert("Hoja_Seca_2B");
	mats.insert("Hoja_Seca_2C");
	mats.insert("Hoja_Verde_A");
	mats.insert("Hoja_Verde_B");
	mats.insert("Hojas_Rojas_top");
	mats.insert("hybrids_blossom");
	mats.insert("hybrids_Leaf");
	mats.insert("Ivy_1_Leaf");
	mats.insert("Leave_A_a");
	mats.insert("Leave_A_b");
	mats.insert("Leave_A_c");
	mats.insert("Mona_Lisa_1_Leaf1");
	mats.insert("Mona_Lisa_1_Leaf2");
	mats.insert("Mona_Lisa_2_Leaf1");
	mats.insert("Mona_Lisa_1_petal11");
	mats.insert("Mona_Lisa_1_petal12");
	mats.insert("paniceum_Leaf");
	mats.insert("Pansy_1_blblbac");
	mats.insert("Pansy_1_Leaf");
	mats.insert("Pansy_1_Leafcop");
	mats.insert("Pansy_1_Leafsma");
	mats.insert("Poinsettia_1_Leaf");
	mats.insert("Poinsettia_1_redleaf");
	mats.insert("Poinsettia_1_smallre");
	mats.insert("Rose_1_Blatt2");
	mats.insert("Rose_1_Blutenb");
	mats.insert("Rose_1_Blatt1_");
	mats.insert("Rose_1_Kelchbl");
	mats.insert("Rose_2__Blutenb");
	mats.insert("Rose_2__Kelchbl");
	mats.insert("Rose_2_Blatt1_");
	mats.insert("Rose_3_Blutenb");
	mats.insert("Rose_3_Blatt2");
	mats.insert("zebrina_Leaf");
}

static void SetTwoSidedMaterials(eastl::unordered_set<eastl::string>& mats)
{
	// San Miguel
	mats.insert("aglaonema_Leaf");
	mats.insert("brevipedunculata_Leaf1");
	mats.insert("Azalea_1_blattg1");
	mats.insert("Azalea_1_Blutenb");
	mats.insert("Azalea_1_leafcal");
	mats.insert("Azalea_2_blattg1");
	mats.insert("Azalea_2_Blutenb");
	mats.insert("Chusan_Palm_1_Leaf");
	mats.insert("Fern_1_Fan");
	mats.insert("Fern_3_Leaf");
	mats.insert("Ficus_1_Leaf1");
	mats.insert("Geranium_1_Leaf");
	mats.insert("Geranium_1_blbl1");
	mats.insert("Geranium_1_blbl2");
	mats.insert("Geranium_1_Kelchbl");
	mats.insert("Hoja_Seca_2A");
	mats.insert("Hoja_Seca_2B");
	mats.insert("Hoja_Seca_2C");
	mats.insert("Hoja_Verde_A");
	mats.insert("Hoja_Verde_B");
	mats.insert("Hojas_Rojas_top");
	mats.insert("hybrids_blossom");
	mats.insert("hybrids_Leaf");
	mats.insert("Ivy_1_Leaf");
	mats.insert("Leave_A_a");
	mats.insert("Leave_A_b");
	mats.insert("Leave_A_c");
	mats.insert("Mona_Lisa_1_Leaf1");
	mats.insert("Mona_Lisa_1_Leaf2");
	mats.insert("Mona_Lisa_2_Leaf1");
	mats.insert("Mona_Lisa_1_petal11");
	mats.insert("Mona_Lisa_1_petal12");
	mats.insert("paniceum_Leaf");
	mats.insert("Pansy_1_blblbac");
	mats.insert("Pansy_1_Leaf");
	mats.insert("Pansy_1_Leafcop");
	mats.insert("Pansy_1_Leafsma");
	mats.insert("Poinsettia_1_Leaf");
	mats.insert("Poinsettia_1_redleaf");
	mats.insert("Poinsettia_1_smallre");
	mats.insert("Rose_1_Blatt2");
	mats.insert("Rose_1_Blutenb");
	mats.insert("Rose_1_Blatt1_");
	mats.insert("Rose_1_Kelchbl");
	mats.insert("Rose_2__Blutenb");
	mats.insert("Rose_2__Kelchbl");
	mats.insert("Rose_2_Blatt1_");
	mats.insert("Rose_3_Blutenb");
	mats.insert("Rose_3_Blatt2");
	mats.insert("zebrina_Leaf");
	mats.insert("Tronco");
	mats.insert("Muros");
	mats.insert("techos");
	mats.insert("Azotea");
	mats.insert("Pared_SanMiguel_N");
	mats.insert("Pared_SanMiguel_H");
	mats.insert("Pared_SanMiguel_B");
	mats.insert("Pared_SanMiguel_G");
	mats.insert("Barandal_Detalle_Extremos");
	mats.insert("Madera_Silla");
	mats.insert("Forja_Macetas");
	mats.insert("Muro_Naranja_Escalera");
	mats.insert("Tela_Mesa_D_2");
	mats.insert("Tela_Mesa_D");
}

void setDefaultTextures(Scene* pScene, int index)
{
	Material& m = pScene->materials[index];
	m.twoSided = false;
	m.alphaTested = false;

	// default textures
	pScene->textures[index] = (char*)conf_calloc(strlen(DEFAULT_ALBEDO) + 1, sizeof(char));
	strcpy(pScene->textures[index], DEFAULT_ALBEDO);

	pScene->normalMaps[index] = (char*)conf_calloc(strlen(DEFAULT_NORMAL) + 1, sizeof(char));
	strcpy(pScene->normalMaps[index], DEFAULT_NORMAL);

	pScene->specularMaps[index] = (char*)conf_calloc(strlen(DEFAULT_SPEC) + 1, sizeof(char));
	strcpy(pScene->specularMaps[index], DEFAULT_SPEC);
}

void setTextures(Scene* pScene, int index, const char* albedo, const char* specular, const char* normal, bool twoSided, bool alpha)
{
	Material& m = pScene->materials[index];
	m.twoSided = twoSided;
	m.alphaTested = alpha;

	// default textures
	pScene->textures[index] = (char*)conf_calloc(strlen(albedo) + 1, sizeof(char));
	strcpy(pScene->textures[index], albedo);

	pScene->specularMaps[index] = (char*)conf_calloc(strlen(specular) + 1, sizeof(char));
	strcpy(pScene->specularMaps[index], specular);

	pScene->normalMaps[index] = (char*)conf_calloc(strlen(normal) + 1, sizeof(char));
	strcpy(pScene->normalMaps[index], normal);
}

static void SetMaterials(Scene* pScene)
{
	int index = 0;

	// 0	flags
	setTextures(pScene, index++, "ForgeFlags", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	// 0	arc034
	setTextures(pScene, index++, "arco_frente", "arco_frente_SPEC", "arco_frente_NRM", false, false);

	// 1	hugeBackWall00
	setDefaultTextures(pScene, index++);

	// 2	leaves0379
	setTextures(pScene, index++, "citrus_limon_leaf", "citrus_limon_leaf_SPEC", "citrus_limon_leaf_NRM", true, true);

	// 3	hugeWallFront00
	setTextures(pScene, index++, "Barro_2", "Barro_2_SPEC", "Barro_2_NRM", false, false);

	// 4	floor2nd00
	setDefaultTextures(pScene, index++);

	// 5	leaves013
	setTextures(pScene, index++, "HP01lef2", "HP01lef2_SPEC", DEFAULT_NORMAL, true, true);

	// 6	leaves014
	setTextures(pScene, index++, "FL13lef4", "FL13lef4_SPEC", DEFAULT_NORMAL, true, true);

	// 7	metalHat
	setTextures(pScene, index++, "metal_viejo_2", "metal_viejo_2_SPEC", "metal_viejo_2_NRM", false, false);

	// 8	metal010
	setTextures(pScene, index++, "Fierro_A", "Fierro_A_SPEC", "Fierro_A_NRM", false, false);

	// 9	floor03
	setTextures(pScene, index++, "piso_rustico", "piso_rustico_SPEC", "piso_rustico_NRM", false, false);

	// 10	wall03
	setTextures(pScene, index++, "muros_b2", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	// 11	flower0342
	setTextures(pScene, index++, "FL13pet1", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	// 12	stem015
	setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	// 13	stem014
	setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	// 14	stem016
	setTextures(pScene, index++, "FL19stm", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	// 15	leaves0377
	setTextures(pScene, index++, "FL17lef2", "FL17lef2_SPEC", "FL17lef2_NRM", true, true);

	// 16	plate00
	setTextures(pScene, index++, "plato_a", "plato_a_SPEC", "plato_a_NRM", false, false);

	// 17	sideLeaves1:group146
	setTextures(pScene, index++, "l37-upper", "l33-upper_SPEC", "l37-upper_NRM", true, true);

	// 18	leaves0378
	setTextures(pScene, index++, "FL17lef2", "FL17lef2_SPEC", "FL17lef2_NRM", true, true);

	// 19	treeBranch
	setTextures(pScene, index++, "bark06mi", DEFAULT_SPEC, "bark06mi_NRM", false, false);

	// 20	basket02
	setTextures(pScene, index++, "Maceta_A_Color", "Maceta_A_Color_SPEC", "Maceta_A_Color_NRM", false, false);

	// 21	leaves026
	setTextures(pScene, index++, "FL19lef2", "FL19lef2_SPEC", "FL19lef2_NRM", true, true);

	// 22	chainMetal00
	setTextures(pScene, index++, "metal_viejo_2", "metal_viejo_2_SPEC", "metal_viejo_2_NRM", false, false);

	// 23	cloth01
	setTextures(pScene, index++, "tela_mesa_b", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	// 24	wood01
	setTextures(pScene, index++, "WOOD08", "WOOD08_SPEC", "WOOD08_NRM", false, false);

	// 25	picture07
	setTextures(pScene, index++, "D30_Smiguel_2003_7833", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	// 26	smallBackWall00
	setTextures(pScene, index++, "muros_d", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	// 27	treeLeaves00
	setTextures(pScene, index++, "sm_leaf_02a", "sm_leaf_02a_SPEC", "sm_leaf_02_NRM", true, true);

	//28	stem0328
	setTextures(pScene, index++, "ampelopsis_brevipedunculata_bark", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//29	salt00
	setTextures(pScene, index++, "brushedMetal", "brushedMetal_SPEC", "brushedMetal_NRM", false, false);

	//30	leaves00
	setTextures(pScene, index++, "FL16lef3", "FL16lef1_SPEC", "FL16lef3_NRM", true, true);

	//31	metal07
	setTextures(pScene, index++, "brushedMetal", "brushedMetal_SPEC", "brushedMetal_NRM", false, false);

	//32	underLeaves02
	setTextures(pScene, index++, "l37-upper", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//33	rose00
	setTextures(pScene, index++, "FL13pet2", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//34	cup
	setTextures(pScene, index++, "tela_mesa_b", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//35	leaves012
	setTextures(pScene, index++, "HP17lef2", "HP17lef2_SPEC", DEFAULT_NORMAL, true, true);

	//36	cloth00
	setTextures(pScene, index++, "tela_mesa_d", "tela_mesa_d_SPEC", DEFAULT_NORMAL, false, false);

	//37	cloth02
	setTextures(pScene, index++, "tela_blanca", "tela_blanca_SPEC", "tela_blanca_NRM", false, false);

	//38	underLeaves01
	setTextures(pScene, index++, "l04-upper", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//39	stem06
	setTextures(pScene, index++, "HP19stm1", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//40	leaves02
	setTextures(pScene, index++, "HP17lef2", "HP17lef2_SPEC", "HP17lef2_NRM", true, true);

	//41	leaves015
	setTextures(pScene, index++, "HP13lef1", "HP13lef1_SPEC", "hp13lef1_NRM", true, true);

	//42	flower01
	setTextures(pScene, index++, "FL19pe15", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//43	stem010
	setTextures(pScene, index++, "FL29twg", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//44	leaves022
	setTextures(pScene, index++, "FL16lef3", "FL16lef1_SPEC", "FL16lef3_NRM", true, true);

	//45	leaves024
	setTextures(pScene, index++, "HP17lef2", "HP17lef2_SPEC", "HP17lef2_NRM", true, true);

	//46	leaves021
	setTextures(pScene, index++, "FL16pet1", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//47	basket04
	setTextures(pScene, index++, "Maceta_C_Color", "Maceta_C_Color_SPEC", "Maceta_C_NRM", false, false);

	//48	door00
	setTextures(pScene, index++, "WOOD08", "WOOD08_SPEC", "WOOD08_NRM", false, false);

	//49	leaves023
	setTextures(pScene, index++, "HP13lef1", "HP13lef1_SPEC", "hp13lef1_NRM", true, true);

	//50	underCeil
	setTextures(pScene, index++, "techo", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//51	leaves08
	setTextures(pScene, index++, "TR02lef5", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//52	wood02
	setTextures(pScene, index++, "wood.3.Bubinga", "WOOD08_SPEC", "WOOD08_NRM", false, false);

	//53	wall02
	setTextures(pScene, index++, "muros_b1", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//54	leaves09
	setTextures(pScene, index++, "HP19lef2", "HP19lef2_SPEC", "HP19lef2_NRM", true, true);

	//55	stem013
	setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//56	metal07
	setTextures(pScene, index++, "Fierro_A", "Fierro_A_SPEC", "Fierro_A_NRM", false, false);

	//57	paper
	setTextures(pScene, index++, "individual_b", "individual_b_SPEC", "individual_b_NRM", false, false);

	//58	lightBulb
	setTextures(pScene, index++, DEFAULT_ALBEDO, DEFAULT_SPEC_TRANSPARENT, DEFAULT_NORMAL, false, false);

	//59	giwa
	setTextures(pScene, index++, "Barro_2", "Barro_2_SPEC", "Barro_2_NRM", false, false);

	//60	flower0304
	setTextures(pScene, index++, "HP01pet1", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//61	picture02
	setTextures(pScene, index++, "D30_Smiguel_2003_7815", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//62	picture01
	setTextures(pScene, index++, "D30_Smiguel_2003_7785", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//63	metal09
	setTextures(pScene, index++, "rust_a1", "rust_a1_SPEC", "rust_a1_NRM", false, false);

	//64	arc_floor00
	setTextures(
		pScene, index++, "arcos_lisos_3_color_1", "arcos_lisos_3_color_1_SPEC", "arcos_lisos_3_color_1_NRM", false, false);

	//65	hugeWallLeft
	setTextures(pScene, index++, "muros_n", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//66	cap
	setTextures(pScene, index++, "muros_q_patio2", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//67	flower00
	setTextures(pScene, index++, "HP01pet3", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//68	stem0369
	setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//69	ceil_arc_cap
	setTextures(pScene, index++, "moldura_techo", "moldura_techo_SPEC", "moldura_techo_NRM", false, false);

	//70	stem08
	setTextures(pScene, index++, "FL29twg", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//71	wood03
	setTextures(pScene, index++, "WOOD08", "WOOD08_SPEC", "WOOD08_NRM", false, false);

	//72	wall01
	setTextures(pScene, index++, "muros_l", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//73	hugeBackWall01
	setTextures(pScene, index++, "muros_h", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//74	picture06
	setTextures(pScene, index++, "0001_carros", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//75	leaves01
	setTextures(pScene, index++, "aglaonema_leaf", "aglaonema_leaf_SPEC", DEFAULT_NORMAL, true, true);

	//76	sool00
	setTextures(pScene, index++, "FL29cnt2", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//77	capital
	setTextures(pScene, index++, "detmoldura_06_color", "detmoldura_06_color_SPEC", "detmoldura_06_color_NRM", false, false);

	//78	underCeil00
	setTextures(pScene, index++, "cantera_naranja_liso", "cantera_naranja_liso_SPEC", "cantera_naranja_liso_NRM", false, false);

	//79	leaves06
	setTextures(pScene, index++, "citrus_limon_leaf", "citrus_limon_leaf_SPEC", "citrus_limon_leaf_NRM", true, true);

	//80	group21
	setTextures(pScene, index++, "muros_c2", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//81	leaves07
	setTextures(pScene, index++, "FL29lef", "FL29lef_SPEC", "FL29lef_NRM", true, true);

	//83	underLeaves00
	setTextures(pScene, index++, "FL13pet2", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//84	flower0339
	setTextures(pScene, index++, "FL19pe15", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//85	sool01
	setTextures(pScene, index++, "FL29cnt2", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//86	picture05
	setTextures(pScene, index++, "06-30-1997", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//87	stem05
	setTextures(pScene, index++, "FL29twg", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//88	stem04
	setTextures(pScene, index++, "FL19stm", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//89	foundtain02
	setTextures(pScene, index++, "techo_01", "techo_SPEC", "techo_NRM", false, false);

	//91	foundtain01
	setTextures(pScene, index++, "techo_01", "techo_SPEC", "techo_NRM", false, false);

	//92	pictureEdge
	setTextures(pScene, index++, "wood.3.Bubinga", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//93	SOY
	setTextures(pScene, index++, "PITTED", "madera_marcos_SPEC", DEFAULT_NORMAL, false, false);

	//94	metal06
	setTextures(pScene, index++, "rust_a1", "rust_a1_SPEC", "rust_a1_NRM", false, false);

	//95	picture15
	setTextures(pScene, index++, "D30_Smiguel_2003_7758", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//96	floor00
	setTextures(pScene, index++, "piso_rustico", "piso_rustico_SPEC", "piso_rustico_NRM", false, false);

	//97	leaves017
	setTextures(pScene, index++, "FL29lef", "FL29lef_SPEC", "FL29pet1_NRM", true, true);

	//98	hugeWall00
	setTextures(pScene, index++, "muros_a", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//99	picture14
	setTextures(pScene, index++, "tapa_talabera", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//100	stem012
	setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//102	stem003
	setTextures(pScene, index++, "FL19stm", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//103	leaves019
	setTextures(pScene, index++, "HP11lef2", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//104	leaves018
	setTextures(pScene, index++, "FL19lef4", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//105	metal00
	setTextures(pScene, index++, "rust_a1", "rust_a1_SPEC", "rust_a1_NRM", true, true);

	//106	stem02
	setTextures(pScene, index++, "FL19stm", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//107	floorOutside00
	setTextures(pScene, index++, "052terresable", "052terresable_SPEC", "052terresable_NRM", false, false);

	//108	capitel
	setTextures(pScene, index++, "detmoldura_06_color", "detmoldura_06_color_SPEC", "detmoldura_06_color_NRM", false, false);

	//109	pillar00
	setTextures(pScene, index++, "concrete", "concrete_SPEC", "concrete_NRM", false, false);

	//110	windowStairCase
	setTextures(pScene, index++, "concreto_02", "concreto_02_SPEC", "concreto_02_NRM", false, false);

	//111	stem01
	setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//112	doorCover
	setTextures(pScene, index++, "marco_puerta_1", "marco_puerta_1_SPEC", "marco_puerta_1_NRM", false, false);

	//113	Wall00
	setTextures(pScene, index++, "muros_g", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//114	stair00
	setTextures(pScene, index++, "escalera_color", "escalera_color_SPEC", "escalera_color_NRM", false, false);

	//115	treeLeaves03
	setTextures(pScene, index++, "sm_leaf_02b", "sm_leaf_02b_SPEC", "sm_leaf_02b_NRM", true, true);

	//116	trans00
	setTextures(pScene, index++, DEFAULT_ALBEDO, DEFAULT_SPEC_TRANSPARENT, DEFAULT_NORMAL, false, false);

	//117	gagoyle
	setTextures(pScene, index++, "concreto_02", "concreto_02_SPEC", "concreto_02_NRM", false, false);

	//118	picture00
	setTextures(pScene, index++, "D30_Smiguel_2003_7843", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//119	ceils
	setTextures(pScene, index++, "techo", "techo_SPEC", "techo_NRM", false, false);

	//120	flower0338
	setTextures(pScene, index++, "HP01pet3", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//121	stem09
	setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//122	basket01
	setTextures(pScene, index++, "Maceta_B_Color", "Maceta_C_Color_SPEC", "Maceta_B_NRM", false, false);

	//123	basket00
	setTextures(pScene, index++, "Maceta_D2_Color_0", "Maceta_C_Color_SPEC", "Maceta_B_NRM", false, false);

	//124	metal01
	setTextures(pScene, index++, "Fierro_A", "Fierro_A_SPEC", "Fierro_A_NRM", false, false);

	//125	sideFloor2nd
	setTextures(pScene, index++, "piso_rustico", "piso_rustico_SPEC", "piso_rustico_NRM", false, false);

	//126	wall00
	setTextures(pScene, index++, "muros_q_patio2", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//127	picture12
	setTextures(pScene, index++, "D30_Smiguel_2003_7843", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//128	flower0343
	setTextures(pScene, index++, "FL19pe16", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//129	leaves0300
	setTextures(pScene, index++, "FL29lef", "FL29lef_SPEC", "FL29lef_NRM", true, true);

	//130	sool0337
	setTextures(pScene, index++, "FL29cnt2", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//131	arc01
	setTextures(pScene, index++, "arcos_lisos_2_color", "arcos_lisos_2_color_SPEC", "arcos_lisos_2_color_NRM", false, false);

	//132	arc00
	//setTextures(pScene, index++, "techo", "techo_SPEC", "techo_NRM", false, false);

	//133	outsideWall
	setTextures(pScene, index++, "newWall", DEFAULT_SPEC, "newWall_NRM", false, false);

	//134	metal03
	setTextures(pScene, index++, "rust_detalle", "rust_detalle_SPEC", "rust_detalle_NRM", false, false);

	//135	muros01
	setTextures(pScene, index++, "muros_k", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//136	wall05
	setTextures(pScene, index++, "muros_e", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//137	metal04
	setTextures(pScene, index++, "Fierro_A", "Fierro_A_SPEC", "Fierro_A_NRM", false, false);

	//138	metal02 mmm
	setTextures(pScene, index++, "rust_detalle", "rust_detalle_SPEC", "rust_detalle_NRM", false, false);

	//139	arc03
	//setTextures(pScene, index++, "techo", "techo_SPEC", "techo_NRM", false, false);

	//140	metal05
	setTextures(pScene, index++, "rust_detalle", "rust_detalle_SPEC", "rust_detalle_NRM", false, false);

	//141	metal0104
	setTextures(pScene, index++, "Fierro_A", "Fierro_A_SPEC", "Fierro_A_NRM", false, false);

	//142	sool02199
	setTextures(pScene, index++, "FL29cnt2", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//143	stem01
	setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//143	leaves0380
	setTextures(pScene, index++, "HP01lef2", "HP01lef2_SPEC", DEFAULT_NORMAL, true, true);

	//143	stem0330
	setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//144	stem0368
	setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//145	leaves025
	setTextures(pScene, index++, "FL29pet2", "FL29lef_SPEC", "FL29pet2_NRM", true, true);

	//146	utencil
	setTextures(pScene, index++, "brushedMetal", "brushedMetal_SPEC", "brushedMetal_NRM", false, false);

	//147	door01
	setTextures(pScene, index++, "wood.3.Bubinga", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//148	treeLeaves02
	setTextures(pScene, index++, "sm_leaf_seca_02b", "sm_leaf_02b_SPEC", "sm_leaf_02b_NRM", true, true);

	//149	stem0330
	setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//150	leaves027
	setTextures(pScene, index++, "FL29pet2", "FL29lef_SPEC", "FL29pet2_NRM", true, true);

	//151	treeLeaves01
	setTextures(pScene, index++, "sm_leaf_02a", "sm_leaf_02b_SPEC", "sm_leaf_02b_NRM", true, true);

	//152	stem0388
	setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//153	stem0291
	setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//154	flower0344
	setTextures(pScene, index++, "FL17pet1", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//155	treeLeaves06
	setTextures(pScene, index++, "FL29pet2", "FL29pet2_SPEC", "FL29pet2_NRM", true, true);

	//156	treeLeaves05
	setTextures(pScene, index++, "l37-upper", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//157	stem0386
	setTextures(pScene, index++, "FL24stm", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//158	picture04
	setTextures(pScene, index++, "06-30-1997", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//159	sool0361
	setTextures(pScene, index++, "FL29cnt1", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//160	foundtain00
	setTextures(pScene, index++, "techo_01", "techo_SPEC", "techo_NRM", false, false);

	//161	leaves011
	setTextures(pScene, index++, "FL17lef2", "FL17lef2_SPEC", "FL17lef2_NRM", true, true);

	//162	underBalconi
	setTextures(pScene, index++, "techo", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//163	leaves0381
	setTextures(pScene, index++, "TR02lef5", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//164	leaves010
	setTextures(pScene, index++, "TR14lef1", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//165	group35
	setTextures(pScene, index++, "muros_q4", "muros_q4_SPEC", "muros_q4_NRM", true, true);

	//166	treeLeaves04
	setTextures(pScene, index++, "sm_leaf_02a", "sm_leaf_02a_SPEC", "sm_leaf_02_NRM", true, true);

	//167	picture08
	setTextures(pScene, index++, "D30_Smiguel_2003_7833", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//168	treeLeaves08
	setTextures(pScene, index++, "l04-upper", "l04-upper_SPEC", "l04-upper_NRM", true, true);

	//169	floor02
	setTextures(pScene, index++, DEFAULT_ALBEDO, DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//170	leaves04
	setTextures(pScene, index++, "HP19lef3", "HP19lef3_SPEC", "HP19lef3_NRM", true, true);

	//171	bat00
	setTextures(pScene, index++, "madera_marcos", "madera_marcos_SPEC", "madera_marcos_NRM", false, false);

	//172	ceil00
	setTextures(pScene, index++, "WOOD08", "WOOD08_SPEC", "WOOD08_NRM", false, false);

	//173	leaves016
	setTextures(pScene, index++, "HP19lef3", "HP19lef3_SPEC", DEFAULT_NORMAL, true, true);

	//174	sideStair01
	setTextures(pScene, index++, "muros_m", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//175	Wall01
	setTextures(pScene, index++, "Barro_2", "Barro_2_SPEC", "Barro_2_NRM", false, false);

	//176	sideStair00
	setTextures(pScene, index++, "muros_f", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//177	TreeBasket
	setTextures(pScene, index++, "techo_01", "techo_SPEC", "techo_NRM", false, false);

	//178	ceil01
	setTextures(pScene, index++, "WOOD08", "WOOD08_SPEC", "WOOD08_NRM", false, false);

	//179	leaves020
	setTextures(pScene, index++, "TR14lef1", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//174	basket03
	setTextures(pScene, index++, "Maceta_A_Color", "Maceta_A_Color_SPEC", "Maceta_A_Color_NRM", false, false);

	//175	underBasket
	setTextures(pScene, index++, "techo_01", "techo_SPEC", "techo_NRM", false, false);

	//176	sideLeaves02:group147
	setTextures(pScene, index++, "FL29pet2", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//177	flower0341
	setTextures(pScene, index++, "FL29pet1", "FL29pet1_SPEC", "FL29pet1_NRM", true, true);

	//178	stem0327
	setTextures(pScene, index++, "HP11stm", "HP11stm", "HP11stm_NRM", false, false);

	//179	stem0329
	setTextures(pScene, index++, "HP11stm", "HP11stm", "HP11stm_NRM", false, false);

	//180	water
	//setTextures(pScene, index++, "tela_silla_b", "tela_silla_b_SPEC", "tela_silla_b_NRM", false, false);

	//181	sool00
	setTextures(pScene, index++, "FL29cnt2", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//182	picture09
	//setTextures(pScene, index++, "D30_Smiguel_2003_7833", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//183	picture010
	setTextures(pScene, index++, "0001_carros", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//184	treeLeaves07
	setTextures(pScene, index++, "sm_leaf_02a", "sm_leaf_02a_SPEC", "sm_leaf_02_NRM", true, true);

	//185	sideStem1:group145
	setTextures(pScene, index++, "bark06mi", DEFAULT_SPEC, "bark06mi_NRM", false, false);

	//186	chainMetal01
	setTextures(pScene, index++, "metal_viejo_2", "metal_viejo_2_SPEC", "metal_viejo_2_NRM", false, false);

	//187	leaves05
	setTextures(pScene, index++, "FL29pet1", "FL29pet1_SPEC", "FL29pet1_NRM", true, true);

	//188	stem07
	setTextures(pScene, index++, "HP11stm", "HP11stm", "HP11stm_NRM", false, false);

	//189	picture013
	setTextures(pScene, index++, "D30_Smiguel_2003_7768", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//190	door
	setTextures(pScene, index++, "puerta", "puerta_SPEC", "puerta_NRM", false, false);

	//191	doorHandle
	setTextures(pScene, index++, "brushedMetal", "brushedMetal_SPEC", "brushedMetal_NRM", false, false);

	//192	muros00
	setTextures(pScene, index++, "muros_j", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//193	picture011
	setTextures(pScene, index++, "D30_Smiguel_2003_7833", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//194	stem0326
	setTextures(pScene, index++, "HP11stm", "HP11stm", "HP11stm_NRM", false, false);

	//195	ceilEdgeMid
	setTextures(
		pScene, index++, "postes_barandal_color", "postes_barandal_color_SPEC", "postes_barandal_color_NRM", false, false);

	//196	flower0340
	setTextures(pScene, index++, "FL13pet1", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//197	sool02198
	setTextures(pScene, index++, "FL29cnt2", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//198	balconi2nd
	setTextures(pScene, index++, "balconi2nd", "balconi2nd_SPEC", "balconi2nd_NRM", false, false);

	//199	ceilEdge
	setTextures(pScene, index++, "concrete", "concrete_SPEC", "concrete_NRM", false, false);

	//200	floor01 flower
	setTextures(pScene, index++, "FL29pet2", DEFAULT_SPEC, DEFAULT_NORMAL, true, true);

	//201	floor_Main00
	setTextures(pScene, index++, "piso_patio_exterior", "piso_patio_exterior_SPEC", "piso_patio_exterior_NRM", false, false);

	//202	picture03
	setTextures(pScene, index++, "D30_Smiguel_2003_7833", DEFAULT_SPEC, DEFAULT_NORMAL, false, false);

	//203	gargoyles
	setTextures(pScene, index++, "concreto_02", "concreto_02_SPEC", "concreto_02_NRM", false, false);

	//205	letherSeat
	setTextures(pScene, index++, "silla_d_piel", "silla_d_piel_SPEC", "silla_d_piel_NRM", false, false);

	//208	dirt
	setTextures(pScene, index++, "052terresable", "052terresable_SPEC", "052terresable_NRM", false, false);

	//204	woodTable
	setTextures(pScene, index++, "Vigas_B", "Vigas_B_SPEC", "Vigas_B_NRM", false, false);

	//207	woodChair
	setTextures(pScene, index++, "WOOD08", "WOOD08_SPEC", "WOOD08_NRM", false, false);

	//90	seat00
	setTextures(
		pScene, index++, "Finishes.Flooring.Carpet.Loop.5", "Finishes.Flooring.Carpet.Loop.5_SPEC",
		"Finishes.Flooring.Carpet.Loop.5_NRM", false, false);

	//206	fountainFloor
	setTextures(pScene, index++, "piso_rustico", "piso_rustico_SPEC", "piso_rustico_NRM", false, false);

	//206	seat : polySurface5
	setTextures(pScene, index++, "tela_silla_b", "tela_silla_b_SPEC", "tela_silla_b_NRM", false, false);

	//107	floorOutside00
	setTextures(pScene, index++, "052terresable", "052terresable_SPEC", "052terresable_NRM", false, false);

	//133	outsideWall
	setTextures(pScene, index++, "newWall", DEFAULT_SPEC, "newWall_NRM", false, false);

	//101	arc00
	setTextures(pScene, index++, "techo", "techo_SPEC", "techo_NRM", false, false);

	//101	arc03
	setTextures(pScene, index++, "techo", "techo_SPEC", "techo_NRM", false, false);

	//101	wall04
	setTextures(pScene, index++, "techo", "techo_SPEC", "techo_NRM", false, false);

	for (uint32_t i = index; i < pScene->numMaterials; i++)
	{
		Material& m = pScene->materials[i];
		m.twoSided = true;
		m.alphaTested = true;

		// default textures
		pScene->textures[i] = (char*)conf_calloc(strlen(DEFAULT_ALBEDO) + 1, sizeof(char));
		strcpy(pScene->textures[i], DEFAULT_ALBEDO);

		pScene->normalMaps[i] = (char*)conf_calloc(strlen(DEFAULT_NORMAL) + 1, sizeof(char));
		strcpy(pScene->normalMaps[i], DEFAULT_NORMAL);

		pScene->specularMaps[i] = (char*)conf_calloc(strlen(DEFAULT_SPEC) + 1, sizeof(char));
		strcpy(pScene->specularMaps[i], DEFAULT_SPEC);
	}
}


#if !defined(METAL)

static inline float2 abs(const float2& v) { return float2(fabsf(v.getX()), fabsf(v.getY())); }
static inline float2 subtract(const float2& v, const float2& w) { return float2(v.getX() - w.getX(), v.getY() - w.getY()); }
static inline float2 step(const float2& y, const float2& x)
{
	return float2(x.getX() >= y.getX() ? 1.f : 0.f, x.getY() >= y.getY() ? 1.f : 0.f);
}
static inline float2 mulPerElem(const float2& v, float f) { return float2(v.getX() * f, v.getY() * f); }
static inline float2 mulPerElem(const float2& v, const float2& w) { return float2(v.getX() * w.getX(), v.getY() * w.getY()); }
static inline float2 sumPerElem(const float2& v, const float2& w) { return float2(v.getX() + w.getX(), v.getY() + w.getY()); }
static inline float2 sign_not_zero(const float2& v) { return subtract(mulPerElem(step(float2(0, 0), v), 2.0), float2(1, 1)); }
static inline uint   packSnorm2x16(const float2& v)
{
	uint x = (uint)round(clamp(v.getX(), -1, 1) * 32767.0f);
	uint y = (uint)round(clamp(v.getY(), -1, 1) * 32767.0f);
	return ((uint)0x0000FFFF & x) | ((y << 16) & (uint)0xFFFF0000);
}
static inline uint packUnorm2x16(const float2& v)
{
	uint x = (uint)round(clamp(v.getX(), 0, 1) * 65535.0f);
	uint y = (uint)round(clamp(v.getY(), 0, 1) * 65535.0f);
	return ((uint)0x0000FFFF & x) | ((y << 16) & (uint)0xFFFF0000);
}

#define F16_EXPONENT_BITS 0x1F
#define F16_EXPONENT_SHIFT 10
#define F16_EXPONENT_BIAS 15
#define F16_MANTISSA_BITS 0x3ff
#define F16_MANTISSA_SHIFT (23 - F16_EXPONENT_SHIFT)
#define F16_MAX_EXPONENT (F16_EXPONENT_BITS << F16_EXPONENT_SHIFT)

static inline unsigned short F32toF16(float val)
{
	uint           f32 = (*(uint*)&val);
	unsigned short f16 = 0;
	/* Decode IEEE 754 little-endian 32-bit floating-point value */
	int sign = (f32 >> 16) & 0x8000;
	/* Map exponent to the range [-127,128] */
	int exponent = ((f32 >> 23) & 0xff) - 127;
	int mantissa = f32 & 0x007fffff;
	if (exponent == 128)
	{ /* Infinity or NaN */
		f16 = (unsigned short)(sign | F16_MAX_EXPONENT);
		if (mantissa)
			f16 |= (mantissa & F16_MANTISSA_BITS);
	}
	else if (exponent > 15)
	{ /* Overflow - flush to Infinity */
		f16 = (unsigned short)(sign | F16_MAX_EXPONENT);
	}
	else if (exponent > -15)
	{ /* Representable value */
		exponent += F16_EXPONENT_BIAS;
		mantissa >>= F16_MANTISSA_SHIFT;
		f16 = (unsigned short)(sign | exponent << F16_EXPONENT_SHIFT | mantissa);
	}
	else
	{
		f16 = (unsigned short)sign;
	}
	return f16;
}
static inline uint pack2Floats(float2 f) { return (F32toF16(f.getX()) & 0x0000FFFF) | ((F32toF16(f.getY()) << 16) & 0xFFFF0000); }

static inline float2 normalize(const float2& vec)
{
	float lenSqr = vec.getX() * vec.getX() + vec.getY() * vec.getY();
	float lenInv = (1.0f / sqrtf(lenSqr));
	return float2(vec.getX() * lenInv, vec.getY() * lenInv);
}

static inline float OctWrap(float v, float w) { return (1.0f - abs(w)) * (v >= 0.0f ? 1.0f : -1.0f); }

static inline uint encodeDir(const float3& n)
{
	float  absLength = (abs(n.getX()) + abs(n.getY()) + abs(n.getZ()));
	float3 enc;
	enc.setX(n.getX() / absLength);
	enc.setY(n.getY() / absLength);
	enc.setZ(n.getZ() / absLength);

	if (enc.getZ() < 0)
	{
		float oldX = enc.getX();
		enc.setX(OctWrap(enc.getX(), enc.getY()));
		enc.setY(OctWrap(enc.getY(), oldX));
	}
	enc.setX(enc.getX() * 0.5f + 0.5f);
	enc.setY(enc.getY() * 0.5f + 0.5f);

	return packUnorm2x16(float2(enc.getX(), enc.getY()));
}
#endif

static const char* gModelVersion = "1.01";

// Loads a scene using ASSIMP and returns a Scene object with scene information
Scene* loadScene(const char* fileName, float scale, float offsetX, float offsetY, float offsetZ)
{
#if TARGET_IOS
	NSString* fileUrl = [[NSBundle mainBundle] pathForResource:[NSString stringWithUTF8String : fileName] ofType : @""];
	fileName = [fileUrl fileSystemRepresentation];
#endif

#if 1
	Scene* scene = (Scene*)conf_calloc(1, sizeof(Scene));

	eastl::string cachedModelFileName = eastl::string(fileName) + ".cached";
	File cached = {};
	eastl::string modelVersion;
	if (cached.Open(cachedModelFileName, FileMode::FM_ReadBinary, FSR_Absolute))
		modelVersion = cached.ReadFileID();

	if (modelVersion == gModelVersion)
	{
		scene->numMeshes = cached.ReadUInt();
		scene->totalVertices = cached.ReadUInt();
		scene->totalTriangles = cached.ReadUInt();

		scene->meshes = (MeshIn*)conf_calloc(scene->numMeshes, sizeof(MeshIn));
		cached.Read(scene->meshes, scene->numMeshes * sizeof(MeshIn));

		scene->indices = eastl::vector<uint32_t>(scene->totalTriangles * 3);
		cached.Read(scene->indices.data(), scene->totalTriangles * 3 * sizeof(uint32_t));

		scene->positions = eastl::vector< SceneVertexPos>(scene->totalVertices);
		cached.Read(scene->positions.data(), scene->totalVertices * sizeof(SceneVertexPos));

		scene->texCoords = eastl::vector<SceneVertexTexCoord>(scene->totalVertices);
		cached.Read(scene->texCoords.data(), scene->totalVertices * sizeof(SceneVertexTexCoord));

		scene->normals = eastl::vector<SceneVertexNormal>(scene->totalVertices);
		cached.Read(scene->normals.data(), scene->totalVertices * sizeof(SceneVertexNormal));

		scene->tangents = eastl::vector<SceneVertexTangent>(scene->totalVertices);
		cached.Read(scene->tangents.data(), scene->totalVertices * sizeof(SceneVertexTangent));

		cached.Close();
	}
	else
	{
		HiresTimer timer = {};
		AssimpImporter        importer;
		AssimpImporter::Model model;
		importer.ImportModel(fileName, &model);
		LOGF(LogLevel::eINFO, "Assimp Load %f ms", timer.GetUSec(true) / 1000.0f);

		eastl::vector<SceneVertexPos> positions;
		eastl::vector<float2>         texcoords;
		eastl::vector<float3>         normals;
		eastl::vector<float3>         tangents;
		eastl::vector<uint32_t>       indices;

		uint32_t accIndex_ = 0;

		for (int i = 0; i < model.mMeshArray.size(); i++)
		{
			AssimpImporter::Mesh mesh = model.mMeshArray[i];

			for (int j = 0; j < mesh.mPositions.size(); j++)
			{
				SceneVertexPos tempPos;
				tempPos.x = mesh.mPositions[j].getX() * scale + offsetX;
				tempPos.y = mesh.mPositions[j].getY() * scale + offsetY;
				tempPos.z = mesh.mPositions[j].getZ() * scale + offsetZ;

				positions.push_back(tempPos);
			}

			for (int j = 0; j < mesh.mUvs.size(); j++)
			{
				float2 tempTex;
				tempTex.x = mesh.mUvs[j].getX();
				tempTex.y = 1.0f - mesh.mUvs[j].getY();

				texcoords.push_back(tempTex);
			}

			for (int j = 0; j < mesh.mNormals.size(); j++)
			{
				float3 tempNorm;
				tempNorm.x = mesh.mNormals[j].getX();
				tempNorm.y = mesh.mNormals[j].getY();
				tempNorm.z = mesh.mNormals[j].getZ();

				normals.push_back(tempNorm);
			}

			for (int j = 0; j < mesh.mTangents.size(); j++)
			{
				float3 tempTangent;
				tempTangent.x = mesh.mTangents[j].getX();
				tempTangent.y = mesh.mTangents[j].getY();
				tempTangent.z = mesh.mTangents[j].getZ();

				tangents.push_back(tempTangent);
			}

			for (int j = 0; j < mesh.mIndices.size(); j++)
			{
				int indd = mesh.mIndices[j];

				if (indd < 0)
					indices.push_back(accIndex_);
				else
					indices.push_back(indd + accIndex_);
			}

			accIndex_ += (uint32_t)mesh.mPositions.size();
		}

		LOGF(LogLevel::eINFO, "Generate Vertex Data %f ms", timer.GetUSec(true) / 1000.0f);

		scene->numMeshes = (uint32_t)model.mMeshArray.size();
		scene->totalVertices = (uint32_t)positions.size();
		scene->totalTriangles = (uint32_t)indices.size() / 3;

		scene->meshes = (MeshIn*)conf_calloc(scene->numMeshes, sizeof(MeshIn));

		scene->indices = indices;
		scene->positions = positions;

		scene->texCoords = eastl::vector<SceneVertexTexCoord>(scene->totalVertices, SceneVertexTexCoord{ 0 });
		scene->normals = eastl::vector<SceneVertexNormal>(scene->totalVertices, SceneVertexNormal{ 0 });
		scene->tangents = eastl::vector<SceneVertexTangent>(scene->totalVertices, SceneVertexTangent{ 0 });

		for (uint32_t v = 0; v < scene->totalVertices; v++)
		{
			const float3& normal = normals[v];
			const float3& tangent = tangents[v];
			const float2& tc = texcoords[v];

#if defined(METAL) || defined(__linux__)
			scene->normals[v].nx = normal.x;
			scene->normals[v].ny = normal.y;
			scene->normals[v].nz = normal.z;

			scene->tangents[v].tx = tangent.x;
			scene->tangents[v].ty = tangent.y;
			scene->tangents[v].tz = tangent.z;

			scene->texCoords[v].u = tc.x;
			scene->texCoords[v].v = 1.0f - tc.y;
#else
			scene->normals[v].normal = encodeDir(normal);
			scene->tangents[v].tangent = encodeDir(tangent);
			scene->texCoords[v].texCoord = pack2Floats(float2(tc.x, 1.0f - tc.y));
#endif
		}

		uint32_t accIndex = 0;
		for (uint32_t i = 0; i < scene->numMeshes; ++i)
		{
			MeshIn& batch = scene->meshes[i];

			AssimpImporter::Mesh mesh = model.mMeshArray[i];
			batch.materialId = (uint32_t)i;
			batch.vertexCount = (uint32_t)mesh.mPositions.size();

			batch.startIndex = accIndex;
			batch.indexCount = (uint32_t)mesh.mIndices.size();
			accIndex += batch.indexCount;

			LOGF(LogLevel::eINFO, "%d vertexCount: %d total: %d", i, batch.vertexCount, accIndex);
		}

		if (cached.Open(cachedModelFileName, FileMode::FM_WriteBinary, FSR_Absolute))
		{
			cached.WriteFileID(gModelVersion);
			cached.WriteUInt(scene->numMeshes);
			cached.WriteUInt(scene->totalVertices);
			cached.WriteUInt(scene->totalTriangles);
			cached.Write(scene->meshes, scene->numMeshes * sizeof(MeshIn));
			cached.Write(scene->indices.data(), scene->totalTriangles * 3 * sizeof(uint32_t));
			cached.Write(scene->positions.data(), scene->totalVertices * sizeof(SceneVertexPos));
			cached.Write(scene->texCoords.data(), scene->totalVertices * sizeof(SceneVertexTexCoord));
			cached.Write(scene->normals.data(), scene->totalVertices * sizeof(SceneVertexNormal));
			cached.Write(scene->tangents.data(), scene->totalVertices * sizeof(SceneVertexTangent));
			cached.Close();
		}
	}

	eastl::unordered_set<eastl::string> twoSidedMaterials;
	SetTwoSidedMaterials(twoSidedMaterials);

	eastl::unordered_set<eastl::string> alphaTestMaterials;
	SetAlphaTestMaterials(alphaTestMaterials);

	scene->numMaterials = scene->numMeshes;

	scene->materials = (Material*)conf_calloc(scene->numMaterials, sizeof(Material));
	scene->textures = (char**)conf_calloc(scene->numMaterials, sizeof(char*));
	scene->normalMaps = (char**)conf_calloc(scene->numMaterials, sizeof(char*));
	scene->specularMaps = (char**)conf_calloc(scene->numMaterials, sizeof(char*));

	SetMaterials(scene);
#else

	Scene* scene = (Scene*)conf_calloc(1, sizeof(Scene));
	File   assimpScene = {};
	assimpScene.Open(fileName, FileMode::FM_ReadBinary, FSRoot::FSR_Absolute);
	if (!assimpScene.IsOpen())
	{
		ErrorMsg(
			"Could not open scene %s.\nPlease make sure you have downloaded the art assets by using the PRE_BUILD command in the root "
			"directory",
			fileName);
		return NULL;
	}
	ASSERT(assimpScene.IsOpen());

	assimpScene.Read(&scene->numMeshes, sizeof(uint32_t));
	assimpScene.Read(&scene->totalVertices, sizeof(uint32_t));
	assimpScene.Read(&scene->totalTriangles, sizeof(uint32_t));

	scene->totalTriangles = scene->totalTriangles / 3;

	scene->meshes = (MeshIn*)conf_calloc(scene->numMeshes, sizeof(MeshIn));
	scene->indices = eastl::vector<uint32>(scene->totalTriangles * 3, uint32_t(0));
	scene->positions = eastl::vector<SceneVertexPos>(scene->totalVertices, SceneVertexPos{ 0 });
	scene->texCoords = eastl::vector<SceneVertexTexCoord>(scene->totalVertices, SceneVertexTexCoord{ 0 });
	scene->normals = eastl::vector<SceneVertexNormal>(scene->totalVertices, SceneVertexNormal{ 0 });
	scene->tangents = eastl::vector<SceneVertexTangent>(scene->totalVertices, SceneVertexTangent{ 0 });

	eastl::vector<float2> texcoords(scene->totalVertices);
	eastl::vector<float3> normals(scene->totalVertices);
	eastl::vector<float3> tangents(scene->totalVertices);

	assimpScene.Read(scene->indices.data(), sizeof(uint32_t) * scene->totalTriangles * 3);
	assimpScene.Read(scene->positions.data(), sizeof(float3) * scene->totalVertices);
	assimpScene.Read(texcoords.data(), sizeof(float2) * scene->totalVertices);
	assimpScene.Read(normals.data(), sizeof(float3) * scene->totalVertices);
	assimpScene.Read(tangents.data(), sizeof(float3) * scene->totalVertices);

	for (uint32_t v = 0; v < scene->totalVertices; v++)
	{
		const float3& normal = normals[v];
		const float3& tangent = tangents[v];
		const float2& tc = texcoords[v];

#if defined(METAL) || defined(__linux__)
		scene->normals[v].nx = normal.x;
		scene->normals[v].ny = normal.y;
		scene->normals[v].nz = normal.z;

		scene->tangents[v].tx = tangent.x;
		scene->tangents[v].ty = tangent.y;
		scene->tangents[v].tz = tangent.z;

		scene->texCoords[v].u = tc.x;
		scene->texCoords[v].v = 1.0f - tc.y;
#else
		scene->normals[v].normal = encodeDir(normal);
		scene->tangents[v].tangent = encodeDir(tangent);
		scene->texCoords[v].texCoord = pack2Floats(float2(tc.x, 1.0f - tc.y));
#endif
	}

	for (uint32_t i = 0; i < scene->numMeshes; ++i)
	{
		MeshIn& batch = scene->meshes[i];

		assimpScene.Read(&batch.materialId, sizeof(uint32_t));
		assimpScene.Read(&batch.vertexCount, sizeof(uint32_t));

		assimpScene.Read(&batch.startIndex, sizeof(uint32_t));
		assimpScene.Read(&batch.indexCount, sizeof(uint32_t));
	}

	eastl::unordered_set<eastl::string> twoSidedMaterials;
	SetTwoSidedMaterials(twoSidedMaterials);

	eastl::unordered_set<eastl::string> alphaTestMaterials;
	SetAlphaTestMaterials(alphaTestMaterials);

	assimpScene.Read(&scene->numMaterials, sizeof(uint32_t));
	scene->materials = (Material*)conf_calloc(scene->numMaterials, sizeof(Material));
	scene->textures = (char**)conf_calloc(scene->numMaterials, sizeof(char*));
	scene->normalMaps = (char**)conf_calloc(scene->numMaterials, sizeof(char*));
	scene->specularMaps = (char**)conf_calloc(scene->numMaterials, sizeof(char*));

#ifdef ORBIS
#define DEFAULT_ALBEDO "default.gnf"
#define DEFAULT_NORMAL "default_nrm.gnf"
#define DEFAULT_SPEC "default_spec.gnf"
#else

#define DEFAULT_ALBEDO "Default"
#define DEFAULT_NORMAL "Default_NRM"
#define DEFAULT_SPEC "Default_SPEC"
#define DEFAULT_SPEC_TRANSPARENT "Default_SPEC_TRANS"

#endif

	for (uint32_t i = 0; i < scene->numMaterials; i++)
	{
		Material& m = scene->materials[i];
		m.twoSided = false;

		uint32_t matNameLength = 0;
		assimpScene.Read(&matNameLength, sizeof(uint32_t));

		eastl::vector<char> matName(matNameLength);
		assimpScene.Read(matName.data(), sizeof(char) * matNameLength);

		uint32_t albedoNameLength = 0;
		assimpScene.Read(&albedoNameLength, sizeof(uint32_t));

		eastl::vector<char> albedoName(albedoNameLength);
		assimpScene.Read(albedoName.data(), sizeof(char) * albedoNameLength);

		if (albedoName[0] != '\0')
		{
			eastl::string path(albedoName.data());
			uint            dotPos = 0;
#ifdef ORBIS
			// try to load the GNF version instead: change extension to GNF
			path.rfind('.', -1, &dotPos);
			path.resize(dotPos);
			path[dotPos] = '\0';
			path.append(".gnf", 4);
#endif
			eastl::string albedoMap(path);

			eastl::vector<eastl::string> columnas;

			eastl::string columnaA3("columna_a_color_3");
			eastl::string columnaA2("columna_a_color_2");
			eastl::string columnaA1("columna_a_color");

			eastl::string columnaB3("columna_b_color_3");
			eastl::string columnaB2("columna_b_color_2");

			columnas.push_back(columnaA3);
			columnas.push_back(columnaA2);
			columnas.push_back(columnaB3);
			columnas.push_back(columnaB2);
			columnas.push_back(columnaA1);

			eastl::string columnaB1("columna_b_color");

			for (size_t c = 0; c < columnas.size(); c++)
			{
				int res = path.find(columnas[c], 0);

				if (res > 0)
				{
					path.replace(columnas[c], columnaB1);
					break;
				}
			}

			eastl::vector<eastl::string> macetas;

			eastl::string macetasA2("Maceta_A2_Color");
			eastl::string macetasC("Maceta_C_Color");
			eastl::string macetasC2("Maceta_C2_Color");
			eastl::string macetasB("Maceta_B_Color");
			eastl::string macetasD2("Maceta_D2_Color_0");

			macetas.push_back(macetasA2);
			macetas.push_back(macetasC);
			macetas.push_back(macetasC2);
			macetas.push_back(macetasB);
			macetas.push_back(macetasD2);

			eastl::string macetasA("Maceta_A_Color");

			for (size_t c = 0; c < macetas.size(); c++)
			{
				int res = path.find(macetas[c], 0);

				if (res > 0)
				{
					path.replace(macetas[c], macetasA);
					break;
				}
			}

			eastl::string muros_q3("muros_q3");
			eastl::string muros_q4("muros_q4");

			int res = path.find(muros_q3, 0);

			if (res > 0)
			{
				path.replace(muros_q3, muros_q4);
			}

			//muros_q_patio2
			eastl::string muros_n("muros_n");
			eastl::string muros_q_patio2("muros_q_patio2");

			res = path.find(muros_n, 0);

			if (res > 0)
			{
				path.replace(muros_n, muros_q_patio2);
			}

			if (!FileSystem::FileExists(path, FSR_Textures))
			{
				eastl::string base_filename2 = FileSystem::GetFileName(path);
				eastl::string pTemp("");
				base_filename2.append(pTemp.begin(), pTemp.end());

				// try load the associated normal map
				albedoMap.resize(0);
				albedoMap.reserve(base_filename2.size());
				albedoMap.append(base_filename2.begin(), base_filename2.end());

				if (!FileSystem::FileExists(albedoMap, FSR_Textures))
				{
					albedoMap = DEFAULT_ALBEDO;
				}
			}

			scene->textures[i] = (char*)conf_calloc(albedoMap.size() + 1, sizeof(char));
			strcpy(scene->textures[i], albedoMap.c_str());

			// try load the associated normal map
			eastl::string normalMap(path);
			normalMap.rfind('.', -1, &dotPos);
			normalMap.insert(dotPos, "_NRM", 4);

			if (!FileSystem::FileExists(normalMap, FSR_Textures))
			{
				eastl::string base_filename2 = FileSystem::GetFileName(path);
				eastl::string pTemp("");
				base_filename2.append(pTemp.begin(), pTemp.end());

				// try load the associated normal map
				normalMap.resize(0);
				normalMap.reserve(base_filename2.size());
				normalMap.append(base_filename2.begin(), base_filename2.end());

				normalMap.rfind('.', -1, &dotPos);
				normalMap.insert(dotPos, "_NRM", 4);

				if (!FileSystem::FileExists(normalMap, FSR_Textures))
				{
					normalMap = DEFAULT_NORMAL;
				}
			}

			scene->normalMaps[i] = (char*)conf_calloc(normalMap.size() + 1, sizeof(char));
			strcpy(scene->normalMaps[i], normalMap.c_str());

			// try load the associated spec map
			eastl::string specMap(path);
			dotPos = 0;
			specMap.rfind('.', -1, &dotPos);
			specMap.insert(dotPos, "_SPEC", 5);

			if (!FileSystem::FileExists(specMap, FSR_Textures))
			{
				eastl::string base_filename2 = FileSystem::GetFileName(path);
				eastl::string pTemp("");
				base_filename2.append(pTemp.begin(), pTemp.end());

				// try load the associated specular map
				specMap.resize(0);
				specMap.reserve(base_filename2.size());
				specMap.append(base_filename2.begin(), base_filename2.end());

				specMap.rfind('.', -1, &dotPos);
				specMap.insert(dotPos, "_SPEC", 5);

				if (!FileSystem::FileExists(specMap, FSR_Textures))
				{
					//Transparent
					if (i == 9 || i == 13 || i == 26 || i == 61 || i == 70 || i == 79 || i == 80 || i == 87 || i == 109 || i == 110 ||
						i == 119 || i == 147 || i == 160 || i == 197 || i == 215)
					{
						specMap = DEFAULT_SPEC_TRANSPARENT;
					}
					else
						specMap = DEFAULT_SPEC;
				}
			}

			//if (!FileSystem::FileExists(specMap, FSR_Textures))
			//	specMap = DEFAULT_SPEC;

			scene->specularMaps[i] = (char*)conf_calloc(specMap.size() + 1, sizeof(char));
			strcpy(scene->specularMaps[i], specMap.c_str());
		}
		else
		{
			// default textures
			scene->textures[i] = (char*)conf_calloc(strlen(DEFAULT_ALBEDO) + 1, sizeof(char));
			strcpy(scene->textures[i], DEFAULT_ALBEDO);

			scene->normalMaps[i] = (char*)conf_calloc(strlen(DEFAULT_NORMAL) + 1, sizeof(char));
			strcpy(scene->normalMaps[i], DEFAULT_NORMAL);

			scene->specularMaps[i] = (char*)conf_calloc(strlen(DEFAULT_SPEC) + 1, sizeof(char));

			//Transparent
			if (i == 9 || i == 13 || i == 26 || i == 61 || i == 70 || i == 79 || i == 80 || i == 87 || i == 109 || i == 110 || i == 119 ||
				i == 147 || i == 160 || i == 197 || i == 215)
			{
				strcpy(scene->specularMaps[i], DEFAULT_SPEC_TRANSPARENT);
			}
			else
				strcpy(scene->specularMaps[i], DEFAULT_SPEC);
		}

		float ns = 0.0f;
		assimpScene.Read(&ns, sizeof(float));    // load shininess

		int twoSided = 0;
		assimpScene.Read(&twoSided, sizeof(float));    // load two sided
		m.twoSided = (twoSided != 0);

		eastl::string tinyMatName(matName.data());
		if (twoSidedMaterials.find(tinyMatName) != twoSidedMaterials.end())
			m.twoSided = true;

		m.alphaTested = (alphaTestMaterials.find(tinyMatName) != alphaTestMaterials.end());
	}

	assimpScene.Close();

#endif
	BufferLoadDesc indirectVBPosDesc = {};
	indirectVBPosDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER | DESCRIPTOR_TYPE_BUFFER;
	indirectVBPosDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	indirectVBPosDesc.mDesc.mVertexStride = sizeof(SceneVertexPos);
	indirectVBPosDesc.mDesc.mElementCount = scene->totalVertices;
	indirectVBPosDesc.mDesc.mStructStride = sizeof(SceneVertexPos);
	indirectVBPosDesc.mDesc.mSize = indirectVBPosDesc.mDesc.mElementCount * indirectVBPosDesc.mDesc.mStructStride;
	indirectVBPosDesc.pData = scene->positions.data();
	indirectVBPosDesc.ppBuffer = &scene->m_pIndirectPosBuffer;
	indirectVBPosDesc.mDesc.pDebugName = L"Indirect Vertex Position Buffer Desc";
	addResource(&indirectVBPosDesc, true);

	// Vertex texcoord buffer for the scene
	BufferLoadDesc indirectVBTexCoordDesc = {};
	indirectVBTexCoordDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER | DESCRIPTOR_TYPE_BUFFER;
	indirectVBTexCoordDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	indirectVBTexCoordDesc.mDesc.mVertexStride = sizeof(SceneVertexTexCoord);
	indirectVBTexCoordDesc.mDesc.mElementCount = scene->totalVertices * (sizeof(SceneVertexTexCoord) / sizeof(uint32_t));
	indirectVBTexCoordDesc.mDesc.mStructStride = sizeof(uint32_t);
	indirectVBTexCoordDesc.mDesc.mSize = indirectVBTexCoordDesc.mDesc.mElementCount * indirectVBTexCoordDesc.mDesc.mStructStride;
	indirectVBTexCoordDesc.pData = scene->texCoords.data();
	indirectVBTexCoordDesc.ppBuffer = &scene->m_pIndirectTexCoordBuffer;
	indirectVBTexCoordDesc.mDesc.pDebugName = L"Indirect Vertex TexCoord Buffer Desc";
	addResource(&indirectVBTexCoordDesc, true);

	// Vertex normal buffer for the scene
	BufferLoadDesc indirectVBNormalDesc = {};
	indirectVBNormalDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER | DESCRIPTOR_TYPE_BUFFER;
	indirectVBNormalDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	indirectVBNormalDesc.mDesc.mVertexStride = sizeof(SceneVertexNormal);
	indirectVBNormalDesc.mDesc.mElementCount = scene->totalVertices * (sizeof(SceneVertexNormal) / sizeof(uint32_t));
	indirectVBNormalDesc.mDesc.mStructStride = sizeof(uint32_t);
	indirectVBNormalDesc.mDesc.mSize = indirectVBNormalDesc.mDesc.mElementCount *
		indirectVBNormalDesc.mDesc.mStructStride;
	indirectVBNormalDesc.pData = scene->normals.data();
	indirectVBNormalDesc.ppBuffer = &scene->m_pIndirectNormalBuffer;
	indirectVBNormalDesc.mDesc.pDebugName = L"Indirect Vertex Normal Buffer Desc";
	addResource(&indirectVBNormalDesc, true);

	// Vertex tangent buffer for the scene
	BufferLoadDesc indirectVBTangentDesc = {};
	indirectVBTangentDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER | DESCRIPTOR_TYPE_BUFFER;
	indirectVBTangentDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	indirectVBTangentDesc.mDesc.mVertexStride = sizeof(SceneVertexTangent);
	indirectVBTangentDesc.mDesc.mElementCount = scene->totalVertices * (sizeof(SceneVertexTangent) / sizeof(uint32_t));
	indirectVBTangentDesc.mDesc.mStructStride = sizeof(uint32_t);
	indirectVBTangentDesc.mDesc.mSize = indirectVBTangentDesc.mDesc.mElementCount * indirectVBTangentDesc.mDesc.mStructStride;
	indirectVBTangentDesc.pData = scene->tangents.data();
	indirectVBTangentDesc.ppBuffer = &scene->m_pIndirectTangentBuffer;
	indirectVBTangentDesc.mDesc.pDebugName = L"Indirect Vertex Tangent Buffer Desc";
	addResource(&indirectVBTangentDesc, true);


	BufferLoadDesc indirectIBDesc = {};
	indirectIBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER | DESCRIPTOR_TYPE_BUFFER;
	indirectIBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	indirectIBDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
	indirectIBDesc.mDesc.mElementCount = scene->totalTriangles * 3;
	indirectIBDesc.mDesc.mStructStride = sizeof(uint32_t);
	indirectIBDesc.mDesc.mSize = indirectIBDesc.mDesc.mElementCount * indirectIBDesc.mDesc.mStructStride;
	indirectIBDesc.pData = scene->indices.data();
	indirectIBDesc.ppBuffer = &scene->m_pIndirectIndexBuffer;
	indirectIBDesc.mDesc.pDebugName = L"Indirect Non-filtered Index Buffer Desc";
	addResource(&indirectIBDesc, true);
	return scene;
}

void removeScene(Scene* scene)
{
	for (uint32_t i = 0; i < scene->numMaterials; ++i)
	{
		if (scene->textures[i])
		{
			conf_free(scene->textures[i]);
			scene->textures[i] = NULL;
		}

		if (scene->normalMaps[i])
		{
			conf_free(scene->normalMaps[i]);
			scene->normalMaps[i] = NULL;
		}

		if (scene->specularMaps[i])
		{
			conf_free(scene->specularMaps[i]);
			scene->specularMaps[i] = NULL;
		}
	}

	if (scene->m_pIndirectIndexBuffer)
	{
		removeResource(scene->m_pIndirectIndexBuffer);
	}
	if (scene->m_pIndirectNormalBuffer)
	{
		removeResource(scene->m_pIndirectNormalBuffer);
	}
	if (scene->m_pIndirectPosBuffer)
	{
		removeResource(scene->m_pIndirectPosBuffer);
	}
	if (scene->m_pIndirectTangentBuffer)
	{
		removeResource(scene->m_pIndirectTangentBuffer);
	}
	if (scene->m_pIndirectTexCoordBuffer)
	{
		removeResource(scene->m_pIndirectTexCoordBuffer);
	}

	if (scene->mPIndexBuffer)
	{
		removeResource(scene->mPIndexBuffer);
	}
	if (scene->mPVertexBuffer)
	{
		removeResource(scene->mPVertexBuffer);
	}

	for (uint32_t i = 0; i < scene->numMeshes; ++i)
	{
		destroyClusters(&scene->meshes[i]);
	}


	scene->positions.~vector();
	scene->texCoords.~vector();
	scene->normals.~vector();
	scene->tangents.~vector();
	scene->indices.~vector();

	conf_free(scene->textures);
	conf_free(scene->normalMaps);
	conf_free(scene->specularMaps);
	conf_free(scene->meshes);
	conf_free(scene->materials);
	conf_free(scene);
}

vec3 makeVec3(const SceneVertexPos& v) { return vec3(v.x, v.y, v.z); }

void createAABB(const Scene* pScene, MeshIn* mesh)
{
	vec4 aabbMin = vec4(INFINITY, INFINITY, INFINITY, INFINITY);
	vec4 aabbMax = -aabbMin;

	for (uint t = 0; t < mesh->indexCount; ++t)
	{
		vec4 currentVertex;

		uint32_t currentIndex = pScene->indices[mesh->startIndex + t];

		currentVertex = vec4(makeVec3(pScene->positions[currentIndex]), 1.0);

		aabbMin = minPerElem(aabbMin, currentVertex);
		aabbMax = maxPerElem(aabbMax, currentVertex);
	}

	mesh->AABB.maxPt = float4(aabbMax.getX(), aabbMax.getY(), aabbMax.getZ(), aabbMax.getW());
	mesh->AABB.minPt = float4(aabbMin.getX(), aabbMin.getY(), aabbMin.getZ(), aabbMin.getW());
	mesh->AABB.Center = (mesh->AABB.maxPt + mesh->AABB.minPt) / 2.0f;

	mesh->AABB.Extents = (float4(
		fabsf(mesh->AABB.maxPt.x - mesh->AABB.minPt.x), fabsf(mesh->AABB.maxPt.y - mesh->AABB.minPt.y),
		fabsf(mesh->AABB.maxPt.z - mesh->AABB.minPt.z), fabsf(mesh->AABB.maxPt.w - mesh->AABB.minPt.w))) *
		0.5f;

	mesh->AABB.corners[0] = mesh->AABB.Center - mesh->AABB.Extents;
	mesh->AABB.corners[0].w = 1.0f;
	mesh->AABB.corners[1] = mesh->AABB.Center + float4(mesh->AABB.Extents.x, -mesh->AABB.Extents.y, -mesh->AABB.Extents.z, 0.0);
	mesh->AABB.corners[1].w = 1.0f;
	mesh->AABB.corners[2] = mesh->AABB.Center + float4(-mesh->AABB.Extents.x, mesh->AABB.Extents.y, -mesh->AABB.Extents.z, 0.0);
	mesh->AABB.corners[2].w = 1.0f;
	mesh->AABB.corners[3] = mesh->AABB.Center + float4(-mesh->AABB.Extents.x, -mesh->AABB.Extents.y, mesh->AABB.Extents.z, 0.0);
	mesh->AABB.corners[3].w = 1.0f;

	mesh->AABB.corners[4] = mesh->AABB.Center + float4(-mesh->AABB.Extents.x, mesh->AABB.Extents.y, mesh->AABB.Extents.z, 0.0);
	mesh->AABB.corners[4].w = 1.0f;
	mesh->AABB.corners[5] = mesh->AABB.Center + float4(mesh->AABB.Extents.x, -mesh->AABB.Extents.y, mesh->AABB.Extents.z, 0.0);
	mesh->AABB.corners[5].w = 1.0f;
	mesh->AABB.corners[6] = mesh->AABB.Center + float4(mesh->AABB.Extents.x, mesh->AABB.Extents.y, -mesh->AABB.Extents.z, 0.0);
	mesh->AABB.corners[6].w = 1.0f;
	mesh->AABB.corners[7] = mesh->AABB.Center + mesh->AABB.Extents;
	mesh->AABB.corners[7].w = 1.0f;
}

// Compute an array of clusters from the mesh vertices. Clusters are sub batches of the original mesh limited in number
// for more efficient CPU / GPU culling. CPU culling operates per cluster, while GPU culling operates per triangle for
// all the clusters that passed the CPU test.
void createClusters(bool twoSided, const Scene* pScene, MeshIn* mesh)
{
	// 12 KiB stack space
	struct Triangle
	{
		vec3 vtx[3];
	};

	Triangle triangleCache[CLUSTER_SIZE * 3];

	const int triangleCount = mesh->indexCount / 3;
	const int clusterCount = (triangleCount + CLUSTER_SIZE - 1) / CLUSTER_SIZE;

	mesh->clusterCount = clusterCount;
	mesh->clusterCompacts = (ClusterCompact*)conf_calloc(mesh->clusterCount, sizeof(ClusterCompact));
	mesh->clusters = (Cluster*)conf_calloc(mesh->clusterCount, sizeof(Cluster));

	for (int i = 0; i < clusterCount; ++i)
	{
		const int clusterStart = i * CLUSTER_SIZE;
		const int clusterEnd = min(clusterStart + CLUSTER_SIZE, triangleCount);

		const int clusterTriangleCount = clusterEnd - clusterStart;

		// Load all triangles into our local cache
		for (int triangleIndex = clusterStart; triangleIndex < clusterEnd; ++triangleIndex)
		{
			triangleCache[triangleIndex - clusterStart].vtx[0] =
				makeVec3(pScene->positions[pScene->indices[mesh->startIndex + triangleIndex * 3]]);
			triangleCache[triangleIndex - clusterStart].vtx[1] =
				makeVec3(pScene->positions[pScene->indices[mesh->startIndex + triangleIndex * 3 + 1]]);
			triangleCache[triangleIndex - clusterStart].vtx[2] =
				makeVec3(pScene->positions[pScene->indices[mesh->startIndex + triangleIndex * 3 + 2]]);
		}

		vec3 aabbMin = vec3(INFINITY, INFINITY, INFINITY);
		vec3 aabbMax = -aabbMin;

		vec3 coneAxis = vec3(0, 0, 0);

		for (int triangleIndex = 0; triangleIndex < clusterTriangleCount; ++triangleIndex)
		{
			const auto& triangle = triangleCache[triangleIndex];
			for (int j = 0; j < 3; ++j)
			{
				aabbMin = minPerElem(aabbMin, triangle.vtx[j]);
				aabbMax = maxPerElem(aabbMax, triangle.vtx[j]);
			}

			vec3 triangleNormal = cross(triangle.vtx[1] - triangle.vtx[0], triangle.vtx[2] - triangle.vtx[0]);

			if (!(triangleNormal == vec3(0, 0, 0)))
				triangleNormal = normalize(triangleNormal);

			//coneAxis = DirectX::XMVectorAdd(coneAxis, DirectX::XMVectorNegate(triangleNormal));
			coneAxis = coneAxis - triangleNormal;
		}

		// This is the cosine of the cone opening angle - 1 means it's 0?,
		// we're minimizing this value (at 0, it would mean the cone is 90?
		// open)
		float coneOpening = 1;
		// dont cull two sided meshes
		bool validCluster = !twoSided;

		const vec3 center = (aabbMin + aabbMax) / 2;
		// if the axis is 0 then we have a invalid cluster
		if (coneAxis == vec3(0, 0, 0))
			validCluster = false;

		coneAxis = normalize(coneAxis);

		float t = -INFINITY;

		// cant find a cluster for 2 sided objects
		if (validCluster)
		{
			// We nee a second pass to find the intersection of the line center + t * coneAxis with the plane defined by each triangle
			for (int triangleIndex = 0; triangleIndex < clusterTriangleCount; ++triangleIndex)
			{
				const Triangle& triangle = triangleCache[triangleIndex];
				// Compute the triangle plane from the three vertices

				const vec3 triangleNormal = normalize(cross(triangle.vtx[1] - triangle.vtx[0], triangle.vtx[2] - triangle.vtx[0]));

				const float directionalPart = dot(coneAxis, -triangleNormal);

				if (directionalPart <= 0)    //AMD BUG?: changed to <= 0 because directionalPart is used to divide a quantity
				{
					// No solution for this cluster - at least two triangles are facing each other
					validCluster = false;
					break;
				}

				// We need to intersect the plane with our cone ray which is center + t * coneAxis, and find the max
				// t along the cone ray (which points into the empty space) See: https://en.wikipedia.org/wiki/Line%E2%80%93plane_intersection
				const float td = dot(center - triangle.vtx[0], triangleNormal) / -directionalPart;

				t = max(t, td);

				coneOpening = min(coneOpening, directionalPart);
			}
		}

		mesh->clusters[i].aabbMax = v3ToF3(aabbMax);
		mesh->clusters[i].aabbMin = v3ToF3(aabbMin);

		mesh->clusters[i].coneAngleCosine = sqrtf(1 - coneOpening * coneOpening);
		mesh->clusters[i].coneCenter = v3ToF3(center + coneAxis * t);
		mesh->clusters[i].coneAxis = v3ToF3(coneAxis);

		mesh->clusterCompacts[i].triangleCount = clusterTriangleCount;
		mesh->clusterCompacts[i].clusterStart = clusterStart;

		//#if AMD_GEOMETRY_FX_ENABLE_CLUSTER_CENTER_SAFETY_CHECK
		// If distance of coneCenter to the bounding box center is more than 16x the bounding box extent, the cluster is also invalid
		// This is mostly a safety measure - if triangles are nearly parallel to coneAxis, t may become very large and unstable
		const float aabbSize = length(aabbMax - aabbMin);
		const float coneCenterToCenterDistance = length(f3Tov3(mesh->clusters[i].coneCenter) - center);

		if (coneCenterToCenterDistance > (16 * aabbSize))
			validCluster = false;

		mesh->clusters[i].valid = validCluster;
	}
}

void destroyClusters(MeshIn* pMesh)
{
	// Destroy clusters
	conf_free(pMesh->clusters);
	conf_free(pMesh->clusterCompacts);
}

void addClusterToBatchChunk(
	const ClusterCompact* cluster, uint batchStart, uint accumDrawCount, uint accumNumTriangles, int meshIndex,
	FilterBatchChunk* batchChunk)
{
	const int filteredIndexBufferStartOffset = accumNumTriangles * 3;

	FilterBatchData* smallBatchData = &batchChunk->batches[batchChunk->currentBatchCount++];

	smallBatchData->accumDrawIndex = accumDrawCount;
	smallBatchData->faceCount = cluster->triangleCount;
	smallBatchData->meshIndex = meshIndex;

	// Offset relative to the start of the mesh
	smallBatchData->indexOffset = cluster->clusterStart * 3;
	smallBatchData->outputIndexOffset = filteredIndexBufferStartOffset;
	smallBatchData->drawBatchStart = batchStart;
}

float GetTwoSidedWorldSpaceBias(const eastl::string& strKey)
{
	float finalVal = 0.f;

	if (strKey.find("kinda") != eastl::string::npos)
	{
		finalVal = 0.09f;
	}
	else if (strKey.find("thin") != eastl::string::npos)
	{
		finalVal = 0.15f;
		if (strKey.find("super") != eastl::string::npos)
		{
			finalVal = 0.5f; //original 0.6 or 0.7
		}

		if (strKey.find("triple") != eastl::string::npos)
		{
			finalVal = 0.8f;
		}
	}
	return finalVal;
}


void GetSDFCustomSubMeshData(const eastl::string& strKey, SDFCustomSubMeshData& outData)
{
	outData.mIsSDFMesh = true;
	outData.mIsTwoSided = false;
	outData.mUseDoubleVoxelSize = false;
	outData.mIsAlphaTested = false;
	outData.mTwoSidedWorldSpaceBias = 0.f;
	outData.mMeshName = strKey;

	if (strKey.find("twosided") != eastl::string::npos)
	{
		outData.mIsTwoSided = true;
		outData.mTwoSidedWorldSpaceBias = GetTwoSidedWorldSpaceBias(strKey);
	}

	if (strKey.find("double") != eastl::string::npos)
	{
		outData.mUseDoubleVoxelSize = true;
	}

	if (strKey.find("alphatested") != eastl::string::npos)
	{
		outData.mIsAlphaTested = true;
	}
}

CPUImage* loadImage(const eastl::string& imgFilename)
{
	CPUImage* image = conf_new(CPUImage);
	image->loadImage(imgFilename.c_str());
	image->Uncompress();
	return image;
}

void initAlphaTestedImageMaps(AlphaTestedImageMaps& imageMaps)
{
	imageMaps.reserve(100);

	imageMaps["HP19lef3.dds"] = loadImage("HP19lef3.dds");
	imageMaps["sm_leaf_02a.dds"] = loadImage("sm_leaf_02a.dds");
	imageMaps["sm_leaf_02b.dds"] = loadImage("sm_leaf_02b.dds");

	imageMaps["FL19pe15.dds"] = loadImage("FL19pe15.dds");
	imageMaps["FL17pet1.dds"] = loadImage("FL17pet1.dds");
	imageMaps["FL16lef3.dds"] = loadImage("FL16lef3.dds");
	imageMaps["FL17lef2.dds"] = loadImage("FL17lef2.dds");
	imageMaps["FL13pet2.dds"] = loadImage("FL13pet2.dds");
	imageMaps["citrus_limon_leaf.dds"] = loadImage("citrus_limon_leaf.dds");
	imageMaps["HP01lef2.dds"] = loadImage("HP01lef2.dds");
	imageMaps["FL13lef4.dds"] = loadImage("FL13lef4.dds");
	imageMaps["FL19lef2.dds"] = loadImage("FL19lef2.dds");
	imageMaps["HP17lef2.dds"] = loadImage("HP17lef2.dds");
	imageMaps["HP13lef1.dds"] = loadImage("HP13lef1.dds");
	imageMaps["HP11lef2.dds"] = loadImage("HP11lef2.dds");
	imageMaps["HP19lef2.dds"] = loadImage("HP19lef2.dds");

	imageMaps["TR14lef1.dds"] = loadImage("TR14lef1.dds");
	imageMaps["FL29pet1.dds"] = loadImage("FL29pet1.dds");
	imageMaps["FL29lef.dds"] = loadImage("FL29lef.dds");

	imageMaps["FL29pet2.dds"] = loadImage("FL29pet2.dds");

	imageMaps["HP01pet1.dds"] = loadImage("HP01pet1.dds");
	imageMaps["FL19lef4.dds"] = loadImage("FL19lef4.dds");
	imageMaps["FL19pe16.dds"] = loadImage("FL19pe16.dds");

	imageMaps["HP01pet3.dds"] = loadImage("HP01pet3.dds");
	imageMaps["TR02lef5.dds"] = loadImage("TR02lef5.dds");
	imageMaps["l37-upper.dds"] = loadImage("l37-upper.dds");
	imageMaps["sm_leaf_seca_02b.dds"] = loadImage("sm_leaf_seca_02b.dds");
	imageMaps["l04-upper.dds"] = loadImage("l04-upper.dds");
	imageMaps["FL13pet1.dds"] = loadImage("FL13pet1.dds");
	imageMaps["FL16pet1.dds"] = loadImage("FL16pet1.dds");
	imageMaps["aglaonema_leaf.dds"] = loadImage("aglaonema_leaf.dds");

}

void initAlphaTestedMaterialTexturesMaps(AlphaTestedMaterialMaps& alphaTestedMaterialMaps)
{
	//alphaTestedMaterialMaps.reserve(30);

	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves04", "HP19lef3.dds"));

	alphaTestedMaterialMaps.push_back(eastl::make_pair("treeLeaves04", "sm_leaf_02a.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("treeLeaves00", "sm_leaf_02a.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("treeLeaves01", "sm_leaf_02a.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("treeLeaves02", "sm_leaf_seca_02b.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("treeLeaves03", "sm_leaf_02b.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("treeLeaves05", "l37-upper.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("treeLeaves06", "FL29pet2.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("treeLeaves07", "sm_leaf_02a.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("treeLeaves08", "l04-upper.dds"));

	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves022", "FL16lef3.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves024", "HP17lef2.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("rose00", "FL13pet2.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves0379", "citrus_limon_leaf.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves0380", "HP01lef2.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves0381", "TR02lef5.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves013", "HP01lef2.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves014", "FL13lef4.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves0377", "FL17lef2.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves0378", "FL17lef2.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves026", "FL19lef2.dds"));

	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves00", "FL16lef3.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves012", "HP17lef2.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves02", "HP17lef2.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves015", "HP13lef1.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves019", "HP11lef2.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves023", "HP13lef1.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves09", "HP19lef2.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves06", "citrus_limon_leaf.dds"));

	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves016", "HP19lef3.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves07", "FL29lef.dds"));

	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves027", "FL29pet2.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves025", "FL29pet2.dds"));

	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves020", "TR14lef1.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves021", "FL16pet1.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves010", "TR14lef1.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves05", "FL29lef.dds"));

	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves07", "FL29lef.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves017", "FL29lef.dds"));

	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves0300", "FL29lef.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves018", "FL19lef4.dds"));

	alphaTestedMaterialMaps.push_back(eastl::make_pair("floor1", "FL29pet2.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("flower01", "FL19pe15.dds"));



	alphaTestedMaterialMaps.push_back(eastl::make_pair("flower0304", "HP01pet1.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("flower0341", "FL29pet1.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("flower0343", "FL19pe16.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("flower0338", "HP01pet3.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves08", "TR02lef5.dds"));

	alphaTestedMaterialMaps.push_back(eastl::make_pair("group146", "l37-upper.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("group147", "FL29pet2.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("flower0342", "FL13pet1.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("flower0344", "FL17pet1.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("flower0340", "FL13pet1.dds"));

	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves011", "FL17lef2.dds"));
	alphaTestedMaterialMaps.push_back(eastl::make_pair("smallLeaves01", "aglaonema_leaf.dds"));
}

eastl::string FindAlbedoTextureName(const eastl::string& meshName,
	const AlphaTestedMaterialMaps& alphaTestedMaterialMaps)
{
	for (AlphaTestedMaterialMaps::const_iterator iter = alphaTestedMaterialMaps.begin();
		iter != alphaTestedMaterialMaps.end(); ++iter)
	{
		//we are doing double check here to handle more than one digit
		//for example if map value is 07 but mesh name is 075, then the first check
		//will give true incorrectly, but the second check is going to verify it
		eastl_size_t position = meshName.find(iter->first);
		if (meshName.find(iter->first) != eastl::string::npos)
		{
			eastl::string restOfStr = meshName.substr(position + iter->first.size());
			if (restOfStr[0] == '_')
			{
				return iter->second;
			}
		}
	}

	return "";
}

CPUImage* FindAlbedoImage(const eastl::string& texName, const AlphaTestedImageMaps& alphaTestedImageMaps)
{
	for (AlphaTestedImageMaps::const_iterator iter = alphaTestedImageMaps.begin();
		iter != alphaTestedImageMaps.end(); ++iter)
	{
		if (texName.find(iter->first) != eastl::string::npos)
		{
			return iter->second;
		}
	}

	return NULL;
}


void loadSDFMeshAlphaTested(ThreadSystem* threadSystem, const eastl::string& fileName, SDFMesh* outMesh, float scale,
	float offsetX, bool generateSDFVolumeData, AlphaTestedImageMaps& alphaTestedImageMaps, AlphaTestedMaterialMaps& alphaTestedMaterialMaps,
	BakedSDFVolumeInstances& sdfVolumeInstances)

{
	AssimpImporter importer;
	AssimpImporter::Model model;
	eastl::string sceneFullPath = FileSystem::FixPath(fileName + ".obj", FSRoot::FSR_Meshes);

	if (!importer.ImportModel(sceneFullPath.c_str(), &model))
	{
		ErrorMsg("Failed to load %s", FileSystem::GetFileNameAndExtension(sceneFullPath).c_str());
		return;
	}


	uint32_t meshCount = (uint32_t)model.mMeshArray.size();

	SDFMesh& mesh = *outMesh;

	mesh.mMeshInstances.resize(meshCount);

	Vertex* vertices = NULL;
	uint32_t*   indices = NULL;

	uint32_t totalVertexCount = 0;
	uint32_t totalIndexCount = 0;



	eastl::vector<SDFCustomSubMeshData> SDFCustomSubMeshDataList;

	uint32_t realChildrenCount = 0;
	for (uint32_t i = 0; i < (uint32_t)model.mRootNode.mChildren.size(); ++i)
	{
		eastl::string curName = model.mRootNode.mChildren[i].mName;
		if (curName == "default")
		{
			continue;
		}

		eastl::string newStr = curName;

		eastl::string::iterator removeBeginIter = eastl::remove_if(newStr.begin(), newStr.end(), isspace);

		newStr.erase(removeBeginIter, newStr.end());
		if (newStr.find("sdf") != eastl::string::npos)
		{
			eastl::string::size_type stackStartPos = 0;
			stackStartPos = newStr.find("beginstack");
			if (stackStartPos != eastl::string::npos)
			{
				uint32_t endOfStackPos = (uint32_t)stackStartPos + 10;
				uint32_t digitNum = (uint32_t)newStr.size() - (endOfStackPos);
				eastl::string stackDigitStr = "";
				stackDigitStr = newStr.substr(endOfStackPos, digitNum);

				//LOGF(LogLevel::eINFO, "Stack id %s", stackDigitStr.c_str());

				if (newStr.find("beginstack10") != eastl::string::npos)
				{
					int i = 0;
				}

				eastl::vector<SDFCustomSubMeshData>::iterator findIter =
					eastl::find_if(SDFCustomSubMeshDataList.begin(), SDFCustomSubMeshDataList.end(),
						[&stackDigitStr](const SDFCustomSubMeshData& subMeshData) -> bool
				{
					return subMeshData.mStackStrID == stackDigitStr;
				});

				if (findIter != SDFCustomSubMeshDataList.end())
				{
					GetSDFCustomSubMeshData(newStr, *findIter);
					findIter->mStacks.push_back(SDFCustomSubMeshData());
					findIter->mStacks.back().m_PSubMesh = &model.mMeshArray[realChildrenCount];
					GetSDFCustomSubMeshData(newStr, findIter->mStacks.back());
				}
				else
				{
					SDFCustomSubMeshDataList.push_back(SDFCustomSubMeshData{});
					SDFCustomSubMeshData& customSubMesh = SDFCustomSubMeshDataList.back();
					GetSDFCustomSubMeshData(newStr, customSubMesh);
					customSubMesh.mStacks.push_back(SDFCustomSubMeshData());
					customSubMesh.mStacks.back().m_PSubMesh = &model.mMeshArray[realChildrenCount];
					GetSDFCustomSubMeshData(newStr, customSubMesh.mStacks.back());
					customSubMesh.mStackStrID = stackDigitStr;
				}

			}
			else
			{
				stackStartPos = newStr.find("continuestack");
				if (stackStartPos != eastl::string::npos)
				{
					uint32_t endOfStackPos = (uint32_t)stackStartPos + 13;
					uint32_t digitNum = (uint32_t)newStr.size() - (endOfStackPos);
					eastl::string stackDigitStr = "";
					stackDigitStr = newStr.substr(endOfStackPos, digitNum);
					//LOGF(LogLevel::eINFO, "Continue Stack id %s", stackDigitStr.c_str());

					eastl::vector<SDFCustomSubMeshData>::iterator findIter =
						eastl::find_if(SDFCustomSubMeshDataList.begin(), SDFCustomSubMeshDataList.end(),
							[stackDigitStr](const SDFCustomSubMeshData& subMeshData) -> bool
					{
						return subMeshData.mStackStrID == stackDigitStr;
					});

					if (findIter != SDFCustomSubMeshDataList.end())
					{
						findIter->mStacks.push_back(SDFCustomSubMeshData());
						findIter->mStacks.back().m_PSubMesh = &model.mMeshArray[realChildrenCount];

						GetSDFCustomSubMeshData(newStr, findIter->mStacks.back());
					}
					else
					{
						SDFCustomSubMeshDataList.push_back(SDFCustomSubMeshData{});
						SDFCustomSubMeshData& customSubMesh = SDFCustomSubMeshDataList.back();
						customSubMesh.mStacks.push_back(SDFCustomSubMeshData());
						customSubMesh.mStacks.back().m_PSubMesh = &model.mMeshArray[realChildrenCount];

						GetSDFCustomSubMeshData(newStr, customSubMesh.mStacks.back());
						customSubMesh.mStackStrID = stackDigitStr;
					}
				}
				else
				{
					SDFCustomSubMeshDataList.push_back(SDFCustomSubMeshData{});
					SDFCustomSubMeshData& customSubMesh = SDFCustomSubMeshDataList.back();
					customSubMesh.m_PSubMesh = &model.mMeshArray[realChildrenCount];
					GetSDFCustomSubMeshData(newStr, customSubMesh);
				}
			}

		}

		//LOGF(LogLevel::eINFO, "Alpha Node mesh instance %d name %s", realChildrenCount, newStr.c_str());
		++realChildrenCount;
	}


	for (uint32_t i = 0; i < SDFCustomSubMeshDataList.size(); ++i)
	{
		SDFCustomSubMeshData& customSubMesh = SDFCustomSubMeshDataList[i];


		if (customSubMesh.mStacks.empty())
		{
			AssimpImporter::Mesh& subMesh = *customSubMesh.m_PSubMesh;

			uint32_t vertexCount = (uint32_t)subMesh.mPositions.size();
			uint32_t indexCount = (uint32_t)subMesh.mIndices.size();

			mesh.mMeshInstances[i] = { i, indexCount, totalIndexCount, totalVertexCount };

			mesh.mMeshInstances[i].mMainMesh = &mesh;

			if (customSubMesh.mIsAlphaTested)
			{
				mesh.mMeshInstances[i].mTextureImgRef = FindAlbedoImage(FindAlbedoTextureName(
					customSubMesh.mMeshName, alphaTestedMaterialMaps), alphaTestedImageMaps);
			}

			vertices = (Vertex*)conf_realloc(vertices, sizeof(Vertex) * (totalVertexCount + vertexCount));
			indices = (uint32_t*)conf_realloc(indices, sizeof(uint32_t) * (totalIndexCount + indexCount));

			//mesh.mUncompressedTexCoords.reserve(vertexCount);
			//mesh.mUncompressedNormals.reserve(vertexCount);

			for (uint32_t j = 0; j < vertexCount; j++)
			{
				float3 transformedV = subMesh.mPositions[j] * scale + float3(offsetX, 0.f, 0.f);

				vec2 texCoord = f2Tov2(subMesh.mUvs[j]);



				mesh.mUncompressedTexCoords.push_back(texCoord);

				//vertices[totalVertexCount++] = { subMesh.mPositions[j], subMesh.mNormals[j] };
				vertices[totalVertexCount++] = { transformedV , subMesh.mNormals[j] };
				mesh.mPositions.push_back(SceneVertexPos{ transformedV.getX(), transformedV.getY(), transformedV.getZ() });
				mesh.mUncompressedNormals.push_back(f3Tov3(subMesh.mNormals[j]));

				//SDF STUFF//
				mesh.mMeshInstances[i].mLocalBoundingBox.Add(vec3(transformedV.x,
					transformedV.y, transformedV.z));


				//
			}

			uint32_t accIndex_ = 0;


			for (uint32_t j = 0; j < indexCount; j++)
			{


				int indd = subMesh.mIndices[j];

				if (indd < 0)
				{
					indices[totalIndexCount++] = accIndex_;
					mesh.mIndices.push_back(accIndex_);
				}
				else
				{
					indices[totalIndexCount++] = (indd + accIndex_);
					mesh.mIndices.push_back((indd + accIndex_));
				}
			}
			accIndex_ += (uint32_t)subMesh.mPositions.size();

			//LOGF(LogLevel::eINFO, "Alpha No stack Mesh instance %i has index count %i", i, indexCount);
		}
		else
		{
			for (uint32_t subMeshIndex = 0; subMeshIndex < customSubMesh.mStacks.size(); ++subMeshIndex)
			{
				AssimpImporter::Mesh& subMesh = *customSubMesh.mStacks[subMeshIndex].m_PSubMesh;



				uint32_t vertexCount = (uint32_t)subMesh.mPositions.size();
				uint32_t indexCount = (uint32_t)subMesh.mIndices.size();

				mesh.mMeshInstances[i].mStackInstances.push_back(SDFMeshInstance());
				SDFMeshInstance& stackInstance = mesh.mMeshInstances[i].mStackInstances.back();
				stackInstance = { i, indexCount, totalIndexCount, totalVertexCount };

				stackInstance.mIsAlphaTested = customSubMesh.mStacks[subMeshIndex].mIsAlphaTested;

				if (stackInstance.mIsAlphaTested)
				{
					eastl::string texName = FindAlbedoTextureName(
						customSubMesh.mStacks[subMeshIndex].mMeshName, alphaTestedMaterialMaps);

					stackInstance.mTextureImgRef = FindAlbedoImage(texName, alphaTestedImageMaps);

					if (!stackInstance.mTextureImgRef)
					{
						LOGF(LogLevel::eINFO, "No image for alpha tested image, object name: %s", customSubMesh.mStacks[subMeshIndex].mMeshName.c_str());
					}
				}


				stackInstance.mMainMesh = &mesh;

				vertices = (Vertex*)conf_realloc(vertices, sizeof(Vertex) * (totalVertexCount + vertexCount));
				indices = (uint32_t*)conf_realloc(indices, sizeof(uint32_t) * (totalIndexCount + indexCount));


				for (uint32_t j = 0; j < vertexCount; j++)
				{
					float3 transformedV = subMesh.mPositions[j] * scale + float3(offsetX, 0.f, 0.f);

					vec2 texCoord = f2Tov2(subMesh.mUvs[j]);



					mesh.mUncompressedTexCoords.push_back(texCoord);

					//vertices[totalVertexCount++] = { subMesh.mPositions[j], subMesh.mNormals[j] };
					vertices[totalVertexCount++] = { transformedV , subMesh.mNormals[j] };
					mesh.mPositions.push_back(SceneVertexPos{ transformedV.getX(), transformedV.getY(), transformedV.getZ() });
					mesh.mUncompressedNormals.push_back(f3Tov3(subMesh.mNormals[j]));

					//SDF STUFF//
					stackInstance.mLocalBoundingBox.Add(vec3(transformedV.x,
						transformedV.y, transformedV.z));

					mesh.mMeshInstances[i].mLocalBoundingBox.Add(vec3(transformedV.x,
						transformedV.y, transformedV.z));


					//
				}

				uint32_t accIndex_ = 0;


				for (uint32_t j = 0; j < indexCount; j++)
				{


					int indd = subMesh.mIndices[j];

					if (indd < 0)
					{
						indices[totalIndexCount++] = accIndex_;
						mesh.mIndices.push_back(accIndex_);
					}
					else
					{
						indices[totalIndexCount++] = (indd + accIndex_);
						mesh.mIndices.push_back((indd + accIndex_));
					}
				}
				accIndex_ += (uint32_t)subMesh.mPositions.size();

				//LOGF(LogLevel::eINFO, "Alpha Stack Mesh instance %i and name %s has index count %i", i, customSubMesh.mStacks[subMeshIndex].mMeshName.c_str(), indexCount);
			}
		}

		if (generateSDFVolumeData && customSubMesh.mIsSDFMesh)
		{
			bool twoSided = customSubMesh.mIsTwoSided;
			eastl::string sdfBakedDataFileName = customSubMesh.mMeshName;
			SDFMeshInstance& sdfMeshInstance = mesh.mMeshInstances[i];

			sdfMeshInstance.mIsAlphaTested = customSubMesh.mIsAlphaTested;

			SDFVolumeData* volumeData = NULL;

			ivec3 specialVoxelSizeValue(0);

			if (customSubMesh.mUseDoubleVoxelSize)
			{
				specialVoxelSizeValue = ivec3(
					SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_X,
					SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Y,
					SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Z);
			}

			const eastl::string fileNameToCheck = SDFVolumeData::GetFullDirBakedFileName(sdfBakedDataFileName);
			SDFVolumeData::GenerateVolumeData(&volumeData, fileNameToCheck,
				SDFVolumeData::GetBakedFileName(sdfBakedDataFileName), customSubMesh.mTwoSidedWorldSpaceBias);

			if (volumeData)
			{
				volumeData->mSubMeshName = customSubMesh.mMeshName;
				sdfVolumeInstances.push_back(volumeData);
				sdfMeshInstance.mHasGeneratedSDFVolumeData = true;
				++mesh.mTotalGeneratedSDFMeshes;
			}
			++mesh.mTotalSDFMeshes;
			
		}

		customSubMesh.m_PSubMesh = NULL;
		mesh.mCustomSubMeshDataList.push_back(customSubMesh);
	}

	if (totalVertexCount == 0 || totalIndexCount == 0)
	{
		return;
	}	
	conf_free(vertices);
	conf_free(indices);
}

void loadSDFMesh(ThreadSystem* threadSystem, const eastl::string& fileName, SDFMesh* outMesh, float scale,
	float offsetX, bool generateSDFVolumeData, BakedSDFVolumeInstances& sdfVolumeInstances)

{
	AssimpImporter importer;
	AssimpImporter::Model model;
	eastl::string sceneFullPath = FileSystem::FixPath(fileName + ".obj", FSRoot::FSR_Meshes);
	if (!importer.ImportModel(sceneFullPath.c_str(), &model))
	{
		ErrorMsg("Failed to load %s", FileSystem::GetFileNameAndExtension(sceneFullPath).c_str());
		return;
	}

			
	uint32_t meshCount = (uint32_t)model.mMeshArray.size();

	SDFMesh& mesh = *outMesh;

	mesh.mMeshInstances.resize(meshCount);

	Vertex* vertices = NULL;
	uint32_t*   indices = NULL;

	uint32_t totalVertexCount = 0;
	uint32_t totalIndexCount = 0;

	AABBox overallBoundingBox;

	eastl::vector<SDFCustomSubMeshData> SDFCustomSubMeshDataList;

	uint32_t realChildrenCount = 0;
	for (uint32_t i = 0; i < (uint32_t)model.mRootNode.mChildren.size(); ++i)
	{
		eastl::string curName = model.mRootNode.mChildren[i].mName;
		if (curName == "default")
		{
			continue;
		}

		char spaceChar = ' ';
		eastl::string::iterator afterFirstSpaceName = eastl::find(&curName.front(), &curName.back(), spaceChar);


		eastl::string newStr = afterFirstSpaceName;

		eastl::string::iterator removeBeginIter = eastl::remove_if(newStr.begin(), newStr.end(), isspace);

		newStr.erase(removeBeginIter, newStr.end());

		if (newStr.find("sdf") != eastl::string::npos)
		{
			SDFCustomSubMeshDataList.push_back(SDFCustomSubMeshData{});
			SDFCustomSubMeshData& customSubMesh = SDFCustomSubMeshDataList.back();
			GetSDFCustomSubMeshData(newStr, customSubMesh);
			customSubMesh.m_PSubMesh = &model.mMeshArray[realChildrenCount];
		}

		//LOGF(LogLevel::eINFO, "Node mesh instance %d name %s", realChildrenCount, newStr.c_str());
		++realChildrenCount;
	}

	for (uint32_t i = 0; i < SDFCustomSubMeshDataList.size(); ++i)
	{
		SDFCustomSubMeshData& customSubMesh = SDFCustomSubMeshDataList[i];
		AssimpImporter::Mesh& subMesh = *customSubMesh.m_PSubMesh;

		uint32_t vertexCount = (uint32_t)subMesh.mPositions.size();
		uint32_t indexCount = (uint32_t)subMesh.mIndices.size();

		mesh.mMeshInstances[i] = { i, indexCount, totalIndexCount, totalVertexCount };

		mesh.mMeshInstances[i].mMainMesh = &mesh;

		vertices = (Vertex*)conf_realloc(vertices, sizeof(Vertex) * (totalVertexCount + vertexCount));
		indices = (uint32_t*)conf_realloc(indices, sizeof(uint32_t) * (totalIndexCount + indexCount));

		for (uint32_t j = 0; j < vertexCount; j++)
		{
			float3 transformedV = subMesh.mPositions[j] * scale + float3(offsetX, 0.f, 0.f);

			vec2 texCoord = f2Tov2(subMesh.mUvs[j]);

			mesh.mUncompressedTexCoords.push_back(texCoord);

			//vertices[totalVertexCount++] = { subMesh.mPositions[j], subMesh.mNormals[j] };
			vertices[totalVertexCount++] = { transformedV , subMesh.mNormals[j] };
			mesh.mPositions.push_back(SceneVertexPos{ transformedV.getX(), transformedV.getY(), transformedV.getZ() });
			mesh.mUncompressedNormals.push_back(f3Tov3(subMesh.mNormals[j]));

			//SDF STUFF//
			mesh.mMeshInstances[i].mLocalBoundingBox.Add(vec3(transformedV.x,
				transformedV.y, transformedV.z));

			overallBoundingBox.Add(vec3(transformedV.x,
				transformedV.y, transformedV.z));
			//
		}

		uint32_t accIndex_ = 0;

		for (uint32_t j = 0; j < indexCount; j++)
		{

			int indd = subMesh.mIndices[j];

			if (indd < 0)
			{
				indices[totalIndexCount++] = accIndex_;
				mesh.mIndices.push_back(accIndex_);
			}
			else
			{
				indices[totalIndexCount++] = (indd + accIndex_);
				mesh.mIndices.push_back((indd + accIndex_));
			}
		}

		if (generateSDFVolumeData && customSubMesh.mIsSDFMesh)
		{
			bool twoSided = customSubMesh.mIsTwoSided;
			eastl::string sdfBakedDataFileName = customSubMesh.mMeshName;

			SDFMeshInstance& sdfMeshInstance = mesh.mMeshInstances[i];

			SDFVolumeData* volumeData = NULL;

			ivec3 specialVoxelSizeValue(0);

			if (customSubMesh.mUseDoubleVoxelSize)
			{
				specialVoxelSizeValue = ivec3(
					SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_X,
					SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Y,
					SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Z);
			}
			/*SDFVolumeData::GenerateVolumeData(threadSystem, &mesh,
				&sdfMeshInstance, 1.f, twoSided, &volumeData,
				sdfBakedDataFileName, customSubMesh.mTwoSidedWorldSpaceBias, specialVoxelSizeValue);*/

			const eastl::string fileNameToCheck = SDFVolumeData::GetFullDirBakedFileName(sdfBakedDataFileName);
			SDFVolumeData::GenerateVolumeData(&volumeData, fileNameToCheck, 
				SDFVolumeData::GetBakedFileName(sdfBakedDataFileName), customSubMesh.mTwoSidedWorldSpaceBias);

			if (volumeData)
			{
				volumeData->mSubMeshName = customSubMesh.mMeshName;
				sdfVolumeInstances.push_back(volumeData);
				sdfMeshInstance.mHasGeneratedSDFVolumeData = true;
				++mesh.mTotalGeneratedSDFMeshes;
			}
			++mesh.mTotalSDFMeshes;
			
		}
		customSubMesh.m_PSubMesh = NULL;
		mesh.mCustomSubMeshDataList.push_back(customSubMesh);

		accIndex_ += (uint32_t)subMesh.mPositions.size();
	}

	if (totalVertexCount == 0 || totalIndexCount == 0)
	{
		return;
	}

	conf_free(vertices);
	conf_free(indices);
}

void generateMissingSDF(ThreadSystem* threadSystem, SDFMesh* sdfMesh, BakedSDFVolumeInstances& sdfVolumeInstances)
{
	const CustomSDFSubMeshDataList& customList = sdfMesh->mCustomSubMeshDataList;

	for (uint32_t i = 0; i < customList.size(); ++i)
	{
		const SDFCustomSubMeshData& customSubMesh = customList[i];
		if (!customSubMesh.mIsSDFMesh)
		{
			continue;
		}
		SDFMeshInstance& sdfMeshInstance = sdfMesh->mMeshInstances[i];

		if (sdfMeshInstance.mHasGeneratedSDFVolumeData)
		{
			continue;
		}

		bool twoSided = customSubMesh.mIsTwoSided;
		eastl::string sdfBakedDataFileName = customSubMesh.mMeshName;

		SDFVolumeData* volumeData = NULL;

		ivec3 specialVoxelSizeValue(0);

		if (customSubMesh.mUseDoubleVoxelSize)
		{
			specialVoxelSizeValue = ivec3(
				SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_X,
				SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Y,
				SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Z);
		}

		SDFVolumeData::GenerateVolumeData(threadSystem, sdfMesh,
			&sdfMeshInstance, 1.f, twoSided, &volumeData,
			sdfBakedDataFileName, customSubMesh.mTwoSidedWorldSpaceBias, specialVoxelSizeValue);

		if (volumeData)
		{
			volumeData->mSubMeshName = customSubMesh.mMeshName;
			sdfVolumeInstances.push_back(volumeData);
			sdfMeshInstance.mHasGeneratedSDFVolumeData = true;
			++sdfMesh->mTotalGeneratedSDFMeshes;
		}
	}
}
