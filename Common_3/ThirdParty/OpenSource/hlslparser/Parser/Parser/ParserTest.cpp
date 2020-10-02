

#include "Parser.h"
#include "MCPPPreproc.h"

#include "StringLibrary.h"

#include "HLSLParser.h"

#include "GLSLGenerator.h"
#include "HLSLGenerator.h"
#include "MSLGenerator.h"

#include <direct.h>


#pragma warning(disable:4996)

static eastl::string ReadFile(const char* fileName)
{
	std::ifstream ifs(fileName);
	std::stringstream buffer;
	buffer << ifs.rdbuf();
	return eastl::string(buffer.str().c_str());
}


static bool WriteFile(const char* fileName, const char* contents)
{
	//check first if there is right folder along the path of filename
	//And make directory	

	size_t found;
	eastl::string FilenameStr(fileName);
	found = FilenameStr.find_last_of("/\\");
	eastl::string DirnameStr = FilenameStr.substr(0, found);

	_mkdir(DirnameStr.c_str());

	std::ofstream ofs(fileName);

	ofs << contents;
	ofs.close();
	return true;
}

struct ShaderTestItem
{
	ShaderTestItem()
	{
		mName = "";
		for (int i = 0; i < 6; i++)
		{
			mIsStage[i] = false;
		}
	}

	void AddVs()
	{
		mIsStage[0] = true;
	}

	void AddFs()
	{
		mIsStage[1] = true;
	}

	void AddCs()
	{
		mIsStage[2] = true;
	}

	void AddHs()
	{
		mIsStage[3] = true;
	}

	void AddDs()
	{
		mIsStage[4] = true;
	}

	void AddGs()
	{
		mIsStage[5] = true;
	}

	eastl::string mName;
	bool mIsStage[6];

	eastl::string mEntry;
	eastl::string mVariation;

	eastl::vector < eastl::string > mMacroLhs;
	eastl::vector < eastl::string > mMacroRhs;
};

// hull/domain/geometry shaders are rare eneough that we can add their flags manually
ShaderTestItem MakeTestItem(const eastl::string & mName, bool isVs, bool isFs, bool isCs, const eastl::vector < eastl::string > & mMacroLhs, const eastl::vector < eastl::string > & mMacroRhs)
{
	ShaderTestItem item;
	item.mName = mName;
	item.mMacroLhs = mMacroLhs;
	item.mMacroRhs = mMacroRhs;

	// all the Forge test shaders enter at main(), but int larger engines it's very common to have a different entry point.
	// for example, if you want multiple entries in the file, they can't all be called main()
	item.mEntry = "main"; 
	item.mVariation = "";
	item.mIsStage[0] = isVs;
	item.mIsStage[1] = isFs;
	item.mIsStage[2] = isCs;
	return item;
}

ShaderTestItem MakeTestItemVariation(const eastl::string & mName, bool isVs, bool isFs, bool isCs, const eastl::vector < eastl::string > & mMacroLhs, const eastl::vector < eastl::string > & mMacroRhs, const eastl::string & variation)
{
	ShaderTestItem item;
	item.mName = mName;
	item.mMacroLhs = mMacroLhs;
	item.mMacroRhs = mMacroRhs;

	// all the Forge test shaders enter at main(), but int larger engines it's very common to have a different entry point.
	// for example, if you want multiple entries in the file, they can't all be called main()
	item.mEntry = "main";
	item.mVariation = variation;
	item.mIsStage[0] = isVs;
	item.mIsStage[1] = isFs;
	item.mIsStage[2] = isCs;
	return item;
}

int BuildShaderGroup(const eastl::string & dstBaseDir, const eastl::string & srcDir, const eastl::vector < ShaderTestItem > & testItems, const eastl::vector < BindingOverride > * bindingVec)
{
	int result = 0;

	_mkdir(dstBaseDir.c_str());

	_mkdir((dstBaseDir + "Shaders/").c_str());

	_mkdir((dstBaseDir + "Shaders/D3D12").c_str());
	_mkdir((dstBaseDir + "Shaders/Vulkan").c_str());
	_mkdir((dstBaseDir + "Shaders/Metal").c_str());

	FILE * logFile = fopen((dstBaseDir + "log.txt").c_str(),"w");
	ASSERT_PARSER(logFile != NULL);

	for (int itemIter = 0; itemIter < testItems.size(); itemIter++)
	{
		const ShaderTestItem & currItem = testItems[itemIter];

		const char * stageNames[] = 
		{
			"vert",
			"frag",
			"comp",
			"tesc",
			"tese",
			"geom",
		};

		// note: the enums are NOT in numerical order
		const Parser::Target targetList[] = 
		{
			Parser::Target_VertexShader,
			Parser::Target_FragmentShader,
			Parser::Target_ComputeShader,
			Parser::Target_HullShader,
			Parser::Target_DomainShader,
			Parser::Target_GeometryShader,
		};

		int platformTarget = 0;


		const Parser::Language languageList[] =
		{
			Parser::Language_HLSL,
			Parser::Language_GLSL,
			Parser::Language_MSL
		};

		const char * platformNameList[] =
		{
			"D3D12",
			"Vulkan",
			"Metal"
		};

		for (int platformTarget = 0; platformTarget < 3; platformTarget++)		
		{
			eastl::string platformName = platformNameList[platformTarget];
			eastl::vector < eastl::string > platformMacroLhs;
			eastl::vector < eastl::string > platformMacroRhs;

			platformMacroLhs.push_back("HLSL");
			platformMacroLhs.push_back("GLSL");
			platformMacroLhs.push_back("MSL");
			platformMacroRhs.push_back(platformTarget == 0 ? "1" : "0");
			platformMacroRhs.push_back(platformTarget == 1 ? "1" : "0");
			platformMacroRhs.push_back(platformTarget == 2 ? "1" : "0");

			// Special macros used to specify update frequency as a macro for more clarity
			const uint32_t updateFrequencyCount = 4;
			const char* pUpdateFreqNames[updateFrequencyCount] =
			{
				"UPDATE_FREQ_NONE",
				"UPDATE_FREQ_PER_FRAME",
				"UPDATE_FREQ_PER_BATCH",
				"UPDATE_FREQ_PER_DRAW",
			};
			const char* pUpdateFreqValues[updateFrequencyCount] =
			{
				"space0",
				"space1",
				"space2",
				"space3",
			};

			for (uint32_t i = 0; i < updateFrequencyCount; ++i)
			{
				platformMacroLhs.push_back(pUpdateFreqNames[i]);
				platformMacroRhs.push_back(pUpdateFreqValues[i]);
			}

			eastl::string dstDir = dstBaseDir + "Shaders/" + platformName + "/";

			for (int stageIter = 0; stageIter < 6; stageIter++)
			{
				eastl::string stage = stageNames[stageIter];
				if (currItem.mIsStage[stageIter])
				{
					fprintf(logFile,"%d/%d (%s - %s): %s\n", itemIter, (int)testItems.size(), stage.c_str(), platformName.c_str(), currItem.mName.c_str());

					// preprocess
					eastl::string preprocData;
					bool preprocSuccess = false;

					eastl::string baseName = currItem.mName;
					if (currItem.mVariation.size() != 0)
					{
						baseName = baseName + "_" + currItem.mVariation;
					}

					eastl::string srcName = srcDir + currItem.mName + "." + stage;

					Parser::Target target = targetList[stageIter];

					eastl::vector < eastl::string > macroLhs;
					eastl::vector < eastl::string > macroRhs;
					{
						//eastl::vector < eastl::string > macroLhs = currItem.mMacroLhs;
						//eastl::vector < eastl::string > macroRhs = currItem.mMacroRhs;

						for (int i = 0; i < currItem.mMacroLhs.size(); i++)
						{
							macroLhs.push_back(currItem.mMacroLhs[i].c_str());
						}

						for (int i = 0; i < currItem.mMacroRhs.size(); i++)
						{
							macroRhs.push_back(currItem.mMacroRhs[i].c_str());
						}

						for (int i = 0; i < platformMacroLhs.size(); i++)
						{
							macroLhs.push_back(platformMacroLhs[i].c_str());
							macroRhs.push_back(platformMacroRhs[i].c_str());
						}
					}

					Parser::Options options;
					Parser::ParsedData parsedData;

					//options.mDebugPreprocFile = 

					eastl::string dstPreprocName = dstDir + baseName + "_" + stage + "_preproc.txt";
					eastl::string dstTokenName = dstDir + baseName + "_" + stage + "_token.txt";
					eastl::string dstGeneratedName = dstDir + baseName + "." + stage;

					if (platformTarget == 2)
					{
						dstGeneratedName += ".metal";
					}

					options.mDebugPreprocEnable = true;
					options.mDebugPreprocFile = dstPreprocName;
					options.mDebugTokenEnable = true;
					options.mDebugTokenFile = dstTokenName;
					options.mGeneratedWriteEnable = true;
					options.mGeneratedWriteFile = dstGeneratedName;
					options.mLanguage = languageList[platformTarget];
					options.mOperation = Parser::Operation_Generate;
					options.mTarget = target;
					
					if (bindingVec != nullptr)
					{
						options.mOverrideRequired = true;
						options.mOverrideVec = *bindingVec;
					}


					result |= !Parser::ProcessFile(parsedData, srcName, "main", options, macroLhs, macroRhs);

				}

				fprintf(logFile, "\n");
			}
		}
	}

	fclose(logFile);
	logFile = NULL;

	return result;
}

int ParserTest()
{
	int result = 0;
	printf("Testing parser.\n");

	// hack these paths for now
	eastl::string srcBaseDir = "../../../../../../Examples_3/Unit_Tests/src/";

	//eastl::string dstBaseDir = "../../../src/27_ShaderTranslator/Shaders/";//"D:/Forge/The-Forge/Examples_3/Unit_Tests/src/26_ShaderTranslator/Shaders/";
	eastl::string dstBaseDir = "TestData/";//"D:/Forge/The-Forge/Examples_3/Unit_Tests/src/26_ShaderTranslator/Shaders/";

	_mkdir(dstBaseDir.c_str());
	_mkdir((dstBaseDir + "built/").c_str());

	eastl::vector < eastl::string > importanceSampleLhs = { "IMPORTANCE_SAMPLE_COUNT" };
	eastl::vector < eastl::string > importanceSampleRhs = { "128" };

	eastl::vector < eastl::string > irradianceSampleLhs = { "SAMPLE_DELTA" };
	eastl::vector < eastl::string > irradianceSampleRhs = { "0.025f" };

	// 01
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "01_Transformations/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/01_Transformations/";
		

		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("basic", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("skybox", true, true, false, {}, {}));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 02
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "02_Compute/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/02_Compute/";

		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("compute", false, false, true, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("display", true, true, false, emptyLhs, emptyRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 03
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "03_MultiThread/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/03_MultiThread/";

		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("Graph", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("Particle", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("Skybox", true, true, false, emptyLhs, emptyRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 04
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "04_ExecuteIndirect/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/04_ExecuteIndirect/";

		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("basic", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("skybox", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("ExecuteIndirect", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("ComputeUpdate", false, false, true, emptyLhs, emptyRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 05
	{
		// none
	}

	// 06
	if (0)
	{
		eastl::vector < BindingOverride > bindingVec;
		// textures
		bindingVec.push_back(BindingOverride("dstTexture", 0, 0 ));
		bindingVec.push_back(BindingOverride("srcTexture", 0, 1));
		bindingVec.push_back(BindingOverride("ColorsTexture", 0, 2));
		bindingVec.push_back(BindingOverride("InvAlphaTexture", 0, 3));
		bindingVec.push_back(BindingOverride("brdfIntegrationMap", 0, 4));
		bindingVec.push_back(BindingOverride("irradianceMap", 0, 5));
		bindingVec.push_back(BindingOverride("albedoMap", 0, 6));
		bindingVec.push_back(BindingOverride("normalMap", 0, 7));
		bindingVec.push_back(BindingOverride("metallicMap", 0, 8));
		bindingVec.push_back(BindingOverride("roughnessMap", 0, 9));
		bindingVec.push_back(BindingOverride("aoMap", 0, 10));
		bindingVec.push_back(BindingOverride("shadowMap", 0, 11));
		bindingVec.push_back(BindingOverride("specularMap", 0, 12));
		bindingVec.push_back(BindingOverride("skyboxTex", 0, 13));
		bindingVec.push_back(BindingOverride("DirectionalLightShadowMaps", 0, 14));

		// buffers
		bindingVec.push_back(BindingOverride("RootConstant", 0, 0));
		bindingVec.push_back(BindingOverride("cbSimulation", 0, 1));
		bindingVec.push_back(BindingOverride("cbHairGlobal", 0, 2));
		bindingVec.push_back(BindingOverride("cbCamera", 0, 3));
		bindingVec.push_back(BindingOverride("cbObject", 0, 4));
		bindingVec.push_back(BindingOverride("cbHair", 0, 5));
		bindingVec.push_back(BindingOverride("cbHairGlobal", 0, 6));
		bindingVec.push_back(BindingOverride("cbPointLights", 0, 7));
		bindingVec.push_back(BindingOverride("cbDirectionalLights", 0, 8));
		bindingVec.push_back(BindingOverride("cbDirectionalLightShadowCameras", 0, 9));
		bindingVec.push_back(BindingOverride("cbPointLights", 0, 10));
		bindingVec.push_back(BindingOverride("cbDirectionalLights", 0, 11));
		bindingVec.push_back(BindingOverride("CapsuleRootConstant", 0, 12));
		bindingVec.push_back(BindingOverride("uniformBlock", 0, 13));
		bindingVec.push_back(BindingOverride("HairVertexPositions", 0, 14));
		bindingVec.push_back(BindingOverride("HairVertexPositionsPrev", 0, 15));
		bindingVec.push_back(BindingOverride("HairVertexPositionsPrevPrev", 0, 16));
		bindingVec.push_back(BindingOverride("HairRestPositions", 0, 17));
		bindingVec.push_back(BindingOverride("HairVertexTangents", 0, 18));
		bindingVec.push_back(BindingOverride("FollowHairRootOffsets", 0, 19));
		bindingVec.push_back(BindingOverride("HairRestLengths", 0, 20));
		bindingVec.push_back(BindingOverride("HairRefsInLocalFrame", 0, 21));
		bindingVec.push_back(BindingOverride("HairGlobalRotations", 0, 22));
		bindingVec.push_back(BindingOverride("DepthsTexture", 0, 23));
		bindingVec.push_back(BindingOverride("HairThicknessCoefficients", 0, 24));
		bindingVec.push_back(BindingOverride("GuideHairVertexPositions", 0, 25));
		bindingVec.push_back(BindingOverride("GuideHairVertexTangents", 0, 26));
		bindingVec.push_back(BindingOverride("HairThicknessCoefficients", 0, 27));

		// samplers
		bindingVec.push_back(BindingOverride("skyboxSampler", 0, 0));
		bindingVec.push_back(BindingOverride("PointSampler", 0, 1));
		bindingVec.push_back(BindingOverride("bilinearSampler", 0, 2));
		bindingVec.push_back(BindingOverride("bilinearClampedSampler", 0, 3));

		eastl::string srcDir = srcBaseDir + "06_MaterialPlayground/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/06_MaterialPlayground/";
		
		eastl::vector < ShaderTestItem > itemList;

		if (1)
		{
			eastl::vector < eastl::string > macroLhs;
			eastl::vector < eastl::string > macroRhs;
			macroLhs.push_back("IMPORTANCE_SAMPLE_COUNT");
			macroLhs.push_back("MAX_NUM_POINT_LIGHTS");
			macroLhs.push_back("MAX_NUM_DIRECTIONAL_LIGHTS");
			macroRhs.push_back("128");
			macroRhs.push_back("8");
			macroRhs.push_back("1");


			eastl::vector < eastl::string > emptyLhs;
			eastl::vector < eastl::string > emptyRhs;

			itemList.push_back(MakeTestItem("BRDFIntegration", false, false, true, importanceSampleLhs, importanceSampleRhs));
			itemList.push_back(MakeTestItem("computeIrradianceMap", false, false, true, irradianceSampleLhs, irradianceSampleRhs));
			itemList.push_back(MakeTestItem("computeSpecularMap", false, false, true, importanceSampleLhs, importanceSampleRhs));
			itemList.push_back(MakeTestItem("fullscreen", true, false, false, emptyLhs, emptyRhs));
			itemList.push_back(MakeTestItem("panoToCube", false, false, true, emptyLhs, emptyRhs));
			itemList.push_back(MakeTestItem("renderSceneBRDF", true, true, false, macroLhs, macroRhs));
			itemList.push_back(MakeTestItem("renderSceneShadows", true, true, false, emptyLhs, emptyRhs));
			itemList.push_back(MakeTestItem("showCapsules", true, true, false, emptyLhs, emptyRhs));
			itemList.push_back(MakeTestItem("skeleton", true, true, false, emptyLhs, emptyRhs));
			itemList.push_back(MakeTestItem("skybox", true, true, false, emptyLhs, emptyRhs));
			itemList.push_back(MakeTestItem("hair", true, true, false, macroLhs, macroRhs));
		}

		eastl::vector < eastl::string > hairVariations;
		hairVariations.push_back("SHORT_CUT_CLEAR");
		hairVariations.push_back("SHORT_CUT_DEPTH_PEELING");
		hairVariations.push_back("SHORT_CUT_RESOLVE_DEPTH");
		hairVariations.push_back("SHORT_CUT_FILL_COLOR");
		hairVariations.push_back("SHORT_CUT_RESOLVE_COLOR");
		hairVariations.push_back("HAIR_SHADOW");
		hairVariations.push_back("HAIR_INTEGRATE");
		hairVariations.push_back("HAIR_SHOCK_PROPAGATION");
		hairVariations.push_back("HAIR_LOCAL_CONSTRAINTS");
		hairVariations.push_back("HAIR_LENGTH_CONSTRAINTS");
		hairVariations.push_back("HAIR_UPDATE_FOLLOW_HAIRS");
		hairVariations.push_back("HAIR_PRE_WARM");

		eastl::vector < int > hairIsFrag;
		hairIsFrag.push_back(true);
		hairIsFrag.push_back(true);
		hairIsFrag.push_back(true);
		hairIsFrag.push_back(true);
		hairIsFrag.push_back(true);
		hairIsFrag.push_back(true);
		hairIsFrag.push_back(false);
		hairIsFrag.push_back(false);
		hairIsFrag.push_back(false);
		hairIsFrag.push_back(false);
		hairIsFrag.push_back(false);
		hairIsFrag.push_back(false);

		for (int i = 0; i < hairVariations.size(); i++)
		{
			eastl::vector < eastl::string > macroLhs;

			macroLhs.push_back("MAX_NUM_POINT_LIGHTS");
			macroLhs.push_back("MAX_NUM_DIRECTIONAL_LIGHTS");
			macroLhs.push_back("HAIR_MAX_CAPSULE_COUNT");

			eastl::vector < eastl::string > macroRhs;
			macroRhs.push_back("8");
			macroRhs.push_back("8");
			macroRhs.push_back("3");

			eastl::string variation = hairVariations[i];
			macroLhs.push_back(variation);
			macroRhs.push_back("1");

			bool isFrag = hairIsFrag[i];

			itemList.push_back(MakeTestItemVariation("hair", false, isFrag, !isFrag, macroLhs, macroRhs, variation));
		}

		result |= BuildShaderGroup(dstDir,srcDir,itemList, &bindingVec);
	}

	// 07
	if (0)
	{
		eastl::string srcDir = srcBaseDir + "07_Tessellation/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/07_Tessellation/";

		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("compute", false, false, true, emptyLhs, emptyRhs));
		ShaderTestItem grass = MakeTestItem("grass", true, true, false, emptyLhs, emptyRhs);
		grass.AddHs();
		grass.AddDs();

		itemList.push_back(grass);

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 08
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "08_GltfViewer/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/08_GltfViewer/";


		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < ShaderTestItem > itemList;
		//itemList.push_back(MakeTestItem("basic", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("floor", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("FXAA", false, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("Triangular", true, false, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("vignette", false, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("watermark", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("zPass", true, false, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("zPassFloor", true, false, false, emptyLhs, emptyRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 09
	if (0)
	{
		eastl::string srcDir = srcBaseDir + "09_LightShadowPlayground/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/09_LightShadowPlayground/";

		eastl::vector < ShaderTestItem > itemList;

		eastl::vector < eastl::string > currLhs;
		eastl::vector < eastl::string > currRhs;

		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		
		currLhs.push_back("ESM_MSAA_SAMPLES");
		currLhs.push_back("COPY_BUFFER_WORKGROUP");
		currLhs.push_back("ESM_SHADOWMAP_RES");
		currRhs.push_back("2");
		currRhs.push_back("16");
		currRhs.push_back("2048u");

		itemList.push_back(MakeTestItem("bakedSDFMeshShadow", false, false, true, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("batchCompaction", false, false, true, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("clearVisibilityBuffers", false, false, true, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("copyDEMQuads", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("copyDepthQuads", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("display", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("fill_Indirection", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("generateAsmDEM", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("meshDepthPass", true, false, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("meshDepthPassAlpha", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("panoToCube", false, false, true, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("quad", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("skybox", false, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("triangleFiltering", false, false, true, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("updateRegion3DTexture", false, false, true, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("upsampleSDFShadow", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("visibilityBufferPass", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("visibilityBufferPassAlpha", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("visibilityBufferShade", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("visualizeSDFMesh", false, false, true, emptyLhs, emptyRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 09a
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "09a_HybridRaytracing/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/09a_HybridRaytracing/";

		eastl::vector < eastl::string > currLhs;
		eastl::vector < eastl::string > currRhs;

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("compositePass", false, false, true, currLhs, currRhs));
		itemList.push_back(MakeTestItem("display", true, true, false, currLhs, currRhs));
		itemList.push_back(MakeTestItem("gbufferPass", true, true, false, currLhs, currRhs));
		itemList.push_back(MakeTestItem("lightingPass", false, false, true, currLhs, currRhs));
		itemList.push_back(MakeTestItem("raytracedShadowsPass", false, false, true, currLhs, currRhs));
		itemList.push_back(MakeTestItem("raytracedShadowsUpscalePass", false, false, true, currLhs, currRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 10
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "10_ScreenSpaceReflections/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/10_ScreenSpaceReflections/";


		eastl::vector < eastl::string > macroLhs, macroRhs;
		macroLhs.push_back("IMPORTANCE_SAMPLE_COUNT");
		macroLhs.push_back("MAX_NUM_POINT_LIGHTS");
		macroLhs.push_back("MAX_NUM_DIRECTIONAL_LIGHTS");
		macroRhs.push_back("128");
		macroRhs.push_back("8");
		macroRhs.push_back("1");


		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("BRDFIntegration", false, false, true, importanceSampleLhs, importanceSampleRhs));
		itemList.push_back(MakeTestItem("computeIrradianceMap", false, false, true, irradianceSampleLhs, irradianceSampleRhs));
		itemList.push_back(MakeTestItem("computeSpecularMap", false, false, true, importanceSampleLhs, importanceSampleRhs));
		itemList.push_back(MakeTestItem("fillGbuffers", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("panoToCube", false, false, true, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("PPR_Holepatching", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("PPR_Projection", false, false, true, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("PPR_Reflection", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("renderSceneBRDF", true, true, false, macroLhs, macroRhs));
		itemList.push_back(MakeTestItem("skybox", true, true, false, emptyLhs, emptyRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 11
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "11_MultiGPU/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/11_MultiGPU/";

		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("basic", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("skybox", true, true, false, emptyLhs, emptyRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 12
	{
		// none
	}

	// 13
	{
		 // none
	}

	// 14
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "14_WaveIntrinsics/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/14_WaveIntrinsics/";

		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("magnify", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("wave", true, true, false, emptyLhs, emptyRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 15
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "15_Transparency/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/15_Transparency/";

		eastl::vector < ShaderTestItem > itemList;

		eastl::vector < eastl::string > macroLhs, macroRhs;
		macroLhs.push_back("MAX_NUM_TEXTURES");
		macroRhs.push_back("8");
		macroLhs.push_back("MAX_NUM_OBJECTS");
		macroRhs.push_back("128");
		macroLhs.push_back("USE_SHADOWS");
		macroRhs.push_back("1");
		itemList.push_back(MakeTestItem("AdaptiveOIT", false, true, false, macroLhs, macroRhs));
		itemList.push_back(MakeTestItem("AdaptiveOITClear", false, true, false, macroLhs, macroRhs));
		itemList.push_back(MakeTestItem("AdaptiveOITComposite", false, true, false, macroLhs, macroRhs));
		itemList.push_back(MakeTestItem("copy", false, true, false, macroLhs, macroRhs));
		itemList.push_back(MakeTestItem("downsample", false, true, false, macroLhs, macroRhs));
		itemList.push_back(MakeTestItem("forward", true, true, false, macroLhs, macroRhs));
		itemList.push_back(MakeTestItem("fullscreen", true, false, false, macroLhs, macroRhs));
		itemList.push_back(MakeTestItem("gaussianBlur", false, true, false, macroLhs, macroRhs));
		itemList.push_back(MakeTestItem("generateMips", false, false, true, macroLhs, macroRhs));
		itemList.push_back(MakeTestItem("phenomenologicalTransparency", false, true, false, macroLhs, macroRhs));
		itemList.push_back(MakeTestItem("phenomenologicalTransparencyComposite", false, true, false, macroLhs, macroRhs));
		itemList.push_back(MakeTestItem("shadow", true, true, false, macroLhs, macroRhs));
		itemList.push_back(MakeTestItem("skybox", true, true, false, macroLhs, macroRhs));
		itemList.push_back(MakeTestItem("stochasticShadow", false, true, false, macroLhs, macroRhs));
		itemList.push_back(MakeTestItem("weightedBlendedOIT", false, true, false, macroLhs, macroRhs));
		itemList.push_back(MakeTestItem("weightedBlendedOITComposite", false, true, false, macroLhs, macroRhs));
		itemList.push_back(MakeTestItem("weightedBlendedOITVolition", false, true, false, macroLhs, macroRhs));
		itemList.push_back(MakeTestItem("weightedBlendedOITVolitionComposite", false, true, false, macroLhs, macroRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 16
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "16_Raytracing/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/16_Raytracing/";

		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("DisplayTexture", true, true, false, emptyLhs, emptyRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 16a
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "16a_SphereTracing/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/16a_SphereTracing/";

		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("fstri", true, false, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("rt", false, true, false, emptyLhs, emptyRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 17
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "17_EntityComponentSystem/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/17_EntityComponentSystem/";

		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("basic", true, true, false, emptyLhs, emptyRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 18
	{
		// none
	}

	// 19
	{
		// none
	}

	// 20
	{
		// none
	}

	// 21
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "21_Playback/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/21_Playback/";

		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("basic", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("plane", true, true, false, emptyLhs, emptyRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 22
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "22_Blending/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/22_Blending/";

		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("basic", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("plane", true, true, false, emptyLhs, emptyRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 23
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "23_JointAttachment/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/23_JointAttachment/";

		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("basic", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("plane", true, true, false, emptyLhs, emptyRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 24
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "24_PartialBlending/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/24_PartialBlending/";

		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("basic", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("plane", true, true, false, emptyLhs, emptyRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 25
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "25_AdditiveBlending/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/25_AdditiveBlending/";

		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("basic", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("plane", true, true, false, emptyLhs, emptyRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 26
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "26_BakedPhysics/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/26_BakedPhysics/";

		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("basic", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("plane", true, true, false, emptyLhs, emptyRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 27
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "27_MultiThread/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/27_MultiThread/";

		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("basic", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("plane", true, true, false, emptyLhs, emptyRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 28
	if (1)
	{
		eastl::string srcDir = srcBaseDir + "28_Skinning/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/28_Skinning/";

		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < eastl::string > boneLhs, boneRhs;
		boneLhs.push_back("MAX_NUM_BONES");
		boneRhs.push_back("1024");

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("basic", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("plane", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("skinning", true, true, false, boneLhs,boneRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	// 29
	{
		// none
	}

	// Visibility Buffer
	if (0)
	{
		eastl::string srcDir = srcBaseDir + "../../Visibility_Buffer/src/Shaders/D3D12/";
		eastl::string dstDir = dstBaseDir + "built/Visibility_Buffer/";

		eastl::vector < eastl::string > sampleLhs;
		eastl::vector < eastl::string > sampleRhs;
		sampleLhs.push_back("SAMPLE_COUNT");
		sampleRhs.push_back("2");

		eastl::vector < eastl::string > emptyLhs;
		eastl::vector < eastl::string > emptyRhs;

		eastl::vector < ShaderTestItem > itemList;
		itemList.push_back(MakeTestItem("batch_compaction", false, false, true, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("clear_buffers", false, false, true, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("clear_light_clusters", false, false, true, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("cluster_lights", false, false, true, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("CurveConversion", false, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("deferred_pass", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("deferred_pass_alpha", false, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("deferred_shade", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("deferred_shade_pointlight", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("display", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("godray", false, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("HDAO", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("panoToCube", false, false, true, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("resolve", true, true, false, sampleLhs, sampleRhs));
		itemList.push_back(MakeTestItem("resolveGodray", false, true, false, sampleLhs, sampleRhs));
		itemList.push_back(MakeTestItem("shadow_pass", true, false, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("shadow_pass_alpha", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("skybox", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("sun", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("triangle_filtering", false, false, true, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("visibilityBuffer_pass", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("visibilityBuffer_pass_alpha", true, true, false, emptyLhs, emptyRhs));
		itemList.push_back(MakeTestItem("visibilityBuffer_shade", true, true, false, emptyLhs, emptyRhs));

		result |= BuildShaderGroup(dstDir, srcDir, itemList, nullptr);
	}

	return result;
}


