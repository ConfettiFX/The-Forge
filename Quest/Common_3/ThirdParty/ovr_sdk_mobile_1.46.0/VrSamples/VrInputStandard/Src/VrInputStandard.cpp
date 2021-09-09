/************************************************************************************

Filename    : VrInputStandard.cpp
Content     : Use of Standard Input API's to read unified poses & button state
              for hands and tracked controllers
Created     : 2020.03.31
Authors     : Lewis Weaver
Copyright   : Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "VrInputStandard.h"

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

#include "OVR_JSON.h"

#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <array>
#if defined(__ANDROID__)
#include <sys/system_properties.h>
#endif

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

static const Vector3f VERTICAL_LAYER_OFFSET(0, -0.02f, 0);

static const auto NUM_HAPTIC_STATES = 4;

HandSampleConfigurationParameters SampleConfiguration;

static const char* OculusTouchVertexShaderSrc = R"glsl(
attribute highp vec4 Position;
attribute highp vec3 Normal;
attribute highp vec3 Tangent;
attribute highp vec3 Binormal;
attribute highp vec2 TexCoord;

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
  gl_Position = TransformVertex( Position );
  vec3 eye = transposeMultiply( sm.ViewMatrix[VIEW_ID], -vec3( sm.ViewMatrix[VIEW_ID][3] ) );
  oEye = eye - vec3( ModelMatrix * Position );
  vec3 iNormal = Normal * 100.0f;
  oNormal = multiply( ModelMatrix, iNormal );
  oTexCoord = TexCoord;
}
)glsl";

static const char* OculusTouchFragmentShaderSrc = R"glsl(
uniform sampler2D Texture0;
uniform lowp vec3 SpecularLightDirection;
uniform lowp vec3 SpecularLightColor;
uniform lowp vec3 AmbientLightColor;

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

void main()
{
  lowp vec3 eyeDir = normalize( oEye.xyz );
  lowp vec3 Normal = normalize( oNormal );

  lowp vec3 reflectionDir = dot( eyeDir, Normal ) * 2.0 * Normal - eyeDir;
  lowp vec4 diffuse = texture2D( Texture0, oTexCoord );
  lowp vec3 ambientValue = diffuse.xyz * AmbientLightColor;

  lowp float nDotL = max( dot( Normal , SpecularLightDirection ), 0.0 );
  lowp vec3 diffuseValue = diffuse.xyz * SpecularLightColor * nDotL;

  lowp float specularPower = 1.0f - diffuse.a;
  specularPower = specularPower * specularPower;

  lowp vec3 H = normalize( SpecularLightDirection + eyeDir );
  lowp float nDotH = max( dot( Normal, H ), 0.0 );
  lowp float specularIntensity = pow( nDotH, 64.0f * ( specularPower ) ) * specularPower;
  lowp vec3 specularValue = specularIntensity * SpecularLightColor;

  lowp vec3 controllerColor = diffuseValue + ambientValue + specularValue;
  gl_FragColor.xyz = controllerColor;
  gl_FragColor.w = 1.0f;
}
)glsl";

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
  highp vec3 localNormal   = localNormal1 * JointWeights.x
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

  lowp vec4 diffuse = texture2D( Texture0, oTexCoord );
  lowp vec3 ambientValue = diffuse.xyz * AmbientLightColor;

  lowp float nDotL = max( dot( Normal, SpecularLightDirection ), 0.0 );
  lowp vec3 diffuseValue = diffuse.xyz * SpecularLightColor * nDotL;

  lowp vec3 H = normalize( SpecularLightDirection + eyeDir );
  lowp float nDotH = max( dot( Normal, H ), 0.0 );

  lowp float specularPower = 1.0f - diffuse.a;
  specularPower = specularPower * specularPower;
  lowp float specularIntensity = pow16( nDotH );
  lowp vec3 specularValue = specularIntensity * SpecularLightColor;

  lowp float vDotN = dot( eyeDir, Normal );
  lowp float fresnel = clamp( pow5( 1.0 - vDotN ), 0.0, 1.0 );
  lowp vec3 fresnelValue = GlowColor * fresnel;
  lowp vec3 controllerColor = diffuseValue
                            + ambientValue
                            + specularValue
                            + fresnelValue
                            ;
  gl_FragColor.xyz = controllerColor;
  gl_FragColor.w = clamp( fresnel, 0.0, 1.0 );
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

ovrVrInputStandard* theApp = nullptr;

VRMenuObject* FPSLabel = nullptr;
VRMenuObject* StatusLabel = nullptr;
VRMenuObject* KeyCodeLabel = nullptr;

VRMenuObject* BackgroundToggleButton = nullptr;

VRMenuObject* StandardDevicesToggleButton = nullptr;
VRMenuObject* NonStandardDevicesToggleButton = nullptr;

uint64_t FrameIndexToPrint = 0;
const float kHapticsGripThreashold = 0.1f;

// Matrix to get from tracking pose to the OpenXR compatible 'grip' pose
static const OVR::Matrix4f xfTrackedFromBinding = OVR::Matrix4f(OVR::Posef{
    OVR::Quatf{OVR::Vector3f{1, 0, 0}, OVR::DegreeToRad(60.0f)},
    OVR::Vector3f{0, -0.03, 0.04}});
static const OVR::Matrix4f xfTrackedFromBindingInv = xfTrackedFromBinding.Inverted();
static const OVR::Matrix4f xfPointerFromBinding =
    OVR::Matrix4f::Translation(OVR::Vector3(0.0f, 0.0f, -0.055f));

inline GlGeometry BuildCameraMesh(float side = 0.5f) {
    VertexAttribs attribs;

    attribs.position = {
        {-side, 0.0f, -side},
        {+side, 0.0f, -side},
        {+side, 0.0f, +side},
        {-side, 0.0f, +side}, // top
    };
    attribs.normal = {
        {0.0f, +1.0f, 0.0f}, {0.0f, +1.0f, 0.0f}, {0.0f, +1.0f, 0.0f}, {0.0f, +1.0f, 0.0f}, // top
    };
    attribs.uv0 = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, // top
    };

    std::vector<TriangleIndex> indices = {
        0, 2, 1, 2, 0, 3, // top
    };

    return GlGeometry(attribs, indices);
}

inline GlGeometry BuildTesselatedCapsuleBones(
    const float radius,
    const float height,
    const float scale = 1.0f,
    const TriangleIndex horizontal = 2,
    const TriangleIndex vertical = 2) {
    const int vertexCount = (horizontal + 1) * (vertical + 1);
    const float h = (radius + height) * 0.5f * scale;
    const float w = radius * scale;

    VertexAttribs attribs;
    attribs.position.resize(vertexCount);
    attribs.uv0.resize(vertexCount);

    for (int y = 0; y <= vertical; y++) {
        const float yf = (float)y / (float)vertical;
        for (int x = 0; x <= horizontal; x++) {
            const float xf = (float)x / (float)horizontal;
            const int index = y * (horizontal + 1) + x;
            attribs.position[index].x = (-1.0f + xf * 2.0f) * w;
            attribs.position[index].y = 0.0f;
            attribs.position[index].z = -h + yf * 2 * h;

            attribs.uv0[index].x = xf;
            attribs.uv0[index].y = 1.0f - yf;
        }
    }

    std::vector<TriangleIndex> indices;
    indices.resize(horizontal * vertical * 6 * 2);

    // If this is to be used to draw a linear format texture, like
    // a surface texture, it is better for cache performance that
    // the triangles be drawn to follow the side to side linear order.
    int index = 0;
    for (TriangleIndex y = 0; y < vertical; y++) {
        for (TriangleIndex x = 0; x < horizontal; x++) {
            indices[index + 0] = y * (horizontal + 1) + x;
            indices[index + 1] = y * (horizontal + 1) + x + 1;
            indices[index + 2] = (y + 1) * (horizontal + 1) + x;
            indices[index + 3] = (y + 1) * (horizontal + 1) + x;
            indices[index + 4] = y * (horizontal + 1) + x + 1;
            indices[index + 5] = (y + 1) * (horizontal + 1) + x + 1;
            index += 6;
        }
        // fix the quads in the upper right and lower left corners so that the triangles in the
        // quads share the edges going from the center of the tesselated quad to it's corners.
        const int upperLeftIndexStart = 0;
        indices[upperLeftIndexStart + 1] = indices[upperLeftIndexStart + 5];
        indices[upperLeftIndexStart + 3] = indices[upperLeftIndexStart + 0];

        const int lowerRightIndexStart = (horizontal * (vertical - 1) * 6) + (horizontal - 1) * 6;
        indices[lowerRightIndexStart + 1] = indices[lowerRightIndexStart + 5];
        indices[lowerRightIndexStart + 3] = indices[lowerRightIndexStart + 0];
    }

    /// Make this two sided
    for (TriangleIndex y = 0; y < vertical; y++) {
        for (TriangleIndex x = 0; x < horizontal; x++) {
            indices[index + 5] = y * (horizontal + 1) + x;
            indices[index + 4] = y * (horizontal + 1) + x + 1;
            indices[index + 3] = (y + 1) * (horizontal + 1) + x;
            indices[index + 2] = (y + 1) * (horizontal + 1) + x;
            indices[index + 1] = y * (horizontal + 1) + x + 1;
            indices[index + 0] = (y + 1) * (horizontal + 1) + x + 1;
            index += 6;
        }
    }

    return GlGeometry(attribs, indices);
}

inline Posef GetOffsetPose(Vector3f position, Quatf rotation, Vector3f offset) {
    return Posef(rotation, position) * Posef(Quatf(), offset);
}

//==============================
// ovrVrInputStandard::ovrVrInputStandard
ovrVrInputStandard::ovrVrInputStandard(
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
      BeamRenderer(nullptr),
      ControllerModelOculusTouchLeft(nullptr),
      ControllerModelOculusTouchRight(nullptr) {
    theApp = this;
}

//==============================
// ovrVrInputStandard::~ovrVrInputStandard
ovrVrInputStandard::~ovrVrInputStandard() {
    delete ControllerModelOculusTouchLeft;
    ControllerModelOculusTouchLeft = nullptr;
    delete ControllerModelOculusTouchRight;
    ControllerModelOculusTouchRight = nullptr;

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
// ovrVrInputStandard::AppInit
bool ovrVrInputStandard::AppInit(const OVRFW::ovrAppContext* context) {
    const ovrJava& jj = *(reinterpret_cast<const ovrJava*>(context->ContextForVrApi()));
    const xrJava ctx = JavaContextConvert(jj);
    FileSys = ovrFileSys::Create(ctx);
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
                          const Vector2f& size = {100.0f, 100.0f},
                          const std::string& postfix = "") -> VRMenuObject* {
        std::string menuName = "target_";
        menuName += s;
        menuName += postfix;
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

    const auto calcButtonPos = [](int col, int row) {
        return Vector3f(-1.0f + 1.1f * col, 2.25f - row * 0.35f, -1.9f);
    };

    // Column 0
    StandardDevicesToggleButton =
        createMenu("Toggle Standard Pointers", calcButtonPos(0, 0), {400.0f, 100.0f});
    ButtonHandlers[StandardDevicesToggleButton] = [&]() {
        SampleConfiguration.EnableStandardDevices = !SampleConfiguration.EnableStandardDevices;
        if (SampleConfiguration.EnableStandardDevices) {
            ALOG("Enabling standard pointers");
            StandardDevicesToggleButton->SetText("Disable Standard Pointers");
        } else {
            ALOG("Disabling standard pointers");
            StandardDevicesToggleButton->SetText("Enable Standard Pointers");
        }
    };
    ButtonHandlers[StandardDevicesToggleButton]();
    ButtonHandlers[StandardDevicesToggleButton]();

    auto showAxesButton = createMenu("Show Axes", calcButtonPos(0, 1), {400.0f, 100.0f});
    ButtonHandlers[showAxesButton] = [=]() {
        SampleConfiguration.RenderAxis = !SampleConfiguration.RenderAxis;
        if (SampleConfiguration.RenderAxis) {
            showAxesButton->SetText("Hide Axes");
        } else {
            showAxesButton->SetText("Show Axes");
        }
    };
    ButtonHandlers[showAxesButton]();
    ButtonHandlers[showAxesButton]();

    auto hapticsButton = createMenu("OnTrigger Haptics", calcButtonPos(0, 2), {400.0f, 100.0f});
    ButtonHandlers[hapticsButton] = [=]() {
        SampleConfiguration.OnTriggerHapticsState = (HapticStates)(
            ((int)SampleConfiguration.OnTriggerHapticsState + 1) % NUM_HAPTIC_STATES);

        switch (SampleConfiguration.OnTriggerHapticsState) {
            case HapticStates::HAPTICS_BUFFERED:
                hapticsButton->SetText("OnTrigger Haptics: Buffered");
                break;
            case HapticStates::HAPTICS_SIMPLE:
                hapticsButton->SetText("OnTrigger Haptics: Simple");
                break;
            case HapticStates::HAPTICS_SIMPLE_CLICKED:
                hapticsButton->SetText("OnTrigger Haptics: Simple MAX");
                break;
            case HapticStates::HAPTICS_NONE:
                hapticsButton->SetText("OnTrigger Haptics: None");
                break;
            default:
                break;
        }
    };
    ButtonHandlers[hapticsButton]();
    ButtonHandlers[hapticsButton]();

    // Column 1
    FPSLabel = createMenu("FPSLabel", calcButtonPos(1, 0), {300.0f, 100.0f});
    StatusLabel = createMenu("StatusLabel", calcButtonPos(1, 2), {300.0f, 350.0f});

    //------------------------------------------------------------------------------------------

    SpecularLightDirection = Vector3f(1.0f, 1.0f, 0.0f);
    SpecularLightColor = Vector3f(1.0f, 0.95f, 0.8f) * 0.75f;
    AmbientLightColor = Vector3f(1.0f, 1.0f, 1.0f) * 0.15f;

    static ovrProgramParm OculusTouchUniformParms[] = {
        {"Texture0", ovrProgramParmType::TEXTURE_SAMPLED},
        {"SpecularLightDirection", ovrProgramParmType::FLOAT_VECTOR3},
        {"SpecularLightColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"AmbientLightColor", ovrProgramParmType::FLOAT_VECTOR3},
    };
    ProgOculusTouch = GlProgram::Build(
        OculusTouchVertexShaderSrc,
        OculusTouchFragmentShaderSrc,
        OculusTouchUniformParms,
        sizeof(OculusTouchUniformParms) / sizeof(ovrProgramParm));

    static ovrProgramParm HandSkinnedUniformParms[] = {
        {"Texture0", ovrProgramParmType::TEXTURE_SAMPLED},
        {"SpecularLightDirection", ovrProgramParmType::FLOAT_VECTOR3},
        {"SpecularLightColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"AmbientLightColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"JointMatrices", ovrProgramParmType::BUFFER_UNIFORM},
        {"GlowColor", ovrProgramParmType::FLOAT_VECTOR3},
    };
    ProgHandSkinned = GlProgram::Build(
        HandPBRSkinned1VertexShaderSrc,
        HandPBRSkinned1FragmentShaderSrc,
        HandSkinnedUniformParms,
        sizeof(HandSkinnedUniformParms) / sizeof(ovrProgramParm));

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

    static ovrProgramParm AxisUniformParms[] = {
        {"JointMatrices", ovrProgramParmType::BUFFER_UNIFORM},
    };

    ProgAxis = GlProgram::Build(
        AxisVertexShaderSrc,
        AxisFragmentShaderSrc,
        AxisUniformParms,
        sizeof(AxisUniformParms) / sizeof(ovrProgramParm));

    TransformMatrices.resize(MAX_JOINTS, OVR::Matrix4f::Identity());
    AxisUniformBuffer.Create(
        GLBUFFER_TYPE_UNIFORM, MAX_JOINTS * sizeof(Matrix4f), TransformMatrices.data());

    /// Bone Axis rendering
    {
        /// Create Axis surface definition
        AxisSurfaceDef.surfaceName = "AxisSurfaces";
        AxisSurfaceDef.geo = OVRFW::BuildAxis(0.05f);
        AxisSurfaceDef.numInstances = 0;
        /// Build the graphics command
        auto& gc = AxisSurfaceDef.graphicsCommand;
        gc.Program = ProgAxis;
        gc.UniformData[0].Data = &AxisUniformBuffer;
        gc.GpuState.depthEnable = gc.GpuState.depthMaskEnable = true;
        gc.GpuState.blendEnable = ovrGpuState::BLEND_DISABLE;
        gc.GpuState.blendSrc = GL_ONE;
        /// Add surface
        AxisSurface.surface = &(AxisSurfaceDef);
    }

    /// QUEST controllers
    {
        ModelGlPrograms programs;
        programs.ProgSingleTexture = &ProgOculusTouch;
        programs.ProgBaseColorPBR = &ProgOculusTouch;
        programs.ProgSkinnedBaseColorPBR = &ProgOculusTouch;
        programs.ProgLightMapped = &ProgOculusTouch;
        programs.ProgBaseColorEmissivePBR = &ProgOculusTouch;
        programs.ProgSkinnedBaseColorEmissivePBR = &ProgOculusTouch;
        programs.ProgSimplePBR = &ProgOculusTouch;
        programs.ProgSkinnedSimplePBR = &ProgOculusTouch;

        MaterialParms materials;

        {
            const char* controllerModelFile =
                "apk:///assets/oculusQuest_oculusTouch_Right.gltf.ovrscene";
            ControllerModelOculusTouchRight =
                LoadModelFile(GuiSys->GetFileSys(), controllerModelFile, programs, materials);
            if (ControllerModelOculusTouchRight == NULL ||
                static_cast<int>(ControllerModelOculusTouchRight->Models.size()) < 1) {
                ALOGE_FAIL(
                    "Couldn't load Oculus Touch for Oculus Quest Controller Controller right model");
            }

            for (auto& model : ControllerModelOculusTouchRight->Models) {
                auto& gc = model.surfaces[0].surfaceDef.graphicsCommand;
                gc.UniformData[0].Data = &gc.Textures[0];
                gc.UniformData[1].Data = &SpecularLightDirection;
                gc.UniformData[2].Data = &SpecularLightColor;
                gc.UniformData[3].Data = &AmbientLightColor;
                gc.UniformData[4].Data = &gc.Textures[1];
            }
        }
        {
            const char* controllerModelFile =
                "apk:///assets/oculusQuest_oculusTouch_Left.gltf.ovrscene";
            ControllerModelOculusTouchLeft =
                LoadModelFile(GuiSys->GetFileSys(), controllerModelFile, programs, materials);
            if (ControllerModelOculusTouchLeft == NULL ||
                static_cast<int>(ControllerModelOculusTouchLeft->Models.size()) < 1) {
                ALOGE_FAIL(
                    "Couldn't load Oculus Touch for Oculus Quest Controller Controller left model");
            }

            for (auto& model : ControllerModelOculusTouchLeft->Models) {
                auto& gc = model.surfaces[0].surfaceDef.graphicsCommand;
                gc.UniformData[0].Data = &gc.Textures[0];
                gc.UniformData[1].Data = &SpecularLightDirection;
                gc.UniformData[2].Data = &SpecularLightColor;
                gc.UniformData[3].Data = &AmbientLightColor;
                gc.UniformData[4].Data = &gc.Textures[1];
            }
        }
    }

    //------------------------------------------------------------------------------------------

    SpriteAtlas = new ovrTextureAtlas();
    SpriteAtlas->Init(GuiSys->GetFileSys(), "apk:///assets/particles2.ktx");
    SpriteAtlas->BuildSpritesFromGrid(4, 2, 8);

    ParticleSystem = new ovrParticleSystem();
    auto particleGPUstate = ovrParticleSystem::GetDefaultGpuState();
    ParticleSystem->Init(2048, SpriteAtlas, particleGPUstate, false);

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
// ovrVrInputStandard::ResetLaserPointer
void ovrVrInputStandard::ResetLaserPointer(ovrInputDeviceHandBase& trDevice) {
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
// ovrVrInputStandard::AppShutdown
void ovrVrInputStandard::AppShutdown(const OVRFW::ovrAppContext* context) {
    ALOG("AppShutdown");
    for (int i = InputDevices.size() - 1; i >= 0; --i) {
        OnDeviceDisconnected(InputDevices[i]->GetDeviceID());
    }

    SurfaceRender.Shutdown();
}

OVR::Vector3f ProjectToPlane(const OVR::Posef& planePose, const OVR::Vector3f& p) {
    /// Find the plane normal
    const OVR::Vector3f normal = planePose.Rotation.Rotate(OVR::Vector3f{0, 1, 0});

    /// Project
    const OVR::Vector3f v = p - planePose.Translation;
    const float distance = v.Dot(normal);
    const OVR::Vector3f projected = p - (normal * distance);
    return projected;
}

OVR::Vector3f RayProjectToPlane(
    const OVR::Posef& planePose,
    const OVR::Vector3f& rayStart,
    const OVR::Vector3f& rayDirection) {
    /// Find the plane normal
    const OVR::Vector3f normal = planePose.Rotation.Rotate(OVR::Vector3f{0, 1, 0});

    /// Construct a plane around it
    OVR::Planef plane = OVR::Planef(normal, planePose.Translation);

    /// Find the direction along the ray that intersects the plane
    const float t = -(normal.Dot(rayStart) + plane.D) / normal.Dot(rayDirection);

    /// Project
    return rayStart + rayDirection * t;
}

OVRFW::ovrApplFrameOut ovrVrInputStandard::AppFrame(const OVRFW::ovrApplFrameIn& vrFrame) {
    const float frameTime = vrFrame.DeltaSeconds;
    const float frameRate = 1.0f / frameTime;
    if (FPSLabel) {
        std::ostringstream ss;
        ss << frameRate << " fps";
        FPSLabel->SetText(ss.str().c_str());
    }

    if (StatusLabel) {
        std::ostringstream statusLabelText;
        int connectedDevices = 0;
        for (auto device : InputDevices) {
            if (device->GetType() != ovrControllerType_StandardPointer) {
                continue;
            }

            connectedDevices++;
        }

        if (connectedDevices > 0 && SampleConfiguration.EnableStandardDevices) {
            statusLabelText << "Menu Press Count: " << MenuPressCount << std::endl;
            for (auto device : InputDevices) {
                if (device->GetType() != ovrControllerType_StandardPointer) {
                    continue;
                }

                connectedDevices++;

                auto trDevice = static_cast<ovrInputDeviceStandardPointer*>(device);

                statusLabelText << "Standard Pointer "
                                << (trDevice->IsLeftHand() ? "Left" : "Right") << std::endl;
                statusLabelText << "  Pointer Valid: "
                                << (trDevice->IsPointerValid() ? "Yes" : "No") << std::endl;
                statusLabelText << "  Pointer Pinching: " << (trDevice->IsPinching() ? "Yes" : "No")
                                << std::endl;
            }
        } else {
            statusLabelText.clear();
            statusLabelText << connectedDevices << " standard pointer\ndevices available";
        }

        StatusLabel->SetText(statusLabelText.str().c_str());
    }

    return OVRFW::ovrApplFrameOut();
}

void ovrVrInputStandard::RenderRunningFrame(
    const OVRFW::ovrApplFrameIn& in,
    OVRFW::ovrRendererOutput& out) {
    // disallow player movement
    ovrApplFrameIn vrFrameWithoutMove = in;
    vrFrameWithoutMove.LeftRemoteJoystick.x = 0.0f;
    vrFrameWithoutMove.LeftRemoteJoystick.y = 0.0f;

    bool printPoseNow = false;
    if (FrameIndexToPrint != 0 && FrameIndexToPrint == GetFrameIndex()) {
        printPoseNow = true;
        FrameIndexToPrint = 0;
    }

    //------------------------------------------------------------------------------------------

    EnumerateInputDevices();

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

        auto deviceType = device->GetType();
        if (deviceType == ovrControllerType_TrackedRemote || deviceType == ovrControllerType_Hand ||
            deviceType == ovrControllerType_StandardPointer) {
            ovrInputDeviceHandBase& trDevice = *static_cast<ovrInputDeviceHandBase*>(device);

            if (deviceID != ovrDeviceIdType_Invalid) {
                if (false ==
                    trDevice.Update(GetSessionObject(), in.PredictedDisplayTime, in.DeltaSeconds)) {
                    OnDeviceDisconnected(deviceID);
                    continue;
                }
            }
        }
    }

    //------------------------------------------------------------------------------------------

    // Force ignoring motion
    vrFrameWithoutMove.LeftRemoteTracked = false;
    vrFrameWithoutMove.RightRemoteTracked = false;

    // Player movement.
    Scene.SetFreeMove(true);
    Scene.Frame(vrFrameWithoutMove);

    Scene.GetFrameMatrices(SuggestedEyeFovDegreesX, SuggestedEyeFovDegreesY, out.FrameMatrices);
    Scene.GenerateFrameSurfaceList(out.FrameMatrices, out.Surfaces);

    // Clean all hit testing
    for (int i = (int)InputDevices.size() - 1; i >= 0; --i) {
        ovrInputDeviceBase* device = InputDevices[i];
        if (device != nullptr &&
            (device->GetType() == ovrControllerType_TrackedRemote ||
             device->GetType() == ovrControllerType_Hand ||
             device->GetType() == ovrControllerType_StandardPointer)) {
            // clean last hit object
            ovrInputDeviceHandBase& trDevice = *static_cast<ovrInputDeviceHandBase*>(device);
            VRMenuObject* lastHitObject =
                GuiSys->GetVRMenuMgr().ToObject(trDevice.GetLastHitHandle());
            if (lastHitObject != nullptr) {
                lastHitObject->SetSurfaceColor(0, MENU_DEFAULT_COLOR);
            }
        }
    }

    // Establish which devices are active before we iterate; since the button action handlers
    // may change this (resulting in the button being clicked twice)
    EnabledInputDevices.clear();
    for (int i = (int)InputDevices.size() - 1; i >= 0; --i) {
        ovrInputDeviceBase* device = InputDevices[i];
        if (device == nullptr) {
            ALOGW("RenderRunningFrame - device == nullptr ");
            assert(false); // this should never happen!
            continue;
        }
        ovrDeviceID deviceID = device->GetDeviceID();
        if (deviceID == ovrDeviceIdType_Invalid) {
            ALOGW("RenderRunningFrame - deviceID == ovrDeviceIdType_Invalid ");
            assert(deviceID != ovrDeviceIdType_Invalid);
            continue;
        }

        if (device->GetType() != ovrControllerType_TrackedRemote &&
            device->GetType() != ovrControllerType_Hand &&
            device->GetType() != ovrControllerType_StandardPointer) {
            continue;
        }
        ovrInputDeviceHandBase* trDevice = static_cast<ovrInputDeviceHandBase*>(device);

        if (!IsDeviceTypeEnabled(*device)) {
            ResetLaserPointer(*trDevice);
            trDevice->ResetHaptics(GetSessionObject(), in.PredictedDisplayTime);
            continue;
        }

        EnabledInputDevices.push_back(trDevice);
    }

    // loop through all devices to update controller arm models and place the pointer for the
    // dominant hand
    Matrix4f traceMat(out.FrameMatrices.CenterView.Inverted());
    for (auto devIter = EnabledInputDevices.begin(); devIter != EnabledInputDevices.end();
         devIter++) {
        auto& trDevice = **devIter;

        bool updateLaser = trDevice.IsPinching();
        bool renderLaser = trDevice.IsPointerValid();

        if (trDevice.IsMenuPressed() && trDevice.GetType() == ovrControllerType_StandardPointer) {
            ++MenuPressCount;
        }

        trDevice.UpdateHaptics(GetSessionObject(), in.PredictedDisplayTime);

        if (renderLaser) {
            Vector3f pointerStart(0.0f);
            Vector3f pointerEnd(0.0f);
            bool LaserHit = false;
            pointerStart = trDevice.GetRayOrigin();
            pointerEnd = trDevice.GetRayEnd();
            Vector3f const pointerDir = (pointerEnd - pointerStart).Normalized();
            Vector3f targetEnd = pointerStart + pointerDir * (updateLaser ? 10.0f : 0.075f);

            HitTestResult hit = GuiSys->TestRayIntersection(pointerStart, pointerDir);
            LaserHit =
                hit.HitHandle.IsValid() && (trDevice.GetHandPoseConfidence() == ovrConfidence_HIGH);
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
            float confidenceAlpha =
                trDevice.GetHandPoseConfidence() == ovrConfidence_HIGH ? 1.0f : 0.1f;
            ConfidenceLaserColor.x = 1.0f - confidenceAlpha;
            ConfidenceLaserColor.y = confidenceAlpha;
            ConfidenceLaserColor.z = 0.0f;
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
    //------------------------------------------------------------------------------------------

    GuiSys->Frame(in, out.FrameMatrices.CenterView, traceMat);
    BeamRenderer->Frame(in, out.FrameMatrices.CenterView, *BeamAtlas);
    ParticleSystem->Frame(in, SpriteAtlas, out.FrameMatrices.CenterView);

    GuiSys->AppendSurfaceList(out.FrameMatrices.CenterView, &out.Surfaces);

    // render bones first
    const Matrix4f projectionMatrix;
    ParticleSystem->RenderEyeView(out.FrameMatrices.CenterView, projectionMatrix, out.Surfaces);
    BeamRenderer->RenderEyeView(out.FrameMatrices.CenterView, projectionMatrix, out.Surfaces);

    int axisSurfaces = 0;

    // add the controller model surfaces to the list of surfaces to render
    for (int i = 0; i < (int)InputDevices.size(); ++i) {
        ovrInputDeviceBase* device = InputDevices[i];
        if (device == nullptr) {
            assert(false); // this should never happen!
            continue;
        }
        if (device->GetType() != ovrControllerType_TrackedRemote &&
            device->GetType() != ovrControllerType_Hand &&
            device->GetType() != ovrControllerType_StandardPointer) {
            continue;
        }
        if (!IsDeviceTypeEnabled(*device)) {
            continue;
        }
        ovrInputDeviceHandBase& trDevice = *static_cast<ovrInputDeviceHandBase*>(device);

        const Posef& handPose = trDevice.GetHandPose();
        const Matrix4f matDeviceModel = trDevice.GetModelMatrix(handPose);
        TransformMatrices[axisSurfaces++] = OVR::Matrix4f(handPose);
        TransformMatrices[axisSurfaces++] = matDeviceModel;
        TransformMatrices[axisSurfaces++] = trDevice.GetPointerMatrix();

        bool renderHand = (trDevice.GetHandPoseConfidence() == ovrConfidence_HIGH);
        if (renderHand) {
            trDevice.Render(out.Surfaces);
        }
    }

    // Add axis
    if (SampleConfiguration.RenderAxis && AxisSurface.surface != nullptr) {
        const_cast<OVRFW::ovrSurfaceDef*>(AxisSurface.surface)->numInstances = axisSurfaces;
        AxisSurface.modelMatrix = OVR::Matrix4f::Identity();
        for (int j = 0; j < axisSurfaces; ++j) {
            TransformMatrices[j] = TransformMatrices[j].Transposed();
        }
        AxisUniformBuffer.Update(
            TransformMatrices.size() * sizeof(Matrix4f), TransformMatrices.data());
        out.Surfaces.push_back(AxisSurface);
    }
}

void ovrVrInputStandard::AppRenderEye(
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

void ovrVrInputStandard::AppRenderFrame(
    const OVRFW::ovrApplFrameIn& in,
    OVRFW::ovrRendererOutput& out) {
    switch (RenderState) {
        case RENDER_STATE_LOADING: {
            DefaultRenderFrame_Loading(in, out);
        } break;
        case RENDER_STATE_RUNNING: {
            RenderRunningFrame(in, out);
            SubmitCompositorLayers(in, out);
        } break;
        case RENDER_STATE_ENDING: {
            DefaultRenderFrame_Ending(in, out);
        } break;
    }
}

void ovrVrInputStandard::SubmitCompositorLayers(const ovrApplFrameIn& in, ovrRendererOutput& out) {
    // set up layers
    int& layerCount = NumLayers;
    layerCount = 0;

    /// Add content layer
    ovrLayerProjection2& layer = Layers[layerCount].Projection;
    layer = vrapi_DefaultLayerProjection2();
    layer.HeadPose = Tracking.HeadPose;
    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; ++eye) {
        ovrFramebuffer* framebuffer = GetFrameBuffer(GetNumFramebuffers() == 1 ? 0 : eye);
        layer.Textures[eye].ColorSwapChain = framebuffer->ColorTextureSwapChain;
        layer.Textures[eye].SwapChainIndex = framebuffer->TextureSwapChainIndex;
        layer.Textures[eye].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection(
            (ovrMatrix4f*)&out.FrameMatrices.EyeProjection[eye]);
    }
    layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
    layer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA;
    layer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA;
    layerCount++;

    // render images for each eye
    for (int eye = 0; eye < GetNumFramebuffers(); ++eye) {
        ovrFramebuffer* framebuffer = GetFrameBuffer(eye);
        ovrFramebuffer_SetCurrent(framebuffer);

        AppEyeGLStateSetup(in, framebuffer, eye);
        AppRenderEye(in, out, eye);

        ovrFramebuffer_Resolve(framebuffer);
        ovrFramebuffer_Advance(framebuffer);
    }

    ovrFramebuffer_SetNone();
}

void ovrVrInputStandard::AppEyeGLStateSetup(const ovrApplFrameIn&, const ovrFramebuffer* fb, int) {
    GL(glEnable(GL_SCISSOR_TEST));
    GL(glDepthMask(GL_TRUE));
    GL(glEnable(GL_DEPTH_TEST));
    GL(glDepthFunc(GL_LEQUAL));
    GL(glEnable(GL_CULL_FACE));
    GL(glViewport(0, 0, fb->Width, fb->Height));
    GL(glScissor(0, 0, fb->Width, fb->Height));
    GL(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    // This app was originally written with the presumption that
    // its swapchains and compositor front buffer were RGB.
    // In order to have the colors the same now that its compositing
    // to an sRGB front buffer, we have to write to an sRGB swapchain
    // but with the linear->sRGB conversion disabled on write.
    GL(glDisable(GL_FRAMEBUFFER_SRGB_EXT));
}

//---------------------------------------------------------------------------------------------------
// Input device management
//---------------------------------------------------------------------------------------------------

//==============================
// ovrVrInputStandard::FindInputDevice
int ovrVrInputStandard::FindInputDevice(const ovrDeviceID deviceID) const {
    for (int i = 0; i < (int)InputDevices.size(); ++i) {
        if (InputDevices[i]->GetDeviceID() == deviceID) {
            return i;
        }
    }
    return -1;
}

//==============================
// ovrVrInputStandard::RemoveDevice
void ovrVrInputStandard::RemoveDevice(const ovrDeviceID deviceID) {
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
// ovrVrInputStandard::IsDeviceTracked
bool ovrVrInputStandard::IsDeviceTracked(const ovrDeviceID deviceID) const {
    return FindInputDevice(deviceID) >= 0;
}

//==============================
// ovrVrInputStandard::EnumerateInputDevices
void ovrVrInputStandard::EnumerateInputDevices() {
    for (uint32_t deviceIndex = 0;; deviceIndex++) {
        ovrInputCapabilityHeader curCaps;

        if (vrapi_EnumerateInputDevices(GetSessionObject(), deviceIndex, &curCaps) < 0) {
            break; // no more devices
        }

        if (!IsDeviceTracked(curCaps.DeviceID)) {
            ALOG("Input -      tracked");
            OnDeviceConnected(curCaps);
        }
    }
}

//==============================
// ovrVrInputStandard::OnDeviceConnected
void ovrVrInputStandard::OnDeviceConnected(const ovrInputCapabilityHeader& capsHeader) {
    ovrInputDeviceBase* device = nullptr;
    ovrResult result = ovrError_NotInitialized;
    switch (capsHeader.Type) {
        case ovrControllerType_TrackedRemote: {
            ALOG("VrInputStandard - Controller connected, ID = %u", capsHeader.DeviceID);

            ovrInputTrackedRemoteCapabilities remoteCapabilities;
            remoteCapabilities.Header = capsHeader;
            result =
                vrapi_GetInputDeviceCapabilities(GetSessionObject(), &remoteCapabilities.Header);
            if (result == ovrSuccess) {
                ovrInputDeviceTrackedRemoteHand* remoteHandDevice =
                    ovrInputDeviceTrackedRemoteHand::Create(*this, remoteCapabilities);
                if (remoteHandDevice != nullptr) {
                    device = remoteHandDevice;
                    ovrHandMesh mesh;
                    mesh.Header.Version = ovrHandVersion_1;
                    ovrHandedness handedness =
                        remoteHandDevice->IsLeftHand() ? VRAPI_HAND_LEFT : VRAPI_HAND_RIGHT;
                    if (vrapi_GetHandMesh(GetSessionObject(), handedness, &mesh.Header) !=
                        ovrSuccess) {
                        ALOG("VrInputStandard - failed to get hand mesh");
                    }
                    ovrHandSkeleton skeleton;
                    skeleton.Header.Version = ovrHandVersion_1;
                    if (vrapi_GetHandSkeleton(GetSessionObject(), handedness, &skeleton.Header) !=
                        ovrSuccess) {
                        ALOG("VrInputStandard - failed to get hand skeleton");
                    }
                    remoteHandDevice->InitFromSkeletonAndMesh(*this, &skeleton, &mesh);

                    if (remoteHandDevice->IsLeftHand()) {
                        remoteHandDevice->SetControllerModel(ControllerModelOculusTouchLeft);
                    } else {
                        remoteHandDevice->SetControllerModel(ControllerModelOculusTouchRight);
                    }
                }
            }
            break;
        }
        case ovrControllerType_Hand: {
            ALOG("VrInputStandard - Hand connected, ID = %u", capsHeader.DeviceID);

            ovrInputHandCapabilities handCapabilities;
            handCapabilities.Header = capsHeader;
            ALOG("VrInputStandard - calling get device caps");
            result = vrapi_GetInputDeviceCapabilities(GetSessionObject(), &handCapabilities.Header);
            ALOG("VrInputStandard - post calling get device caps %d", result);
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
                        ALOG("VrInputStandard - failed to get hand skeleton");
                    } else {
                        ALOG("VrInputStandard - got a skeleton ... NumBones:%u", skeleton.NumBones);
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

                    ovrHandMesh mesh;
                    mesh.Header.Version = ovrHandVersion_1;
                    if (vrapi_GetHandMesh(GetSessionObject(), handedness, &mesh.Header) !=
                        ovrSuccess) {
                        ALOG("VrInputStandard - failed to get hand mesh");
                    }

                    handDevice->InitFromSkeletonAndMesh(*this, &skeleton, &mesh);
                }
            }
            break;
        }
        case ovrControllerType_StandardPointer: {
            ALOG("VrInputStandard - StandardPointer connected, ID = %u", capsHeader.DeviceID);

            ovrInputStandardPointerCapabilities pointerCaps;
            pointerCaps.Header = capsHeader;
            ALOG("VrInputStandard - StandardPointer calling get device caps");
            result = vrapi_GetInputDeviceCapabilities(GetSessionObject(), &pointerCaps.Header);
            ALOG("VrInputStandard - StandardPointer post calling get device caps %d", result);

            if (result == ovrSuccess) {
                device = ovrInputDeviceStandardPointer::Create(*this, pointerCaps);
                ALOG("VrInputStandard - StandardPointer created device");
            }

            break;
        }

        default:
            ALOG("VrInputStandard - Unknown device connected!");
            return;
    }

    if (result != ovrSuccess) {
        ALOG("VrInputStandard - vrapi_GetInputDeviceCapabilities: Error %i", result);
    }
    if (device != nullptr) {
        ALOG(
            "VrInputStandard - Added device '%s', id = %u", device->GetName(), capsHeader.DeviceID);
        InputDevices.push_back(device);
    } else {
        ALOG("VrInputStandard - Device creation failed for id = %u", capsHeader.DeviceID);
    }
}

bool ovrVrInputStandard::IsDeviceTypeEnabled(const ovrInputDeviceBase& device) const {
    auto deviceType = device.GetType();
    if (deviceType == ovrControllerType_StandardPointer) {
        return SampleConfiguration.EnableStandardDevices;
    } else {
        return !SampleConfiguration.EnableStandardDevices;
    }
}

void ovrVrInputStandard::OnDeviceDisconnected(const ovrDeviceID deviceID) {
    ALOG("VrInputStandard - Controller disconnected, ID = %i", deviceID);
    int deviceIndex = FindInputDevice(deviceID);
    if (deviceIndex >= 0) {
        ovrInputDeviceBase* device = InputDevices[deviceIndex];
        if (device != nullptr) {
            auto deviceType = device->GetType();
            if (deviceType == ovrControllerType_TrackedRemote ||
                deviceType == ovrControllerType_Hand ||
                deviceType == ovrControllerType_StandardPointer) {
                ovrInputDeviceHandBase& trDevice = *static_cast<ovrInputDeviceHandBase*>(device);
                ResetLaserPointer(trDevice);
            }
        }
    }
    RemoveDevice(deviceID);
}

void ovrVrInputStandard::AppResumed(const OVRFW::ovrAppContext* /* context */) {
    ALOGV("ovrVrInputStandard::AppResumed");
    RenderState = RENDER_STATE_RUNNING;
}

void ovrVrInputStandard::AppPaused(const OVRFW::ovrAppContext* /* context */) {
    ALOGV("ovrVrInputStandard::AppPaused");
}

void ovrInputDeviceHandBase::InitFromSkeletonAndMesh(
    ovrVrInputStandard& app,
    ovrHandSkeleton* skeleton,
    ovrHandMesh* mesh) {
    if (mesh == nullptr) {
        ALOGW("InitFromSkeletonAndMesh - mesh == nullptr");
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

    SurfaceDef.surfaceName = "HandSurface";
    SurfaceDef.geo.Create(attribs, indices);

    /// Build the graphics command
    ovrGraphicsCommand& gc = SurfaceDef.graphicsCommand;
    gc.Program = app.ProgHandSkinned;
    gc.UniformData[0].Data = &gc.Textures[0];
    gc.UniformData[1].Data = &app.SpecularLightDirection;
    gc.UniformData[2].Data = &app.SpecularLightColor;
    gc.UniformData[3].Data = &app.AmbientLightColor;
    /// bind the data matrix
    assert(MAX_JOINTS == SkinMatrices.size());
    gc.UniformData[4].Count = MAX_JOINTS;
    gc.UniformData[4].Data = &SkinUniformBuffer;
    /// bind the glow color
    gc.UniformData[5].Data = &GlowColor;
    /// gpu state needs alpha blending
    gc.GpuState.depthEnable = gc.GpuState.depthMaskEnable = true;
    gc.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
    gc.GpuState.blendSrc = GL_SRC_ALPHA;
    gc.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;

    /// add the surface
    Surfaces.clear();
    ovrDrawSurface handSurface;
    handSurface.surface = &(SurfaceDef);
    Surfaces.push_back(handSurface);

    UpdateSkeleton(OVR::Posef::Identity());
}

void ovrInputDeviceHandBase::UpdateSkeleton(const OVR::Posef& handPose) {
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

    /// Ensure the surface is using this uniform parameters
    for (auto& surface : Surfaces) {
        ovrSurfaceDef* sd = const_cast<ovrSurfaceDef*>(surface.surface);
        sd->graphicsCommand.UniformData[4].Count = MAX_JOINTS;
        sd->graphicsCommand.UniformData[4].Data = &SkinUniformBuffer;
        surface.modelMatrix = matDeviceModel;
    }
}

bool ovrInputDeviceHandBase::Update(
    ovrMobile* ovr,
    const double displayTimeInSeconds,
    const float dt) {
    const ovrTracking2 headTracking = vrapi_GetPredictedTracking2(ovr, displayTimeInSeconds);
    HeadPose = headTracking.HeadPose.Pose;

    /// Save Pinch state from last frame
    PreviousFramePinch = IsPinching();
    PreviousFrameMenu = IsMenuPressed();

    ovrResult result =
        vrapi_GetInputTrackingState(ovr, GetDeviceID(), displayTimeInSeconds, &Tracking);
    if (result != ovrSuccess) {
        return false;
    } else {
        HandPose = Tracking.HeadPose.Pose;
    }

    return true;
}

void ovrInputDeviceHandBase::UpdateHaptics(ovrMobile* ovr, float displayTimeInSeconds) {
    if (!HasCapSimpleHaptics() && !HasCapBufferedHaptics()) {
        return;
    }

    const DeviceHapticState& desiredState = GetRequestedHapticsState();
    const auto hapticMaxSamples = GetHapticSamplesMax();
    const auto hapticSampleDurationMs = GetHapticSampleDurationMS();

    if (desiredState.HapticState == HapticStates::HAPTICS_BUFFERED) {
        if (HasCapBufferedHaptics()) {
            // buffered haptics
            float intensity = 0.0f;
            intensity = fmodf(displayTimeInSeconds, 1.0f);

            ovrHapticBuffer hapticBuffer;
            uint8_t dataBuffer[hapticMaxSamples];
            hapticBuffer.BufferTime = displayTimeInSeconds;
            hapticBuffer.NumSamples = hapticMaxSamples;
            hapticBuffer.HapticBuffer = dataBuffer;
            hapticBuffer.Terminated = false;

            for (uint32_t i = 0; i < hapticMaxSamples; i++) {
                dataBuffer[i] = intensity * 255;
                intensity += hapticSampleDurationMs * 0.001f;
                intensity = fmodf(intensity, 1.0f);
            }

            vrapi_SetHapticVibrationBuffer(ovr, GetDeviceID(), &hapticBuffer);
            PreviousHapticState.HapticState = HAPTICS_BUFFERED;
        } else {
            ALOG("Device does not support buffered haptics?");
        }
    } else if (desiredState.HapticState == HapticStates::HAPTICS_SIMPLE_CLICKED) {
        // simple haptics
        if (HasCapSimpleHaptics()) {
            if (PreviousHapticState.HapticState != HAPTICS_SIMPLE_CLICKED) {
                vrapi_SetHapticVibrationSimple(ovr, GetDeviceID(), 1.0f);
                PreviousHapticState = {HAPTICS_SIMPLE_CLICKED, 1.0f};
            }
        } else {
            ALOG("Device does not support simple haptics?");
        }
    } else if (desiredState.HapticState == HapticStates::HAPTICS_SIMPLE) {
        // huge epsilon value since there is so much noise in the grip trigger
        // and currently a problem with sending too many haptics values.
        if (PreviousHapticState.HapticSimpleValue < (desiredState.HapticSimpleValue - 0.05f) ||
            PreviousHapticState.HapticSimpleValue > (desiredState.HapticSimpleValue + 0.05f)) {
            vrapi_SetHapticVibrationSimple(ovr, GetDeviceID(), desiredState.HapticSimpleValue);
            PreviousHapticState = desiredState;
        }
    } else {
        if (PreviousHapticState.HapticState == HAPTICS_BUFFERED) {
            ovrHapticBuffer hapticBuffer;
            uint8_t dataBuffer[hapticMaxSamples];
            hapticBuffer.BufferTime = displayTimeInSeconds;
            hapticBuffer.NumSamples = hapticMaxSamples;
            hapticBuffer.HapticBuffer = dataBuffer;
            hapticBuffer.Terminated = true;

            for (uint32_t i = 0; i < hapticMaxSamples; i++) {
                dataBuffer[i] = (((float)i) / (float)hapticMaxSamples) * 255;
            }

            vrapi_SetHapticVibrationBuffer(ovr, GetDeviceID(), &hapticBuffer);
            PreviousHapticState = {};
        } else if (
            PreviousHapticState.HapticState == HAPTICS_SIMPLE ||
            PreviousHapticState.HapticState == HAPTICS_SIMPLE_CLICKED) {
            vrapi_SetHapticVibrationSimple(ovr, GetDeviceID(), 0.0f);
            PreviousHapticState = {};
        }
    }
}

//==============================
// ovrInputDeviceTrackedRemoteHand::Create
ovrInputDeviceTrackedRemoteHand* ovrInputDeviceTrackedRemoteHand::Create(
    OVRFW::ovrAppl& app,
    const ovrInputTrackedRemoteCapabilities& remoteCapabilities) {
    ALOG("VrInputStandard - ovrInputDeviceTrackedRemoteHand::Create");

    ovrInputStateTrackedRemote remoteInputState;
    remoteInputState.Header.ControllerType = remoteCapabilities.Header.Type;
    remoteInputState.Header.TimeInSeconds = 0.0f;
    ovrResult result = vrapi_GetCurrentInputState(
        app.GetSessionObject(), remoteCapabilities.Header.DeviceID, &remoteInputState.Header);
    if (result == ovrSuccess) {
        ovrHandedness controllerHand =
            (remoteCapabilities.ControllerCapabilities & ovrControllerCaps_LeftHand) != 0
            ? VRAPI_HAND_LEFT
            : VRAPI_HAND_RIGHT;
        ovrInputDeviceTrackedRemoteHand* device =
            new ovrInputDeviceTrackedRemoteHand(remoteCapabilities, controllerHand);
        return device;
    } else {
        ALOG("VrInputStandard - vrapi_GetCurrentInputState: Error %i", result);
    }

    return nullptr;
}

OVR::Matrix4f ovrInputDeviceTrackedRemoteHand::GetModelMatrix(const OVR::Posef& handPose) const {
    OVR::Matrix4f mat(handPose);

    mat = mat * xfTrackedFromBindingInv;

    const float controllerPitch = IsLeftHand() ? OVR::DegreeToRad(180.0f) : 0.0f;
    const float controllerYaw = IsLeftHand() ? OVR::DegreeToRad(90.0f) : OVR::DegreeToRad(-90.0f);
    return mat * Matrix4f::RotationY(controllerYaw) * Matrix4f::RotationX(controllerPitch);
}

bool ovrInputDeviceTrackedRemoteHand::Update(
    ovrMobile* ovr,
    const double displayTimeInSeconds,
    const float dt) {
    bool ret = ovrInputDeviceHandBase::Update(ovr, displayTimeInSeconds, dt);

    if (ret) {
        PointerPose = HandPose;
        HandPose = OVR::Posef(OVR::Matrix4f(HandPose) * xfTrackedFromBinding);
        /// Pointer is at hand for controller

        ovrInputStateTrackedRemote remoteInputState;
        remoteInputState.Header.ControllerType = GetType();
        remoteInputState.Header.TimeInSeconds = 0.0f;
        ovrResult r = vrapi_GetCurrentInputState(ovr, GetDeviceID(), &remoteInputState.Header);
        if (r == ovrSuccess) {
            IsPinchingInternal = remoteInputState.IndexTrigger > 0.99f;

            PreviousFrameMenuPressed = IsMenuPressedInternal;
            IsMenuPressedInternal = (remoteInputState.Buttons & ovrButton_Enter) != 0;

            UpdateHapticRequestedState(remoteInputState);
        }
    }
    return ret;
}

void ovrInputDeviceTrackedRemoteHand::Render(std::vector<ovrDrawSurface>& surfaceList) {
    // We have controller models
    if (nullptr != ControllerModel && Surfaces.size() > 1u) {
        // Render controller
        if (Surfaces[0].surface != nullptr) {
            Surfaces[0].modelMatrix = Matrix4f(HandPose) * xfTrackedFromBindingInv *
                Matrix4f::RotationY(OVR::DegreeToRad(180.0f)) *
                Matrix4f::RotationX(OVR::DegreeToRad(-90.0f));
            surfaceList.push_back(Surfaces[0]);
        }
    }
}

OVR::Matrix4f ovrInputDeviceTrackedRemoteHand::GetPointerMatrix() const {
    return OVR::Matrix4f(PointerPose) * xfPointerFromBinding;
}

void ovrInputDeviceTrackedRemoteHand::SetControllerModel(ModelFile* m) {
    /// we always want to keep rendering the hand
    if (m == nullptr) {
        // stop rendering controller - ensure that we only have the one surface
        Surfaces.resize(1);
    } else {
        // start rendering controller
        ControllerModel = m;
        // Add a surface for it
        Surfaces.resize(2);
        // Ensure we render controller ( Surfaces[0] ) before hand ( Surfaces[1] )
        Surfaces[1] = Surfaces[0];
        // The current model only has one surface, but this will ensure we don't overflow
        for (auto& model : ControllerModel->Models) {
            ovrDrawSurface controllerSurface;
            controllerSurface.surface = &(model.surfaces[0].surfaceDef);
            Surfaces[0] = controllerSurface;
        }
    }
}

void ovrInputDeviceTrackedRemoteHand::UpdateHapticRequestedState(
    const ovrInputStateTrackedRemote& remoteInputState) {
    if (remoteInputState.IndexTrigger > kHapticsGripThreashold) {
        RequestedHapticState = {
            SampleConfiguration.OnTriggerHapticsState, remoteInputState.IndexTrigger};
    } else {
        RequestedHapticState = {};
    }
}

void ovrInputDeviceTrackedRemoteHand::ResetHaptics(ovrMobile* ovr, float displayTimeInSeconds) {
    RequestedHapticState = {};
    UpdateHaptics(ovr, displayTimeInSeconds);
}

//==============================
// ovrInputDeviceTrackedHand::Create
ovrInputDeviceTrackedHand* ovrInputDeviceTrackedHand::Create(
    OVRFW::ovrAppl& app,
    const ovrInputHandCapabilities& capsHeader) {
    ALOG("VrInputStandard - ovrInputDeviceTrackedHand::Create");

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
        ALOG("VrInputStandard - vrapi_GetCurrentInputState: Error %i", result);
    }

    return nullptr;
}

void ovrInputDeviceTrackedHand::InitFromSkeletonAndMesh(
    ovrVrInputStandard& app,
    ovrHandSkeleton* skeleton,
    ovrHandMesh* mesh) {
    /// Base
    ovrInputDeviceHandBase::InitFromSkeletonAndMesh(app, skeleton, mesh);
}

bool ovrInputDeviceTrackedHand::Update(
    ovrMobile* ovr,
    const double displayTimeInSeconds,
    const float dt) {
    bool ret = ovrInputDeviceHandBase::Update(ovr, displayTimeInSeconds, dt);
    if (ret) {
        ovrResult r = ovrSuccess;
        InputStateHand.Header.ControllerType = GetType();
        InputStateHand.Header.TimeInSeconds = 0.0f;
        r = vrapi_GetCurrentInputState(ovr, GetDeviceID(), &InputStateHand.Header);
        if (r != ovrSuccess) {
            ALOG("VrInputStandard - failed to get hand input state.");
            return false;
        }

        RealHandPose.Header.Version = ovrHandVersion_1;
        r = vrapi_GetHandPose(ovr, GetDeviceID(), displayTimeInSeconds, &(RealHandPose.Header));
        if (r != ovrSuccess) {
            ALOG("VrInputStandard - failed to get hand pose");
            return false;
        } else {
            /// Get the root pose from the API
            HandPose = RealHandPose.RootPose;
            /// Pointer poses
            PointerPose = InputStateHand.PointerPose;
            /// update based on hand pose
            HandModel.Update(RealHandPose);
            UpdateSkeleton(HandPose);
        }

        if (IsPinching() != PreviousFramePinch)
            ALOG("HAND IsPinching = %s", (IsPinching() ? "Y" : "N"));
    }
    return ret;
}

void ovrInputDeviceTrackedHand::Render(std::vector<ovrDrawSurface>& surfaceList) {
    GlowColor = OVR::Vector3f(0.75f);

    if (IsInSystemGesture()) {
        // make it more blue if we are in the system gesture
        GlowColor.z = 1.0f;
    }

    ovrInputDeviceHandBase::Render(surfaceList);
}

OVR::Vector3f ovrInputDeviceTrackedHand::GetBonePosition(ovrHandBone bone) const {
    return HandModel.GetSkeleton().GetWorldSpacePoses()[bone].Translation;
}

ovrInputDeviceStandardPointer* ovrInputDeviceStandardPointer::Create(
    OVRFW::ovrAppl& app,
    const ovrInputStandardPointerCapabilities& capsHeader) {
    ALOG("VrInputStandard - ovrInputDeviceStandardPointer::Create");

    ovrInputStateStandardPointer inputState;
    inputState.Header.ControllerType = capsHeader.Header.Type;
    inputState.Header.TimeInSeconds = 0.0f;

    ovrResult result = vrapi_GetCurrentInputState(
        app.GetSessionObject(), capsHeader.Header.DeviceID, &inputState.Header);

    if (result == ovrSuccess) {
        ovrHandedness controllerHand =
            (capsHeader.ControllerCapabilities & ovrControllerCaps_LeftHand) != 0
            ? VRAPI_HAND_LEFT
            : VRAPI_HAND_RIGHT;
        return new ovrInputDeviceStandardPointer(capsHeader, controllerHand);
    } else {
        ALOG("VrInputStandard - vrapi_GetCurrentInputState: Error %i", result);
    }

    return nullptr;
}

bool ovrInputDeviceStandardPointer::Update(
    ovrMobile* ovr,
    const double displayTimeInSeconds,
    const float dt) {
    auto ret = ovrInputDeviceHandBase::Update(ovr, displayTimeInSeconds, dt);

    if (ret) {
        /// Pointer is at hand for controller
        PointerPose = GetHandPose();

        memset(&InputStateStandardPointer, 0, sizeof(InputStateStandardPointer));
        InputStateStandardPointer.Header.ControllerType = GetType();
        InputStateStandardPointer.Header.TimeInSeconds = displayTimeInSeconds;
        ovrResult r =
            vrapi_GetCurrentInputState(ovr, GetDeviceID(), &InputStateStandardPointer.Header);
        if (r == ovrSuccess) {
            HandPose = InputStateStandardPointer.GripPose;

            PointerPose = InputStateStandardPointer.PointerPose;
            UpdateHapticRequestedState(InputStateStandardPointer);
        } else {
            ALOG("Failed to read standard pointer state: %u", r);
        }
    }
    return ret;
}

void ovrInputDeviceStandardPointer::ResetHaptics(ovrMobile* ovr, float displayTimeInSeconds) {
    RequestedHapticState = {};
    UpdateHaptics(ovr, displayTimeInSeconds);
}

void ovrInputDeviceStandardPointer::UpdateHapticRequestedState(
    const ovrInputStateStandardPointer& inputState) {
    if (inputState.PointerStrength > kHapticsGripThreashold) {
        RequestedHapticState = {
            SampleConfiguration.OnTriggerHapticsState, inputState.PointerStrength};
    } else {
        RequestedHapticState = {};
    }
}

} // namespace OVRFW
