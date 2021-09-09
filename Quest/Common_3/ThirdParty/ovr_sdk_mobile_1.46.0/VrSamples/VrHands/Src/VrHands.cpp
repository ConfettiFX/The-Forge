/************************************************************************************

Filename    :   VrHands.cpp
Content     :   Trivial use of the application framework.
Created     :
Authors     :	Jonathan E. Wright, Robert Memmott

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "VrHands.h"

#include "VrApi.h"

#include "GUI/GuiSys.h"
#include "GUI/DefaultComponent.h"
#include "GUI/ActionComponents.h"
#include "GUI/VRMenu.h"
#include "GUI/VRMenuObject.h"
#include "GUI/VRMenuMgr.h"
#include "GUI/Reflection.h"
#include "Locale/OVR_Locale.h"
#include "Misc/Log.h"

#include <memory>
#include <vector>
#include <algorithm>
#include <sstream>

using OVR::Axis_X;
using OVR::Axis_Y;
using OVR::Axis_Z;
using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

static const char* MenuDefinitionFile = R"menu_definition(
itemParms {
	// panel
	VRMenuObjectParms {
		Type = VRMENU_STATIC;
		Flags = VRMENUOBJECT_RENDER_HIERARCHY_ORDER;
		TexelCoords = true;
		SurfaceParms {
			VRMenuSurfaceParms {
				SurfaceName = "panel";
				ImageNames {
					string[0] = "apk:///assets/panel.ktx";
				}
				TextureTypes {
					eSurfaceTextureType[0] =  SURFACE_TEXTURE_DIFFUSE;
				}
				Color = ( 0.0f, 0.0f, 0.1f, 1.0f ); // MENU_DEFAULT_COLOR
				Border = ( 16.0f, 16.0f, 16.0f, 16.0f );
				Dims = ( 100.0f, 100.0f );
			}
		}
		Text = "Panel";
		LocalPose {
			Position = ( 0.0f, 00.0f, 0.0f );
			Orientation = ( 0.0f, 0.0f, 0.0f, 1.0f );
		}
		LocalScale = ( 100.0f, 100.0f, 1.0f );
		TextLocalPose {
			Position = ( 0.0f, 0.0f, 0.0f );
			Orientation = ( 0.0f, 0.0f, 0.0f, 1.0f );
		}
		TextLocalScale = ( 1.0f, 1.0f, 1.0f );
		FontParms {
			AlignHoriz = HORIZONTAL_CENTER;
			AlignVert = VERTICAL_CENTER;
			Scale = 0.5f;
		}
		ParentId = -1;
		Id = 0;
		Name = "panel";
	}
}
)menu_definition";

class SimpleTargetMenu : public OVRFW::VRMenu {
   public:
    static SimpleTargetMenu* Create(
        OVRFW::OvrGuiSys& guiSys,
        OVRFW::ovrLocale& locale,
        const std::string& menuName,
        const std::string& text) {
        return new SimpleTargetMenu(guiSys, locale, menuName, text);
    }

   private:
    SimpleTargetMenu(
        OVRFW::OvrGuiSys& guiSys,
        OVRFW::ovrLocale& locale,
        const std::string& menuName,
        const std::string& text)
        : OVRFW::VRMenu(menuName.c_str()) {
        std::vector<uint8_t> buffer;
        std::vector<OVRFW::VRMenuObjectParms const*> itemParms;

        size_t bufferLen = OVR::OVR_strlen(MenuDefinitionFile);
        buffer.resize(bufferLen + 1);
        memcpy(buffer.data(), MenuDefinitionFile, bufferLen);
        buffer[bufferLen] = '\0';

        OVRFW::ovrParseResult parseResult = OVRFW::VRMenuObject::ParseItemParms(
            guiSys.GetReflection(), locale, menuName.c_str(), buffer, itemParms);
        if (!parseResult) {
            DeletePointerArray(itemParms);
            ALOG("SimpleTargetMenu FAILED -> %s", parseResult.GetErrorText());
            return;
        }

        /// Hijack params
        for (auto* ip : itemParms) {
            // Find the one panel
            if ((int)ip->Id.Get() == 0) {
                const_cast<OVRFW::VRMenuObjectParms*>(ip)->Text = text;
            }
        }

        InitWithItems(
            guiSys,
            2.0f,
            OVRFW::VRMenuFlags_t(OVRFW::VRMENU_FLAG_SHORT_PRESS_HANDLED_BY_APP),
            itemParms);
    }

    virtual ~SimpleTargetMenu(){};
};

static const Vector4f LASER_COLOR(0.0f, 1.0f, 1.0f, 1.0f);
static const Vector4f MENU_DEFAULT_COLOR(0.0f, 0.0f, 0.1f, 1.0f);
static const Vector4f MENU_HIGHLIGHT_COLOR(0.8f, 1.0f, 0.8f, 1.0f);

HandSampleConfigurationParameters SampleConfiguration;

static_assert(MAX_JOINTS == 64, "MAX_JOINTS != 64");
const char* HandPBRSkinned1VertexShaderSrc = R"glsl(
	uniform JointMatrices
	{
		highp mat4 Joints[64];
	} jb;
	attribute highp vec4 Position;
	attribute highp vec3 Normal;
	attribute highp vec3 Tangent;
	attribute highp vec3 Binormal;
	attribute highp vec2 TexCoord;
	attribute highp vec4 JointWeights;
	attribute highp vec4 JointIndices;
	varying lowp vec3 oEye;
	varying lowp vec3 oNormal;
	varying lowp vec2 oTexCoord;
	vec3 multiply( mat4 m, vec3 v )
	{
		return vec3(
			m[0].x * v.x + m[1].x * v.y + m[2].x * v.z,
			m[0].y * v.x + m[1].y * v.y + m[2].y * v.z,
			m[0].z * v.x + m[1].z * v.y + m[2].z * v.z );
	}
	vec3 transposeMultiply( mat4 m, vec3 v )
	{
		return vec3(
			m[0].x * v.x + m[0].y * v.y + m[0].z * v.z,
			m[1].x * v.x + m[1].y * v.y + m[1].z * v.z,
			m[2].x * v.x + m[2].y * v.y + m[2].z * v.z );
	}
	void main()
	{
		highp vec4 localPos1 = jb.Joints[int(JointIndices.x)] * Position;
		highp vec4 localPos2 = jb.Joints[int(JointIndices.y)] * Position;
		highp vec4 localPos3 = jb.Joints[int(JointIndices.z)] * Position;
		highp vec4 localPos4 = jb.Joints[int(JointIndices.w)] * Position;
		highp vec4 localPos = localPos1 * JointWeights.x
							+ localPos2 * JointWeights.y
							+ localPos3 * JointWeights.z
							+ localPos4 * JointWeights.w;
		gl_Position = TransformVertex( localPos );

		vec3 eye = transposeMultiply( sm.ViewMatrix[VIEW_ID], -vec3( sm.ViewMatrix[VIEW_ID][3] ) );
		oEye = eye - vec3( ModelMatrix * Position );

		highp vec3 localNormal1 = multiply( jb.Joints[int(JointIndices.x)], Normal);
		highp vec3 localNormal2 = multiply( jb.Joints[int(JointIndices.y)], Normal);
		highp vec3 localNormal3 = multiply( jb.Joints[int(JointIndices.z)], Normal);
		highp vec3 localNormal4 = multiply( jb.Joints[int(JointIndices.w)], Normal);
		highp vec3 localNormal 	= localNormal1 * JointWeights.x
								+ localNormal2 * JointWeights.y
								+ localNormal3 * JointWeights.z
								+ localNormal4 * JointWeights.w;
		oNormal = multiply( ModelMatrix, localNormal );

		oTexCoord = TexCoord;
	}
)glsl";

static const char* HandPBRSkinned1FragmentShaderSrc = R"glsl(
	uniform sampler2D Texture0;
	uniform lowp vec3 SpecularLightDirection;
	uniform lowp vec3 SpecularLightColor;
	uniform lowp vec3 AmbientLightColor;
	uniform lowp vec3 GlowColor;
	uniform lowp vec4 ChannelControl;

	varying lowp vec3 oEye;
	varying lowp vec3 oNormal;
	varying lowp vec2 oTexCoord;

	lowp vec3 multiply( lowp mat3 m, lowp vec3 v )
	{
		return vec3(
			m[0].x * v.x + m[1].x * v.y + m[2].x * v.z,
			m[0].y * v.x + m[1].y * v.y + m[2].y * v.z,
			m[0].z * v.x + m[1].z * v.y + m[2].z * v.z );
	}

	lowp float pow5( float x )
	{
		float x2 = x * x;
		return x2 * x2 * x;
	}

	lowp float pow16( float x )
	{
		float x2 = x * x;
		float x4 = x2 * x2;
		float x8 = x4 * x4;
		float x16 = x8 * x8;
		return x16;
	}

	void main()
	{
		lowp vec3 eyeDir = normalize( oEye.xyz );
		lowp vec3 Normal = normalize( oNormal );

		lowp vec4 diffuse = texture2D( Texture0, oTexCoord ) + vec4( 0.03, 0.03, 0.03, 1 );
		lowp vec3 ambientValue = diffuse.xyz * AmbientLightColor;

		lowp float nDotL = max( dot( Normal, SpecularLightDirection ), 0.0 );
		lowp vec3 diffuseValue = diffuse.xyz * nDotL;

		lowp vec3 H = normalize( SpecularLightDirection + eyeDir );
		lowp float nDotH = max( dot( Normal, H ), 0.0 );
		lowp vec3 specularValue = pow16( nDotH ) * SpecularLightColor;

		lowp float vDotN = dot( eyeDir, Normal );
		lowp float fresnel = clamp( pow5( 1.0 - vDotN ), 0.0, 1.0 );
		lowp vec3 fresnelValue = GlowColor * fresnel;

		lowp vec3 controllerColor 	= diffuseValue * ChannelControl.x
									+ ambientValue * ChannelControl.y
									+ specularValue * ChannelControl.z
									+ fresnelValue * ChannelControl.w
									;
		gl_FragColor.xyz = controllerColor;
		gl_FragColor.w = clamp( fresnel, ChannelControl.x, 1.0 );
	}
)glsl";

const char* CapsuleVertexShaderSrc = R"glsl(
	attribute highp vec4 Position;
	attribute highp vec3 Normal;
	varying lowp vec3 oEye;
	varying lowp vec3 oNormal;

	vec3 multiply( mat4 m, vec3 v )
	{
		return vec3(
			m[0].x * v.x + m[1].x * v.y + m[2].x * v.z,
			m[0].y * v.x + m[1].y * v.y + m[2].y * v.z,
			m[0].z * v.x + m[1].z * v.y + m[2].z * v.z );
	}
	vec3 transposeMultiply( mat4 m, vec3 v )
	{
		return vec3(
			m[0].x * v.x + m[0].y * v.y + m[0].z * v.z,
			m[1].x * v.x + m[1].y * v.y + m[1].z * v.z,
			m[2].x * v.x + m[2].y * v.y + m[2].z * v.z );
	}

	void main()
	{
		gl_Position = TransformVertex( Position );

		vec3 eye = transposeMultiply( sm.ViewMatrix[VIEW_ID], -vec3( sm.ViewMatrix[VIEW_ID][3] ) );
		oEye = eye - vec3( ModelMatrix * Position );
		oNormal = multiply( ModelMatrix, Normal );
	}
)glsl";

static const char* CapsuleFragmentShaderSrc = R"glsl(
	uniform lowp vec4 ChannelControl;
	uniform lowp vec3 SpecularLightDirection;
	uniform lowp vec3 SpecularLightColor;
	uniform lowp vec3 AmbientLightColor;

	varying lowp vec3 oEye;
	varying lowp vec3 oNormal;

	lowp float pow16( float x )
	{
		float x2 = x * x;
		float x4 = x2 * x2;
		float x8 = x4 * x4;
		float x16 = x8 * x8;
		return x16;
	}

	void main()
	{
		lowp vec3 eyeDir = normalize( oEye.xyz );
		lowp vec3 Normal = normalize( oNormal );

		lowp vec4 diffuse = vec4( 0.8, 0.8, 0.8, 1 );
		lowp vec3 ambientValue = diffuse.xyz * AmbientLightColor;

		lowp float nDotL = max( dot( Normal, SpecularLightDirection ), 0.0 );
		lowp vec3 diffuseValue = diffuse.xyz * nDotL;

		lowp vec3 H = normalize( SpecularLightDirection + eyeDir );
		lowp float nDotH = max( dot( Normal, H ), 0.0 );
		lowp vec3 specularValue = pow16( nDotH ) * SpecularLightColor;

		lowp vec3 controllerColor 	= diffuseValue * ChannelControl.x
									+ ambientValue * ChannelControl.y
									+ specularValue * ChannelControl.z
									;
		gl_FragColor.xyz = controllerColor;
		gl_FragColor.w = ChannelControl.w;
	}
)glsl";

const char* AxisVertexShaderSrc = R"glsl(
	uniform JointMatrices
	{
		highp mat4 Joints[64];
	} jb;

	attribute highp vec4 Position;
	attribute lowp vec4 VertexColor;
	varying lowp vec4 oColor;

	void main()
	{
		highp vec4 localPos = jb.Joints[ gl_InstanceID ] * Position;
		gl_Position = TransformVertex( localPos );
		oColor = VertexColor;
	}
)glsl";

static const char* AxisFragmentShaderSrc = R"glsl(
	varying lowp vec4 oColor;
	void main()
	{
		gl_FragColor = oColor;
	}
)glsl";

ovrVrHands* theApp = nullptr;

VRMenuObject* HandScaleDisplayL = nullptr;
VRMenuObject* HandScaleDisplayR = nullptr;
VRMenuObject* SystemGestureStateL = nullptr;
VRMenuObject* SystemGestureStateR = nullptr;
VRMenuObject* FPSLabel = nullptr;

//==============================
// ovrVrHands::ovrVrHands
ovrVrHands::ovrVrHands(
    const int32_t mainThreadTid,
    const int32_t renderThreadTid,
    const int cpuLevel,
    const int gpuLevel)
    : ovrAppl(mainThreadTid, renderThreadTid, cpuLevel, gpuLevel, true /* useMultiView */),
      RenderState(RENDER_STATE_LOADING),
      FileSys(nullptr),
      DebugLines(nullptr),
      SoundEffectPlayer(nullptr),
      GuiSys(nullptr),
      Locale(nullptr),
      SceneModel(nullptr),
      SpriteAtlas(nullptr),
      ParticleSystem(nullptr),
      BeamAtlas(nullptr),
      BeamRenderer(nullptr) {
    theApp = this;
}

//==============================
// ovrVrHands::~ovrVrHands
ovrVrHands::~ovrVrHands() {
    delete SoundEffectPlayer;
    SoundEffectPlayer = nullptr;
    delete BeamRenderer;
    BeamRenderer = nullptr;
    delete ParticleSystem;
    ParticleSystem = nullptr;
    delete SpriteAtlas;
    SpriteAtlas = nullptr;

    OvrGuiSys::Destroy(GuiSys);
    if (SceneModel != nullptr) {
        delete SceneModel;
    }
}

//==============================
// ovrVrHands::AppInit
bool ovrVrHands::AppInit(const OVRFW::ovrAppContext* context) {
    const ovrJava& jj = *(reinterpret_cast<const ovrJava*>(context->ContextForVrApi()));
    const xrJava ctx = JavaContextConvert(jj);
    FileSys = OVRFW::ovrFileSys::Create(ctx);
    if (nullptr == FileSys) {
        ALOGE("Couldn't create FileSys");
        return false;
    }

    Locale = ovrLocale::Create(*ctx.Env, ctx.ActivityObject, "default");
    if (nullptr == Locale) {
        ALOGE("Couldn't create Locale");
        return false;
    }

    DebugLines = OvrDebugLines::Create();
    if (nullptr == DebugLines) {
        ALOGE("Couldn't create DebugLines");
        return false;
    }
    DebugLines->Init();

    SoundEffectPlayer = new OvrGuiSys::ovrDummySoundEffectPlayer();
    if (nullptr == SoundEffectPlayer) {
        ALOGE("Couldn't create SoundEffectPlayer");
        return false;
    }

    GuiSys = OvrGuiSys::Create(&ctx);
    if (nullptr == GuiSys) {
        ALOGE("Couldn't create GUI");
        return false;
    }

    std::string fontName;
    GetLocale().GetLocalizedString("@string/font_name", "efigs.fnt", fontName);

    GuiSys->Init(FileSys, *SoundEffectPlayer, fontName.c_str(), DebugLines);

    //------------------------------------------------------------------------------------------

    auto createMenu = [=](const std::string& s,
                          const Vector3f& position,
                          const Vector2f& size = {100.0f, 100.0f}) -> VRMenuObject* {
        std::string menuName = "target_";
        menuName += s;
        VRMenu* m = SimpleTargetMenu::Create(*GuiSys, *Locale, menuName, s);
        if (m != nullptr) {
            GuiSys->AddMenu(m);
            GuiSys->OpenMenu(m->GetName());

            Posef pose = m->GetMenuPose();
            pose.Translation = position;
            m->SetMenuPose(pose);

            OvrVRMenuMgr& menuMgr = GuiSys->GetVRMenuMgr();
            VRMenuObject* mo = menuMgr.ToObject(m->GetRootHandle());
            if (mo != nullptr) {
                mo = menuMgr.ToObject(mo->GetChildHandleForIndex(0));
                mo->SetSurfaceDims(0, size);
                mo->RegenerateSurfaceGeometry(0, false);
            }
            return mo;
        }
        return nullptr;
    };

    auto createKeyboardRow = [=](const std::vector<std::string>&& row,
                                 const Vector3f& rowStart,
                                 const Vector3f& rowDelta) -> void {
        Vector3f rowPosition = rowStart;
        for (auto& letter : row) {
            VRMenuObject* mo = createMenu(letter, rowPosition);
            rowPosition += rowDelta;

            /// Add to the text
            ButtonHandlers[mo] = [=]() {
                std::string text = TypeText->GetText();
                text += mo->GetText();
                TypeText->SetText(text.c_str());
            };
        }
    };

    /// Create simple keyboard out of menus
    createKeyboardRow(
        {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
        {-1.0f, 1.0f, -2.0f},
        {0.25f, 0.0f, 0.0f});
    createKeyboardRow(
        {"A", "S", "D", "F", "G", "H", "J", "K", "L"},
        {-0.875f, 0.75f, -2.0f},
        {0.25f, 0.0f, 0.0f});
    createKeyboardRow(
        {"Z", "X", "C", "V", "B", "N", "M", "."}, {-0.75f, 0.5f, -2.0f}, {0.25f, 0.0f, 0.0f});
    /// Add Backspace
    VRMenuObject* backSpace = createMenu("<-", Vector3f(-1.0f + 10 * 0.25f, 1.0f, -2.0f));
    ButtonHandlers[backSpace] = [=]() {
        std::string text = TypeText->GetText();
        text = text.substr(0, text.length() - 1);
        TypeText->SetText(text.c_str());
    };
    /// Add SpaceBar
    VRMenuObject* spaceBar = createMenu("SPACE", {0.1f, 0.25f, -2.0f}, {1000.0f, 100.0f});
    ButtonHandlers[spaceBar] = [=]() {
        std::string text = TypeText->GetText();
        text += " ";
        TypeText->SetText(text.c_str());
    };
    /// Add Clear
    VRMenuObject* clearText =
        createMenu("Clear", Vector3f(-1.0f + 10 * 0.25f, 0.75f, -2.0f), {200.0f, 100.0f});
    ButtonHandlers[clearText] = [=]() { TypeText->SetText(""); };

    /// Add Typing line
    TypeText = createMenu("Text", Vector3f(0.1f, 1.25f, -2.0f), {1200.0f, 100.0f});
    TypeText->SetText("");

    /// Add a toggle to show capsules
    VRMenuObject* capsuleToggle =
        createMenu("Show Capsules", Vector3f(-1.0f, 1.75f, -1.85f), {200.0f, 100.0f});
    ButtonHandlers[capsuleToggle] = [=]() {
        SampleConfiguration.ShowCapsules = !SampleConfiguration.ShowCapsules;
        if (SampleConfiguration.ShowCapsules) {
            capsuleToggle->SetText("Hide Capsules");
        } else {
            capsuleToggle->SetText("Show Capsules");
        }
    };

    /// Shader control
    ButtonHandlers[createMenu("Default", Vector3f(-0.5f, 1.75f, -1.85f), {200.0f, 100.0f})] =
        [=]() { ChannelControl = Vector4f(0.25f, 0.2f, 0.2f, 1.0f); };
    ButtonHandlers[createMenu("Solid", Vector3f(0.0f, 1.75f, -1.85f), {200.0f, 100.0f})] = [=]() {
        ChannelControl = Vector4f(1.0f, 0.2f, 0.6f, 0.0f);
    };
    ButtonHandlers[createMenu("Outline", Vector3f(0.5f, 1.75f, -1.85f), {200.0f, 100.0f})] = [=]() {
        ChannelControl = Vector4f(0.0f, 0.0f, 0.0f, 1.0f);
    };

    VRMenuObject* axisToggle =
        createMenu("Show Axes", Vector3f(1.0f, 1.75f, -1.85f), {200.0f, 100.0f});
    ButtonHandlers[axisToggle] = [=]() {
        SampleConfiguration.ShowAxis = !SampleConfiguration.ShowAxis;
        if (SampleConfiguration.ShowAxis) {
            axisToggle->SetText("Hide Axes");
        } else {
            axisToggle->SetText("Show Axes");
        }
    };

    /// Add size menus
    HandScaleDisplayL = createMenu("size L 1.000", Vector3f(-1.0f, 1.5f, -2.0f), {200.0f, 100.0f});
    HandScaleDisplayR = createMenu("size R 1.000", Vector3f(-0.5f, 1.5f, -2.0f), {200.0f, 100.0f});

    FPSLabel = createMenu("0.00 fps", Vector3f(1.25f, 2.25f, -1.90f), {200.0f, 100.0f});

    /// Add indicators for system state gesture stages
    SystemGestureStateL = createMenu("SG State L", Vector3f(0.75f, 1.5f, -2.0f), {200.0f, 100.0f});
    SystemGestureStateR = createMenu("SG State R", Vector3f(1.25f, 1.5f, -2.0f), {200.0f, 100.0f});

    //------------------------------------------------------------------------------------------

    SpecularLightDirection = Vector3f(0.0f, 1.0f, 0.0f);
    SpecularLightColor = Vector3f(1.0f, 0.95f, 0.8f) * 0.25f;
    AmbientLightColor = Vector3f(1.0f, 1.0f, 1.0f) * 0.05f;
    ChannelControl = Vector4f(0.25f, 1.0f, 1.0f, 1.0f);

    static ovrProgramParm HandSkinnedUniformParms[] = {
        {"Texture0", ovrProgramParmType::TEXTURE_SAMPLED},
        {"SpecularLightDirection", ovrProgramParmType::FLOAT_VECTOR3},
        {"SpecularLightColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"AmbientLightColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"JointMatrices", ovrProgramParmType::BUFFER_UNIFORM},
        {"GlowColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"ChannelControl", ovrProgramParmType::FLOAT_VECTOR4},
    };

    ProgHandSkinned = GlProgram::Build(
        HandPBRSkinned1VertexShaderSrc,
        HandPBRSkinned1FragmentShaderSrc,
        HandSkinnedUniformParms,
        sizeof(HandSkinnedUniformParms) / sizeof(ovrProgramParm));

    static ovrProgramParm CapsuleUniformParms[] = {
        {"ChannelControl", ovrProgramParmType::FLOAT_VECTOR4},
        {"SpecularLightDirection", ovrProgramParmType::FLOAT_VECTOR3},
        {"SpecularLightColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"AmbientLightColor", ovrProgramParmType::FLOAT_VECTOR3},
    };

    ProgHandCapsules = GlProgram::Build(
        CapsuleVertexShaderSrc,
        CapsuleFragmentShaderSrc,
        CapsuleUniformParms,
        sizeof(CapsuleUniformParms) / sizeof(ovrProgramParm));

    static ovrProgramParm HandAxisUniformParms[] = {
        {"JointMatrices", ovrProgramParmType::BUFFER_UNIFORM},
    };

    ProgHandAxis = GlProgram::Build(
        AxisVertexShaderSrc,
        AxisFragmentShaderSrc,
        HandAxisUniformParms,
        sizeof(HandAxisUniformParms) / sizeof(ovrProgramParm));

    //------------------------------------------------------------------------------------------

    {
        MaterialParms materialParms;
        materialParms.UseSrgbTextureFormats = false;
        const char* sceneUri = "apk:///assets/box.ovrscene";
        SceneModel = LoadModelFile(
            GuiSys->GetFileSys(), sceneUri, Scene.GetDefaultGLPrograms(), materialParms);

        if (SceneModel != nullptr) {
            Scene.SetWorldModel(*SceneModel);
            Vector3f modelOffset;
            modelOffset.x = 0.5f;
            modelOffset.y = 0.0f;
            modelOffset.z = -2.25f;
            Scene.GetWorldModel()->State.SetMatrix(
                Matrix4f::Scaling(2.5f, 2.5f, 2.5f) * Matrix4f::Translation(modelOffset));
        }
    }

    {
        // Hand texure
        std::vector<uint8_t> buffer;
#if 0		
		// Use the reference UV mapping file
		const char * textureUri = "apk:///assets/handsTextureMapping.png";
#else
        // Use a sample UV mapped texture with individual finger marks
        const char* textureUri = "apk:///assets/handsTexture.png";
#endif
        if (GuiSys->GetFileSys().ReadFile(textureUri, buffer)) {
            ALOG(
                "### Loaded texture file: %s size %u data %p",
                textureUri,
                (uint32_t)buffer.size(),
                buffer.data());
            int width = 0;
            int height = 0;
            HandsTexture =
                LoadTextureFromBuffer(textureUri, buffer, TextureFlags_t(), width, height);
            ALOG(
                "### LoadTextureFromBuffer file: %s width %d height %d", textureUri, width, height);
        }
    }

    //------------------------------------------------------------------------------------------

    SpriteAtlas = new ovrTextureAtlas();
    SpriteAtlas->Init(GuiSys->GetFileSys(), "apk:///assets/particles2.ktx");
    SpriteAtlas->BuildSpritesFromGrid(4, 2, 8);

    ParticleSystem = new ovrParticleSystem();
    ParticleSystem->Init(2048, SpriteAtlas, ovrParticleSystem::GetDefaultGpuState(), false);

    BeamAtlas = new ovrTextureAtlas();
    BeamAtlas->Init(GuiSys->GetFileSys(), "apk:///assets/beams.ktx");
    BeamAtlas->BuildSpritesFromGrid(2, 1, 2);

    BeamRenderer = new ovrBeamRenderer();
    BeamRenderer->Init(256, true);

    //------------------------------------------------------------------------------------------

    SurfaceRender.Init();

    return true;
}

//==============================
// ovrVrHands::ResetLaserPointer
void ovrVrHands::ResetLaserPointer(ovrInputDeviceHandBase& trDevice) {
    ovrBeamRenderer::handle_t& LaserPointerBeamHandle = trDevice.GetLaserPointerBeamHandle();
    ovrParticleSystem::handle_t& LaserPointerParticleHandle =
        trDevice.GetLaserPointerParticleHandle();

    if (LaserPointerBeamHandle.IsValid()) {
        BeamRenderer->RemoveBeam(LaserPointerBeamHandle);
        LaserPointerBeamHandle.Release();
    }
    if (LaserPointerParticleHandle.IsValid()) {
        ParticleSystem->RemoveParticle(LaserPointerParticleHandle);
        LaserPointerParticleHandle.Release();
    }
}

//==============================
// ovrVrHands::AppShutdown
void ovrVrHands::AppShutdown(const OVRFW::ovrAppContext* context) {
    ALOG("AppShutdown");
    for (int i = InputDevices.size() - 1; i >= 0; --i) {
        OnDeviceDisconnected(InputDevices[i]->GetDeviceID());
    }

    SurfaceRender.Shutdown();
}

void ovrVrHands::RenderBones(
    const ovrApplFrameIn& frame,
    const OVR::Matrix4f& worldMatrix,
    const std::vector<ovrJoint>& joints,
    jointHandles_t& handles,
    const OVR::Vector4f& boneColor,
    const float jointRadius,
    const float boneWidth) {
    const uint16_t particleAtlasIndex = 0;
    const uint16_t beamAtlasIndex = 0;
    ovrParticleSystem* ps = ParticleSystem;
    ovrBeamRenderer* br = BeamRenderer;
    ovrTextureAtlas& beamAtlas = *BeamAtlas;

    for (int i = 0; i < static_cast<int>(joints.size()); ++i) {
        const ovrJoint& joint = joints[i];
        OVR::Vector3f jwPosition = worldMatrix.Transform(joint.Pose.Translation);

        OVR::Vector4f jointColor = joint.Color;
        jointColor.w = boneColor.w;
        if (!handles[i].first.IsValid()) {
            handles[i].first = ps->AddParticle(
                frame,
                jwPosition,
                0.0f,
                Vector3f(0.0f),
                Vector3f(0.0f),
                jointColor,
                ovrEaseFunc::NONE,
                0.0f,
                jointRadius,
                FLT_MAX,
                particleAtlasIndex);
        } else {
            ps->UpdateParticle(
                frame,
                handles[i].first,
                jwPosition,
                0.0f,
                Vector3f(0.0f),
                Vector3f(0.0f),
                jointColor,
                ovrEaseFunc::NONE,
                0.0f,
                jointRadius,
                FLT_MAX,
                particleAtlasIndex);
        }

        if (i > 0) {
            const ovrJoint& parentJoint = joints[joint.ParentIndex];
            OVR::Vector3f pwPosition = worldMatrix.Transform(parentJoint.Pose.Translation);

            if (!handles[i].second.IsValid()) {
                handles[i].second = br->AddBeam(
                    frame,
                    beamAtlas,
                    beamAtlasIndex,
                    boneWidth,
                    pwPosition,
                    jwPosition,
                    boneColor,
                    ovrBeamRenderer::LIFETIME_INFINITE);
            } else {
                br->UpdateBeam(
                    frame,
                    handles[i].second,
                    beamAtlas,
                    beamAtlasIndex,
                    boneWidth,
                    pwPosition,
                    jwPosition,
                    boneColor);
            }
        }
    }
}

void ovrVrHands::ResetBones(jointHandles_t& handles) {
    ovrParticleSystem* ps = ParticleSystem;
    ovrBeamRenderer* br = BeamRenderer;

    for (int i = 0; i < static_cast<int>(handles.size()); ++i) {
        if (handles[i].first.IsValid()) {
            ps->RemoveParticle(handles[i].first);
            handles[i].first.Release();
        }
        if (handles[i].second.IsValid()) {
            br->RemoveBeam(handles[i].second);
            handles[i].second.Release();
        }
    }
}

//==============================
// ovrVrHands::AppFrame
OVRFW::ovrApplFrameOut ovrVrHands::AppFrame(const OVRFW::ovrApplFrameIn& vrFrame) {
    const float frameTime = vrFrame.DeltaSeconds;
    const float frameRate = 1.0f / frameTime;
    if (FPSLabel) {
        std::ostringstream ss;
        ss << frameRate << " fps";
        FPSLabel->SetText(ss.str().c_str());
    }

    for (int i = (int)InputDevices.size() - 1; i >= 0; --i) {
        ovrInputDeviceBase* device = InputDevices[i];
        if (device->GetType() == ovrControllerType_Hand) {
            ovrInputDeviceTrackedHand& handDevice =
                *static_cast<ovrInputDeviceTrackedHand*>(device);
            {
                VRMenuObject* DebugSystemDetectorLabel =
                    handDevice.IsLeftHand() ? SystemGestureStateL : SystemGestureStateR;
                if (DebugSystemDetectorLabel) {
                    if (handDevice.IsInSystemGesture()) {
                        DebugSystemDetectorLabel->SetText("System Gesture");
                    } else {
                        DebugSystemDetectorLabel->SetText("Normal");
                    }
                }
            }
        }
    }

    return OVRFW::ovrApplFrameOut();
}

void ovrVrHands::RenderRunningFrame(
    const OVRFW::ovrApplFrameIn& in,
    OVRFW::ovrRendererOutput& out) {
    //------------------------------------------------------------------------------------------

    EnumerateInputDevices();

    const ovrJava* java = reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi());
    int ActiveInputDeviceID;
    vrapi_GetPropertyInt(java, VRAPI_ACTIVE_INPUT_DEVICE_ID, &ActiveInputDeviceID);
    uint32_t uiActiveInputDeviceID = (uint32_t)ActiveInputDeviceID;

    // for each device, query its current tracking state and input state
    // it's possible for a device to be removed during this loop, so we go through it backwards
    for (int i = (int)InputDevices.size() - 1; i >= 0; --i) {
        ovrInputDeviceBase* device = InputDevices[i];
        if (device == nullptr) {
            assert(false); // this should never happen!
            continue;
        }
        ovrDeviceID deviceID = device->GetDeviceID();
        if (deviceID == ovrDeviceIdType_Invalid) {
            assert(deviceID != ovrDeviceIdType_Invalid);
            continue;
        }
        if (device->GetType() == ovrControllerType_Hand) {
            ovrInputDeviceTrackedHand& trDevice = *static_cast<ovrInputDeviceTrackedHand*>(device);

            // clean last hit object
            VRMenuObject* lastHitObject =
                GuiSys->GetVRMenuMgr().ToObject(trDevice.GetLastHitHandle());
            if (lastHitObject != nullptr) {
                lastHitObject->SetSurfaceColor(0, MENU_DEFAULT_COLOR);
            }

            if (false ==
                trDevice.Update(GetSessionObject(), in.PredictedDisplayTime, in.DeltaSeconds)) {
                OnDeviceDisconnected(deviceID);
                continue;
            }

            trDevice.SetIsActiveInputDevice(trDevice.GetDeviceID() == uiActiveInputDeviceID);
        }
    }

    //------------------------------------------------------------------------------------------

    Scene.SetFreeMove(true);
    Scene.Frame(in);
    Scene.GetFrameMatrices(SuggestedEyeFovDegreesX, SuggestedEyeFovDegreesY, out.FrameMatrices);
    Scene.GenerateFrameSurfaceList(out.FrameMatrices, out.Surfaces);

    Matrix4f traceMat(out.FrameMatrices.CenterView.Inverted());
    for (int i = (int)InputDevices.size() - 1; i >= 0; --i) {
        ovrInputDeviceBase* device = InputDevices[i];
        if (device == nullptr) {
            assert(false); // this should never happen!
            continue;
        }
        ovrDeviceID deviceID = device->GetDeviceID();
        if (deviceID == ovrDeviceIdType_Invalid) {
            assert(deviceID != ovrDeviceIdType_Invalid);
            continue;
        }
        if (device->GetType() == ovrControllerType_Hand) {
            ovrInputDeviceTrackedHand& trDevice = *static_cast<ovrInputDeviceTrackedHand*>(device);
            bool isHighConfidence = (trDevice.GetHandPoseConfidence() == ovrConfidence_HIGH);
            bool updateLaser = trDevice.IsPinching();
            bool renderLaser = isHighConfidence && trDevice.IsPointerValid();
            bool renderSkeleton = isHighConfidence && device->GetType() == ovrControllerType_Hand;
            /// Render skeletopn
            if (renderSkeleton) {
                const Posef& handPose = trDevice.GetHandPose();
                const Matrix4f matDeviceModel = trDevice.GetModelMatrix(handPose);
                RenderBones(
                    in,
                    matDeviceModel,
                    trDevice.GetHandModel().GetTransformedJoints(),
                    trDevice.GetFingerJointHandles());
            } else {
                ResetBones(trDevice.GetFingerJointHandles());
            }

            if (renderLaser) {
                Vector3f pointerStart(0.0f);
                Vector3f pointerEnd(0.0f);
                bool LaserHit = false;
                pointerStart = trDevice.GetRayOrigin();
                pointerEnd = trDevice.GetRayEnd();
                Vector3f const pointerDir = (pointerEnd - pointerStart).Normalized();
                Vector3f targetEnd = pointerStart + pointerDir * (updateLaser ? 10.0f : 0.075f);

                HitTestResult hit = GuiSys->TestRayIntersection(pointerStart, pointerDir);
                LaserHit = hit.HitHandle.IsValid() &&
                    (trDevice.GetHandPoseConfidence() == ovrConfidence_HIGH);
                trDevice.SetLastHitHandle(hit.HitHandle);
                if (LaserHit) {
                    targetEnd = pointerStart + hit.RayDir * hit.t - pointerDir * 0.025f;
                    VRMenuObject* hitObject = GuiSys->GetVRMenuMgr().ToObject(hit.HitHandle);
                    if (hitObject != nullptr) {
                        if (updateLaser) {
                            hitObject->SetSurfaceColor(0, MENU_HIGHLIGHT_COLOR);
                            pointerEnd = targetEnd;
                        }

                        // check hit-testing
                        if (trDevice.Clicked()) {
                            auto it = ButtonHandlers.find(hitObject);
                            if (it != ButtonHandlers.end()) {
                                // Call handler
                                it->second();
                            }
                        }
                    }
                }

                Vector4f ConfidenceLaserColor;
                float confidenceAlpha = isHighConfidence ? 1.0f : 0.1f;
                ConfidenceLaserColor.x = 1.0f - confidenceAlpha;
                ConfidenceLaserColor.y = confidenceAlpha;
                if (trDevice.GetIsActiveInputDevice()) {
                    ConfidenceLaserColor.z = 1.0f;
                } else {
                    ConfidenceLaserColor.z = 0.0f;
                }
                ConfidenceLaserColor.w = confidenceAlpha;

                ovrBeamRenderer::handle_t& LaserPointerBeamHandle =
                    trDevice.GetLaserPointerBeamHandle();
                ovrParticleSystem::handle_t& LaserPointerParticleHandle =
                    trDevice.GetLaserPointerParticleHandle();
                if (!LaserPointerBeamHandle.IsValid()) {
                    LaserPointerBeamHandle = BeamRenderer->AddBeam(
                        in,
                        *BeamAtlas,
                        0,
                        0.032f,
                        pointerStart,
                        pointerEnd,
                        ConfidenceLaserColor,
                        ovrBeamRenderer::LIFETIME_INFINITE);
                } else {
                    BeamRenderer->UpdateBeam(
                        in,
                        LaserPointerBeamHandle,
                        *BeamAtlas,
                        0,
                        0.032f,
                        pointerStart,
                        pointerEnd,
                        ConfidenceLaserColor);
                }

                if (!LaserPointerParticleHandle.IsValid()) {
                    if (LaserHit) {
                        LaserPointerParticleHandle = ParticleSystem->AddParticle(
                            in,
                            targetEnd,
                            0.0f,
                            Vector3f(0.0f),
                            Vector3f(0.0f),
                            ConfidenceLaserColor,
                            ovrEaseFunc::NONE,
                            0.0f,
                            0.1f,
                            0.1f,
                            0);
                    }
                } else {
                    if (LaserHit) {
                        ParticleSystem->UpdateParticle(
                            in,
                            LaserPointerParticleHandle,
                            targetEnd,
                            0.0f,
                            Vector3f(0.0f),
                            Vector3f(0.0f),
                            ConfidenceLaserColor,
                            ovrEaseFunc::NONE,
                            0.0f,
                            0.1f,
                            0.1f,
                            0);
                    } else {
                        ParticleSystem->RemoveParticle(LaserPointerParticleHandle);
                        LaserPointerParticleHandle.Release();
                    }
                }
            } else {
                ResetLaserPointer(trDevice);
            }
        }
    }
    //------------------------------------------------------------------------------------------

    GuiSys->Frame(in, out.FrameMatrices.CenterView, traceMat);
    BeamRenderer->Frame(in, out.FrameMatrices.CenterView, *BeamAtlas);
    ParticleSystem->Frame(in, SpriteAtlas, out.FrameMatrices.CenterView);
    GuiSys->AppendSurfaceList(out.FrameMatrices.CenterView, &out.Surfaces);

    // render bones first
    const Matrix4f projectionMatrix;
    ParticleSystem->RenderEyeView(out.FrameMatrices.CenterView, projectionMatrix, out.Surfaces);
    BeamRenderer->RenderEyeView(out.FrameMatrices.CenterView, projectionMatrix, out.Surfaces);

    // add the controller model surfaces to the list of surfaces to render
    for (int i = 0; i < (int)InputDevices.size(); ++i) {
        ovrInputDeviceBase* device = InputDevices[i];
        if (device == nullptr) {
            assert(false); // this should never happen!
            continue;
        }
        if (device->GetType() != ovrControllerType_Hand) {
            continue;
        }
        ovrInputDeviceTrackedHand& trDevice = *static_cast<ovrInputDeviceTrackedHand*>(device);

        bool isHighConfidence = (trDevice.GetHandPoseConfidence() == ovrConfidence_HIGH);
        if (isHighConfidence) {
            trDevice.Render(out.Surfaces);
        }
    }
}

void ovrVrHands::AppRenderEye(
    const OVRFW::ovrApplFrameIn& in,
    OVRFW::ovrRendererOutput& out,
    int eye) {
    // Render the surfaces returned by Frame.
    SurfaceRender.RenderSurfaceList(
        out.Surfaces,
        out.FrameMatrices.EyeView[0], // always use 0 as it assumes an array
        out.FrameMatrices.EyeProjection[0], // always use 0 as it assumes an array
        eye);
}

void ovrVrHands::AppRenderFrame(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out) {
    switch (RenderState) {
        case RENDER_STATE_LOADING: {
            DefaultRenderFrame_Loading(in, out);
        } break;
        case RENDER_STATE_RUNNING: {
            RenderRunningFrame(in, out);
            DefaultRenderFrame_Running(in, out);
        } break;
        case RENDER_STATE_ENDING: {
            DefaultRenderFrame_Ending(in, out);
        } break;
    }
}

//---------------------------------------------------------------------------------------------------
// Input device management
//---------------------------------------------------------------------------------------------------

//==============================
// ovrVrHands::FindInputDevice
int ovrVrHands::FindInputDevice(const ovrDeviceID deviceID) const {
    for (int i = 0; i < (int)InputDevices.size(); ++i) {
        if (InputDevices[i]->GetDeviceID() == deviceID) {
            return i;
        }
    }
    return -1;
}

//==============================
// ovrVrHands::RemoveDevice
void ovrVrHands::RemoveDevice(const ovrDeviceID deviceID) {
    int index = FindInputDevice(deviceID);
    if (index < 0) {
        return;
    }
    ovrInputDeviceBase* device = InputDevices[index];
    delete device;
    InputDevices[index] = InputDevices.back();
    InputDevices[InputDevices.size() - 1] = nullptr;
    InputDevices.pop_back();
}

//==============================
// ovrVrHands::IsDeviceTracked
bool ovrVrHands::IsDeviceTracked(const ovrDeviceID deviceID) const {
    return FindInputDevice(deviceID) >= 0;
}

//==============================
// ovrVrHands::EnumerateInputDevices
void ovrVrHands::EnumerateInputDevices() {
    for (uint32_t deviceIndex = 0;; deviceIndex++) {
        ovrInputCapabilityHeader curCaps;

        if (vrapi_EnumerateInputDevices(GetSessionObject(), deviceIndex, &curCaps) < 0) {
            // ALOG( "Input - No more devices!" );
            break; // no more devices
        }

        if (!IsDeviceTracked(curCaps.DeviceID)) {
            ALOG("Input -      tracked");
            OnDeviceConnected(curCaps);
        }
    }
}

//==============================
// ovrVrHands::OnDeviceConnected
void ovrVrHands::OnDeviceConnected(const ovrInputCapabilityHeader& capsHeader) {
    ovrInputDeviceBase* device = nullptr;
    ovrResult result = ovrError_NotInitialized;
    switch (capsHeader.Type) {
        case ovrControllerType_Hand: {
            ALOG("VrHands - Hand connected, ID = %u", capsHeader.DeviceID);

            ovrInputHandCapabilities handCapabilities;
            handCapabilities.Header = capsHeader;
            ALOG("VrHands - calling get device caps");
            result = vrapi_GetInputDeviceCapabilities(GetSessionObject(), &handCapabilities.Header);
            ALOG("VrHands - post calling get device caps %d", result);
            if (result == ovrSuccess) {
                ovrInputDeviceTrackedHand* handDevice =
                    ovrInputDeviceTrackedHand::Create(*this, handCapabilities);
                if (handDevice != nullptr) {
                    device = handDevice;
                    ovrHandedness handedness =
                        handDevice->IsLeftHand() ? VRAPI_HAND_LEFT : VRAPI_HAND_RIGHT;

                    ovrHandSkeleton skeleton;
                    skeleton.Header.Version = ovrHandVersion_1;
                    if (vrapi_GetHandSkeleton(GetSessionObject(), handedness, &skeleton.Header) !=
                        ovrSuccess) {
                        ALOG("VrHands - failed to get hand skeleton");
                    } else {
                        ALOG("VrHands - got a skeleton ... NumBones:%u", skeleton.NumBones);
                        /// ECHO bind poses
                        for (uint32_t i = 0; i < skeleton.NumBones; ++i) {
                            Posef pose = skeleton.BonePoses[i];
                            ALOG(
                                "Posef{ Quatf{ %.6f, %.6f, %.6f, %.6f }, Vector3f{ %.6f, %.6f, %.6f } }, // bone=%u parent=%d",
                                pose.Rotation.x,
                                pose.Rotation.y,
                                pose.Rotation.z,
                                pose.Rotation.w,
                                pose.Translation.x,
                                pose.Translation.y,
                                pose.Translation.z,
                                i,
                                (int)skeleton.BoneParentIndices[i]);
                        }
                    }

                    {
                        std::unique_ptr<ovrHandMesh> mesh(new ovrHandMesh());
                        if (mesh) {
                            mesh->Header.Version = ovrHandVersion_1;
                            if (vrapi_GetHandMesh(
                                    GetSessionObject(), handedness, &(mesh.get()->Header)) !=
                                ovrSuccess) {
                                ALOG("VrHands - failed to get hand mesh");
                            } else {
                                handDevice->InitFromSkeletonAndMesh(*this, &skeleton, mesh.get());
                            }
                        }
                    }
                }
            }
            break;
        }
        default:
            ALOG("Unknown device connected!");
            return;
    }

    if (result != ovrSuccess) {
        ALOG("VrHands - vrapi_GetInputDeviceCapabilities: Error %i", result);
    }
    if (device != nullptr) {
        ALOG("VrHands - Added device '%s', id = %u", device->GetName(), capsHeader.DeviceID);
        InputDevices.push_back(device);
    } else {
        ALOG("VrHands - Device creation failed for id = %u", capsHeader.DeviceID);
    }
}

//==============================
// ovrVrHands::OnDeviceDisconnected
void ovrVrHands::OnDeviceDisconnected(const ovrDeviceID deviceID) {
    ALOG("VrHands - device disconnected, ID = %i", deviceID);
    int deviceIndex = FindInputDevice(deviceID);
    if (deviceIndex >= 0) {
        ovrInputDeviceBase* device = InputDevices[deviceIndex];
        if (device != nullptr && device->GetType() == ovrControllerType_Hand) {
            ovrInputDeviceTrackedHand& trDevice = *static_cast<ovrInputDeviceTrackedHand*>(device);
            ResetBones(trDevice.GetFingerJointHandles());
            ResetLaserPointer(trDevice);
        }
    }
    RemoveDevice(deviceID);
}

void ovrVrHands::AppResumed(const OVRFW::ovrAppContext* /* context */) {
    ALOGV("ovrVrHands::AppResumed");
    RenderState = RENDER_STATE_RUNNING;
}

void ovrVrHands::AppPaused(const OVRFW::ovrAppContext* /* context */) {
    ALOGV("ovrVrHands::AppPaused");
}

void ovrInputDeviceTrackedHand::UpdateSkeleton(const OVR::Posef& handPose) {
    const std::vector<OVR::Posef>& poses = HandModel.GetSkeleton().GetWorldSpacePoses();
    for (size_t j = 0; j < poses.size(); ++j) {
        /// Compute transform
        TransformMatrices[j] = Matrix4f(poses[j]);
        Matrix4f m = TransformMatrices[j] * BindMatrices[j];
        SkinMatrices[j] = m.Transposed();
    }

    /// Update the shader uniform parameters
    SkinUniformBuffer.Update(SkinMatrices.size() * sizeof(Matrix4f), SkinMatrices.data());

    Matrix4f matDeviceModel = GetModelMatrix(handPose);

    /// Ensure the hand surface is using this uniform parameters
    HandSurface.modelMatrix = matDeviceModel;

    for (uint32_t i = 0; i < CapsuleSurfaces.size(); ++i) {
        const ovrBoneCapsule& capsule = BoneCapsules[i];
        CapsuleSurfaces[i].modelMatrix = matDeviceModel /// all capsules are relateive to the wrist
            * TransformMatrices[capsule.BoneIndex] /// get specific bone space
            * CapsuleTransforms[i] /// geometry transform
            ;
    }

    /// Instance the Axes ...
    AxisSurface.modelMatrix = matDeviceModel;
    for (size_t j = 0; j < poses.size(); ++j) {
        /// Compute transform
        TransformMatrices[j] = TransformMatrices[j].Transposed();
    }
    InstancedBoneUniformBuffer.Update(
        TransformMatrices.size() * sizeof(Matrix4f), TransformMatrices.data());
}

//==============================
// ovrInputDeviceTrackedHand::Create
ovrInputDeviceTrackedHand* ovrInputDeviceTrackedHand::Create(
    OVRFW::ovrAppl& app,
    const ovrInputHandCapabilities& capsHeader) {
    ALOG("VrHands - ovrInputDeviceTrackedHand::Create");

    ovrInputStateHand handInputState;
    handInputState.Header.ControllerType = capsHeader.Header.Type;

    ovrResult result = vrapi_GetCurrentInputState(
        app.GetSessionObject(), capsHeader.Header.DeviceID, &handInputState.Header);
    if (result == ovrSuccess) {
        ovrHandedness controllerHand = (capsHeader.HandCapabilities & ovrHandCaps_LeftHand) != 0
            ? VRAPI_HAND_LEFT
            : VRAPI_HAND_RIGHT;
        ovrInputDeviceTrackedHand* device =
            new ovrInputDeviceTrackedHand(capsHeader, controllerHand);
        return device;
    } else {
        ALOG("VrHands - vrapi_GetCurrentInputState: Error %i", result);
    }

    return nullptr;
}

void ovrInputDeviceTrackedHand::InitFromSkeletonAndMesh(
    ovrVrHands& app,
    ovrHandSkeleton* skeleton,
    ovrHandMesh* mesh) {
    /// Base
    if (mesh == nullptr) {
        ALOGW("InitFromSkeletonAndMesh - mesh == nullptr");
        return;
    }
    if (skeleton == nullptr) {
        ALOGW("InitFromSkeletonAndMesh - skeleton == nullptr");
        return;
    }
    ALOG(
        "InitFromSkeletonAndMesh - mesh=%p NumVertices=%u NumIndices=%u",
        mesh,
        mesh->NumVertices,
        mesh->NumIndices);

    /// Ensure all identity
    TransformMatrices.resize(MAX_JOINTS, OVR::Matrix4f::Identity());
    BindMatrices.resize(MAX_JOINTS, OVR::Matrix4f::Identity());
    SkinMatrices.resize(MAX_JOINTS, OVR::Matrix4f::Identity());
    SkinUniformBuffer.Create(
        GLBUFFER_TYPE_UNIFORM, MAX_JOINTS * sizeof(Matrix4f), SkinMatrices.data());
    InstancedBoneUniformBuffer.Create(
        GLBUFFER_TYPE_UNIFORM, MAX_JOINTS * sizeof(Matrix4f), TransformMatrices.data());

    HandModel.Init(*skeleton);
    FingerJointHandles.resize(HandModel.GetSkeleton().GetJoints().size());

    /// Walk the transform hierarchy and store the wolrd space transforms in TransformMatrices
    const std::vector<OVR::Posef>& poses = HandModel.GetSkeleton().GetWorldSpacePoses();
    for (size_t j = 0; j < poses.size(); ++j) {
        TransformMatrices[j] = Matrix4f(poses[j]);
    }

    for (size_t j = 0; j < BindMatrices.size(); ++j) {
        BindMatrices[j] = TransformMatrices[j].Inverted();
    }

    /// Init skinned rendering
    /// Build geometry from mesh
    VertexAttribs attribs;
    std::vector<TriangleIndex> indices;

    attribs.position.resize(mesh->NumVertices);
    memcpy(
        attribs.position.data(),
        &mesh->VertexPositions[0],
        mesh->NumVertices * sizeof(ovrVector3f));
    attribs.normal.resize(mesh->NumVertices);
    memcpy(attribs.normal.data(), &mesh->VertexNormals[0], mesh->NumVertices * sizeof(ovrVector3f));
    attribs.uv0.resize(mesh->NumVertices);
    memcpy(attribs.uv0.data(), &mesh->VertexUV0[0], mesh->NumVertices * sizeof(ovrVector2f));
    attribs.jointIndices.resize(mesh->NumVertices);
    /// We can't do a straight copy heere since the sizes don't match
    for (std::uint32_t i = 0; i < mesh->NumVertices; ++i) {
        const ovrVector4s& blendIndices = mesh->BlendIndices[i];
        attribs.jointIndices[i].x = blendIndices.x;
        attribs.jointIndices[i].y = blendIndices.y;
        attribs.jointIndices[i].z = blendIndices.z;
        attribs.jointIndices[i].w = blendIndices.w;
    }
    attribs.jointWeights.resize(mesh->NumVertices);
    memcpy(
        attribs.jointWeights.data(),
        &mesh->BlendWeights[0],
        mesh->NumVertices * sizeof(ovrVector4f));

    static_assert(
        sizeof(ovrVertexIndex) == sizeof(TriangleIndex),
        "sizeof(ovrVertexIndex) == sizeof(TriangleIndex) don't match!");
    indices.resize(mesh->NumIndices);
    memcpy(indices.data(), mesh->Indices, mesh->NumIndices * sizeof(ovrVertexIndex));

    ALOG(
        "InitFromSkeletonAndMesh - attribs.position=%u indices=%u",
        (uint)attribs.position.size(),
        (uint)indices.size());

    /// add the surface
    SurfaceDef.surfaceName = "HandSurface";
    SurfaceDef.geo.Create(attribs, indices);
    {
        /// Build the graphics command
        ovrGraphicsCommand& gc = SurfaceDef.graphicsCommand;
        gc.Program = app.ProgHandSkinned;
        gc.UniformData[0].Data = &app.HandsTexture;
        gc.UniformData[1].Data = &app.SpecularLightDirection;
        gc.UniformData[2].Data = &app.SpecularLightColor;
        gc.UniformData[3].Data = &app.AmbientLightColor;
        /// bind the data matrix
        assert(MAX_JOINTS == SkinMatrices.size());
        gc.UniformData[4].Count = MAX_JOINTS;
        gc.UniformData[4].Data = &SkinUniformBuffer;
        /// bind the glow color
        gc.UniformData[5].Data = &GlowColor;
        /// bind the channel control
        gc.UniformData[6].Data = &app.ChannelControl;
        /// gpu state needs alpha blending
        gc.GpuState.depthEnable = gc.GpuState.depthMaskEnable = true;
        gc.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
        gc.GpuState.blendSrc = GL_SRC_ALPHA;
        gc.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;

        HandSurface.surface = &(SurfaceDef);
    }

    UpdateSkeleton(OVR::Posef::Identity());

    /// Capsules
    BoneCapsules.clear();
    CapsuleSurfacesDef.resize(skeleton->NumCapsules);
    CapsuleSurfaces.resize(skeleton->NumCapsules);
    CapsuleTransforms.resize(skeleton->NumCapsules);
    for (uint32_t i = 0; i < skeleton->NumCapsules; ++i) {
        const ovrBoneCapsule& capsule = skeleton->Capsules[i];
        BoneCapsules.push_back(capsule);

        const float capsuleHeight =
            (Vector3f(capsule.Points[0]) - Vector3f(capsule.Points[1])).Length();
        const Vector3f capsuleOffset =
            Vector3f((IsLeftHand() ? 1.0 : -1.0) * capsuleHeight / 2, 0, 0) +
            Vector3f(capsule.Points[0]);

        /// Add the geometry transform
        const Matrix4f capsuleMatrix =
            Matrix4f::Translation(capsuleOffset) /// offset cylinder back to the bone
            * Matrix4f::RotationY(
                  OVR::DegreeToRad(90.0f)) /// restore cylinder orientation to be along the X axis
            ;
        CapsuleTransforms[i] = capsuleMatrix;

        /// Create Capsule surface definition
        CapsuleSurfacesDef[i].surfaceName = "CapsuleSurface";
        CapsuleSurfacesDef[i].geo =
            OVRFW::BuildTesselatedCapsule(capsule.Radius, capsuleHeight, 10, 7);
        /// Build the graphics command
        auto& gc = CapsuleSurfacesDef[i].graphicsCommand;
        gc.Program = app.ProgHandCapsules;
        gc.UniformData[0].Data = &app.ChannelControl;
        gc.UniformData[1].Data = &app.SpecularLightDirection;
        gc.UniformData[2].Data = &app.SpecularLightColor;
        gc.UniformData[3].Data = &app.AmbientLightColor;
        gc.GpuState.depthEnable = gc.GpuState.depthMaskEnable = true;
        gc.GpuState.blendEnable = ovrGpuState::BLEND_DISABLE;
        gc.GpuState.blendSrc = GL_ONE;

        /// Add surface
        CapsuleSurfaces[i].surface = &(CapsuleSurfacesDef[i]);
    }

    /// Bone Axis rendering
    {
        /// Create Axis surface definition
        AxisSurfaceDef.surfaceName = "AxisSurfaces";
        AxisSurfaceDef.geo = OVRFW::BuildAxis(0.025f);
        AxisSurfaceDef.numInstances = skeleton->NumBones;
        /// Build the graphics command
        auto& gc = AxisSurfaceDef.graphicsCommand;
        gc.Program = app.ProgHandAxis;
        gc.UniformData[0].Data = &InstancedBoneUniformBuffer;
        gc.GpuState.depthEnable = gc.GpuState.depthMaskEnable = true;
        gc.GpuState.blendEnable = ovrGpuState::BLEND_DISABLE;
        gc.GpuState.blendSrc = GL_ONE;
        /// Add surface
        AxisSurface.surface = &(AxisSurfaceDef);
    }
}

OVR::Matrix4f ovrInputDeviceTrackedHand::GetModelMatrix(const OVR::Posef& handPose) const {
    return (Matrix4f(handPose) * Matrix4f::Scaling(SampleConfiguration.HandScaleFactor));
}

bool ovrInputDeviceTrackedHand::Update(
    ovrMobile* ovr,
    const double displayTimeInSeconds,
    const float dt) {
    const ovrTracking2 headTracking = vrapi_GetPredictedTracking2(ovr, displayTimeInSeconds);
    HeadPose = headTracking.HeadPose.Pose;

    /// Save Pinch state from last frame
    PreviousFramePinch = IsPinching();
    ovrResult result =
        vrapi_GetInputTrackingState(ovr, GetDeviceID(), displayTimeInSeconds, &Tracking);
    if (result != ovrSuccess) {
        return false;
    } else {
        HandPose = Tracking.HeadPose.Pose;
    }

    ovrResult r = ovrSuccess;
    InputStateHand.Header.ControllerType = GetType();
    r = vrapi_GetCurrentInputState(ovr, GetDeviceID(), &InputStateHand.Header);
    if (r != ovrSuccess) {
        ALOG("VrHands - failed to get hand input state.");
        return false;
    }

    RealHandPose.Header.Version = ovrHandVersion_1;
    r = vrapi_GetHandPose(ovr, GetDeviceID(), displayTimeInSeconds, &(RealHandPose.Header));
    if (r != ovrSuccess) {
        ALOG("VrHands - failed to get hand pose");
        return false;
    } else {
        SampleConfiguration.HandScaleFactor = RealHandPose.HandScale;
        {
            VRMenuObject* scaleDisplay = IsLeftHand() ? HandScaleDisplayL : HandScaleDisplayR;
            std::ostringstream ss;
            ss << (IsLeftHand() ? "size L " : "size R ") << RealHandPose.HandScale;
            scaleDisplay->SetText(ss.str().c_str());
        }

        /// Get the root pose from the API
        HandPose = RealHandPose.RootPose;
        /// Pointer poses
        PointerPose = InputStateHand.PointerPose;
        /// update based on hand pose
        HandModel.Update(RealHandPose);
        UpdateSkeleton(HandPose);
    }
    return true;
}

void ovrInputDeviceTrackedHand::Render(std::vector<ovrDrawSurface>& surfaceList) {
    if (SampleConfiguration.HandScaleFactor > 1.0f) {
        /// Hulk hands
        GlowColor = OVR::Vector3f(0.3f, 1.0f, 0.3f);
    } else if (SampleConfiguration.HandScaleFactor < 1.0f) {
        /// Tiny hands
        GlowColor = OVR::Vector3f(1.0f, 0.5f, 0.5f);
    } else {
        /// Default hands
        GlowColor = OVR::Vector3f(0.75f);
    }

    if (IsInSystemGesture()) {
        // make it more blue if we are in the system gesture
        GlowColor.z = 1.0f;
    }

    if (SampleConfiguration.ShowCapsules) {
        for (auto& surface : CapsuleSurfaces) {
            if (surface.surface != nullptr) {
                surfaceList.push_back(surface);
            }
        }
    }

    if (SampleConfiguration.ShowAxis) {
        if (AxisSurface.surface != nullptr) {
            surfaceList.push_back(AxisSurface);
        }
    }

    /// Render hand last
    surfaceList.push_back(HandSurface);
}

} // namespace OVRFW
