/************************************************************************************

Filename    :   ReflectionData.cpp
Content     :   Data for introspection and reflection of C++ objects.
Created     :   11/16/2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

// this is necessary so that offsetof() can work for private class members
#define _ALLOW_KEYWORD_MACROS
#undef private
#define private public

#include "ReflectionData.h"
#include "VRMenuObject.h"
#include "DefaultComponent.h"
#include "AnimComponents.h"

using OVR::BitFlagsT;
using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::TypesafeNumberT;
using OVR::Vector2f;
using OVR::Vector2i;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

ovrEnumInfo HorizontalJustification_Enums[] = {
    {"HORIZONTAL_LEFT", 0},
    {"HORIZONTAL_CENTER", 1},
    {"HORIZONTAL_RIGHT", 2}};
OVR_VERIFY_ARRAY_SIZE(HorizontalJustification_Enums, 3);

ovrEnumInfo VerticalJustification_Enums[] = {
    {"VERTICAL_BASELINE", 0},
    {"VERTICAL_CENTER", 1},
    {"VERTICAL_CENTER_FIXEDHEIGHT", 2},
    {"VERTICAL_TOP", 3}};
OVR_VERIFY_ARRAY_SIZE(VerticalJustification_Enums, 4);

ovrEnumInfo eContentFlags_Enums[] = {
    {"CONTENT_NONE", 0},
    {"CONTENT_SOLID", 1},
    {"CONTENT_ALL", 0x7fffffff},
};
OVR_VERIFY_ARRAY_SIZE(eContentFlags_Enums, 3);

ovrEnumInfo VRMenuObjectType_Enums[] = {
    {"VRMENU_CONTAINER", 0},
    {"VRMENU_STATIC", 1},
    {"VRMENU_BUTTON", 2},
    {"VRMENU_MAX", 3}};
OVR_VERIFY_ARRAY_SIZE(VRMenuObjectType_Enums, VRMENU_MAX + 1);

ovrEnumInfo VRMenuObjectFlag_Enums[] = {
    {"VRMENUOBJECT_FLAG_NO_FOCUS_GAINED", 0},
    {"VRMENUOBJECT_DONT_HIT_ALL", 1},
    {"VRMENUOBJECT_DONT_HIT_TEXT", 2},
    {"VRMENUOBJECT_HIT_ONLY_BOUNDS", 3},
    {"VRMENUOBJECT_BOUND_ALL", 4},
    {"VRMENUOBJECT_FLAG_POLYGON_OFFSET", 5},
    {"VRMENUOBJECT_FLAG_NO_DEPTH", 6},
    {"VRMENUOBJECT_FLAG_NO_DEPTH_MASK", 7},
    {"VRMENUOBJECT_DONT_RENDER", 8},
    {"VRMENUOBJECT_DONT_RENDER_SURFACE", 9},
    {"VRMENUOBJECT_DONT_RENDER_TEXT", 10},
    {"VRMENUOBJECT_NO_GAZE_HILIGHT", 11},
    {"VRMENUOBJECT_RENDER_HIERARCHY_ORDER", 12},
    {"VRMENUOBJECT_FLAG_BILLBOARD", 13},
    {"VRMENUOBJECT_DONT_MOD_PARENT_COLOR", 14},
    {"VRMENUOBJECT_INSTANCE_TEXT", 15}};
OVR_VERIFY_ARRAY_SIZE(VRMenuObjectFlag_Enums, VRMENUOBJECT_MAX);

ovrEnumInfo VRMenuObjectInitFlag_Enums[] = {
    {"VRMENUOBJECT_INIT_ALIGN_TO_VIEW", 0},
    {"VRMENUOBJECT_INIT_FORCE_POSITION", 1}};
OVR_VERIFY_ARRAY_SIZE(VRMenuObjectInitFlag_Enums, 2);

ovrEnumInfo SurfaceTextureType_Enums[] = {
    {"SURFACE_TEXTURE_DIFFUSE", 0},
    {"SURFACE_TEXTURE_DIFFUSE_ALPHA_DISCARD", 1},
    {"SURFACE_TEXTURE_ADDITIVE", 2},
    {"SURFACE_TEXTURE_COLOR_RAMP", 3},
    {"SURFACE_TEXTURE_COLOR_RAMP_TARGET", 4},
    {"SURFACE_TEXTURE_ALPHA_MASK", 5},
    {"SURFACE_TEXTURE_MAX", 6}};
OVR_VERIFY_ARRAY_SIZE(SurfaceTextureType_Enums, SURFACE_TEXTURE_MAX + 1);

ovrEnumInfo VRMenuId_Enums[] = {{"INVALID_MENU_ID", INT_MIN}};

ovrEnumInfo VRMenuEventType_Enums[] = {
    {"VRMENU_EVENT_FOCUS_GAINED", 0},
    {"VRMENU_EVENT_FOCUS_LOST", 1},
    {"VRMENU_EVENT_TOUCH_DOWN", 2},
    {"VRMENU_EVENT_TOUCH_UP", 3},
    {"VRMENU_EVENT_TOUCH_RELATIVE", 4},
    {"VRMENU_EVENT_TOUCH_ABSOLUTE", 5},
    {"VRMENU_EVENT_SWIPE_FORWARD", 6},
    {"VRMENU_EVENT_SWIPE_BACK", 7},
    {"VRMENU_EVENT_SWIPE_UP", 8},
    {"VRMENU_EVENT_SWIPE_DOWN", 9},
    {"VRMENU_EVENT_FRAME_UPDATE", 10},
    {"VRMENU_EVENT_SUBMIT_FOR_RENDERING", 11},
    {"VRMENU_EVENT_RENDER", 12},
    {"VRMENU_EVENT_OPENING", 13},
    {"VRMENU_EVENT_OPENED", 14},
    {"VRMENU_EVENT_CLOSING", 15},
    {"VRMENU_EVENT_CLOSED", 16},
    {"VRMENU_EVENT_INIT", 17},
    {"VRMENU_EVENT_SELECTED", 18},
    {"VRMENU_EVENT_DESELECTED", 19},
    {"VRMENU_EVENT_UPDATE_OBJECT", 20},
    {"VRMENU_EVENT_SWIPE_COMPLETE", 21},
    {"VRMENU_EVENT_ITEM_ACTION_COMPLETE", 22},
    {"VRMENU_EVENT_MAX", 23}};
OVR_VERIFY_ARRAY_SIZE(VRMenuEventType_Enums, VRMENU_EVENT_MAX + 1);

ovrEnumInfo AnimState_Enums[] = {{"ANIMSTATE_PAUSED", 0}, {"ANIMSTATE_PLAYING", 1}};
OVR_VERIFY_ARRAY_SIZE(AnimState_Enums, OvrAnimComponent::ANIMSTATE_MAX);

ovrEnumInfo EventDispatchType_Enums[] = {
    {"EVENT_DISPATCH_TARGET", 0},
    {"EVENT_DISPATCH_FOCUS", 1},
    {"EVENT_DISPATCH_BROADCAST", 2}};
OVR_VERIFY_ARRAY_SIZE(EventDispatchType_Enums, EVENT_DISPATCH_MAX);

template <typename T>
T* CreateObject(void* placementBuffer) {
    if (placementBuffer != NULL) {
        return new (placementBuffer) T();
    }
    return new T();
}

void* Create_OvrDefaultComponent(void* placementBuffer) {
    return CreateObject<OvrDefaultComponent>(placementBuffer);
}

void* Create_OvrSurfaceAnimComponent(void* placementBuffer) {
    return OvrSurfaceAnimComponent::Create(placementBuffer);
}

void* Create_VRMenuSurfaceParms(void* placementBuffer) {
    return CreateObject<VRMenuSurfaceParms>(placementBuffer);
}

void* Create_VRMenuObjectParms(void* placementBuffer) {
    return CreateObject<VRMenuObjectParms>(placementBuffer);
}

void* Create_ovrSoundLimiter(void* placementBuffer) {
    return CreateObject<ovrSoundLimiter>(placementBuffer);
}

void* Create_string(void* placementBuffer) {
    return CreateObject<std::string>(placementBuffer);
}

void* Create_bool(void* placementBuffer) {
    return CreateObject<bool>(placementBuffer);
}

void* Create_float(void* placementBuffer) {
    return CreateObject<float>(placementBuffer);
}

void* Create_double(void* placementBuffer) {
    return CreateObject<double>(placementBuffer);
}

void* Create_int(void* placementBuffer) {
    return CreateObject<int>(placementBuffer);
}

template <class ItemClass>
void ResizeArray(void* arrayPtr, const int newSize) {
    std::vector<ItemClass>& a = *static_cast<std::vector<ItemClass>*>(arrayPtr);
    a.resize(newSize);
}

template <class ItemClass>
void ResizePtrArray(void* arrayPtr, const int newSize) {
    std::vector<ItemClass>& a = *static_cast<std::vector<ItemClass>*>(arrayPtr);
    int oldSize = static_cast<int>(a.size());
    a.resize(newSize);

    if (newSize > oldSize) {
        // in-place construct since OVR::Array doesn't
        for (int i = oldSize; i < newSize; ++i) {
            ItemClass* item = static_cast<ItemClass*>(&a[i]);
            new (item) ItemClass();
        }
    }
}

template <class ArrayClass, class ElementClass>
void SetArrayElementPtr(void* objPtr, const int index, ElementClass elementPtr) {
    ArrayClass& a = *static_cast<ArrayClass*>(objPtr);
    a[index] = elementPtr;
}

template <class ArrayClass, class ElementClass>
void SetArrayElement(void* objPtr, const int index, ElementClass* elementPtr) {
    ArrayClass& a = *static_cast<ArrayClass*>(objPtr);
    a[index] = *elementPtr;
}

void Resize_std_vector_VRMenuComponent_Ptr(void* objPtr, const int newSize) {
    ResizePtrArray<VRMenuComponent*>(objPtr, newSize);
}

void SetArrayElementFn_std_vector_VRMenuComponent_Ptr(
    void* objPtr,
    const int index,
    void* elementPtr) {
    SetArrayElementPtr<std::vector<VRMenuComponent*>, VRMenuComponent*>(
        objPtr, index, static_cast<VRMenuComponent*>(elementPtr));
}

void Resize_std_vector_VRMenuSurfaceParms(void* objPtr, const int newSize) {
    ResizeArray<VRMenuSurfaceParms>(objPtr, newSize);
}

void Resize_std_vector_VRMenuObjectParms_Ptr(void* objPtr, const int newSize) {
    ResizePtrArray<VRMenuObjectParms*>(objPtr, newSize);
}

void SetArrayElementFn_std_vector_VRMenuObjectParms_Ptr(
    void* objPtr,
    const int index,
    void* elementPtr) {
    SetArrayElementPtr<std::vector<VRMenuObjectParms*>, VRMenuObjectParms*>(
        objPtr, index, static_cast<VRMenuObjectParms*>(elementPtr));
}

void SetArrayElementFn_std_vector_VRMenuSurfaceParms(
    void* objPtr,
    const int index,
    void* elementPtr) {
    SetArrayElement<std::vector<VRMenuSurfaceParms>, VRMenuSurfaceParms>(
        objPtr, index, static_cast<VRMenuSurfaceParms*>(elementPtr));
}

void SetArrayElementFn_string(void* objPtr, const int index, void* elementPtr) {
    static_cast<std::string*>(objPtr)[index] = *static_cast<std::string*>(elementPtr);
}

void SetArrayElementFn_int(void* objPtr, const int index, void* elementPtr) {
    static_cast<int*>(objPtr)[index] = *static_cast<int*>(elementPtr);
}

void SetArrayElementFn_GLuint(void* objPtr, const int index, void* elementPtr) {
    static_cast<GLuint*>(objPtr)[index] = *static_cast<GLuint*>(elementPtr);
}

void SetArrayElementFn_eSurfaceTextureType(void* objPtr, const int index, void* elementPtr) {
    static_cast<eSurfaceTextureType*>(objPtr)[index] =
        *static_cast<eSurfaceTextureType*>(elementPtr);
}

//=============================================================================================
// Reflection Data
//=============================================================================================

ovrMemberInfo Vector2i_Reflection[] = {
    {"x", "int", NULL, ovrTypeOperator::NONE, offsetof(Vector2i, x), 0},
    {"y", "int", NULL, ovrTypeOperator::NONE, offsetof(Vector2i, y), 0},
    {}};

ovrMemberInfo Vector2f_Reflection[] = {
    {"x", "float", NULL, ovrTypeOperator::NONE, offsetof(Vector2f, x), 0},
    {"y", "float", NULL, ovrTypeOperator::NONE, offsetof(Vector2f, y), 0},
    {}};

ovrMemberInfo Vector3f_Reflection[] = {
    {"x", "float", NULL, ovrTypeOperator::NONE, offsetof(Vector3f, x), 0},
    {"y", "float", NULL, ovrTypeOperator::NONE, offsetof(Vector3f, y), 0},
    {"z", "float", NULL, ovrTypeOperator::NONE, offsetof(Vector3f, z), 0},
    {}};

ovrMemberInfo Vector4f_Reflection[] = {
    {"x", "float", NULL, ovrTypeOperator::NONE, offsetof(Vector4f, x), 0},
    {"y", "float", NULL, ovrTypeOperator::NONE, offsetof(Vector4f, y), 0},
    {"z", "float", NULL, ovrTypeOperator::NONE, offsetof(Vector4f, z), 0},
    {"w", "float", NULL, ovrTypeOperator::NONE, offsetof(Vector4f, w), 0},
    {}};

ovrMemberInfo Quatf_Reflection[] = {
    {"x", "float", NULL, ovrTypeOperator::NONE, offsetof(Quatf, x), 0},
    {"y", "float", NULL, ovrTypeOperator::NONE, offsetof(Quatf, y), 0},
    {"z", "float", NULL, ovrTypeOperator::NONE, offsetof(Quatf, z), 0},
    {"w", "float", NULL, ovrTypeOperator::NONE, offsetof(Quatf, w), 0},
    {}};

ovrMemberInfo TypesafeNumberT_longlong_eVRMenuId_INVALID_MENU_ID[] = {
    {"Value", "long long", NULL, ovrTypeOperator::NONE, offsetof(VRMenuId_t, Value), 0},
    {}};

ovrMemberInfo Posef_Reflection[] = {
    {"Position", "Vector3f", NULL, ovrTypeOperator::NONE, offsetof(Posef, Translation), 0},
    {"Orientation", "Quatf", NULL, ovrTypeOperator::NONE, offsetof(Posef, Rotation), 0},
    {}};

ovrMemberInfo VRMenuSurfaceParms_Reflection[] = {
    {"SurfaceName",
     "string",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(VRMenuSurfaceParms, SurfaceName),
     0},
    {"ImageNames",
     "string[]",
     NULL,
     ovrTypeOperator::ARRAY,
     offsetof(VRMenuSurfaceParms, ImageNames),
     VRMENUSURFACE_IMAGE_MAX},
    {"ImageTexId",
     "GLuint[]",
     NULL,
     ovrTypeOperator::ARRAY,
     offsetof(VRMenuSurfaceParms, ImageTexId),
     VRMENUSURFACE_IMAGE_MAX},
    {"ImageWidth",
     "int[]",
     NULL,
     ovrTypeOperator::ARRAY,
     offsetof(VRMenuSurfaceParms, ImageWidth),
     VRMENUSURFACE_IMAGE_MAX},
    {"ImageHeight",
     "int[]",
     NULL,
     ovrTypeOperator::ARRAY,
     offsetof(VRMenuSurfaceParms, ImageHeight),
     VRMENUSURFACE_IMAGE_MAX},
    {"TextureTypes",
     "eSurfaceTextureType[]",
     NULL,
     ovrTypeOperator::ARRAY,
     offsetof(VRMenuSurfaceParms, TextureTypes),
     VRMENUSURFACE_IMAGE_MAX},
    {"Contents",
     "BitFlagsT< eContentFlags >",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(VRMenuSurfaceParms, Contents),
     0},
    {"Color", "Vector4f", NULL, ovrTypeOperator::NONE, offsetof(VRMenuSurfaceParms, Color), 0},
    {"Anchors", "Vector2f", NULL, ovrTypeOperator::NONE, offsetof(VRMenuSurfaceParms, Anchors), 0},
    {"Border", "Vector4f", NULL, ovrTypeOperator::NONE, offsetof(VRMenuSurfaceParms, Border), 0},
    {"Dims", "Vector2f", NULL, ovrTypeOperator::NONE, offsetof(VRMenuSurfaceParms, Dims), 0},
    {"CropUV", "Vector4f", NULL, ovrTypeOperator::NONE, offsetof(VRMenuSurfaceParms, CropUV), 0},
    {"OffsetUVs",
     "Vector2f",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(VRMenuSurfaceParms, OffsetUVs),
     0},
    {}};

ovrMemberInfo VRMenuFontParms_Reflection[] = {
    {"AlignHoriz",
     "HorizontalJustification",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(VRMenuFontParms, AlignHoriz),
     0},
    {"AlignVert",
     "VerticalJustification",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(VRMenuFontParms, AlignVert),
     0},
    {"Billboard", "bool", NULL, ovrTypeOperator::NONE, offsetof(VRMenuFontParms, Billboard), 0},
    {"TrackRoll", "bool", NULL, ovrTypeOperator::NONE, offsetof(VRMenuFontParms, TrackRoll), 0},
    {"Outline", "bool", NULL, ovrTypeOperator::NONE, offsetof(VRMenuFontParms, Outline), 0},
    {"ColorCenter", "float", NULL, ovrTypeOperator::NONE, offsetof(VRMenuFontParms, ColorCenter)},
    {"AlphaCenter", "float", NULL, ovrTypeOperator::NONE, offsetof(VRMenuFontParms, AlphaCenter)},
    {"Scale", "float", NULL, ovrTypeOperator::NONE, offsetof(VRMenuFontParms, Scale), 0},
    {"WrapWidth", "float", NULL, ovrTypeOperator::NONE, offsetof(VRMenuFontParms, WrapWidth), 0},
    {"MaxLines", "int", NULL, ovrTypeOperator::NONE, offsetof(VRMenuFontParms, MaxLines), 0},
    {"MultiLine", "bool", NULL, ovrTypeOperator::NONE, offsetof(VRMenuFontParms, MultiLine), 0},
    {}};

ovrMemberInfo VRMenuObjectParms_Reflection[] = {
    {"Type",
     "eVRMenuObjectType",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(VRMenuObjectParms, Type),
     0},
    {"Flags",
     "BitFlagsT< eVRMenuObjectFlags >",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(VRMenuObjectParms, Flags),
     0},
    {"InitFlags",
     "BitFlagsT< eVRMenuObjectInitFlags >",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(VRMenuObjectParms, InitFlags),
     0},
    {"Components",
     "std::vector< VRMenuComponent* >",
     NULL,
     ovrTypeOperator::ARRAY,
     offsetof(VRMenuObjectParms, Components),
     0},
    {"SurfaceParms",
     "std::vector< VRMenuSurfaceParms >",
     NULL,
     ovrTypeOperator::ARRAY,
     offsetof(VRMenuObjectParms, SurfaceParms),
     0},
    {"Text", "string", NULL, ovrTypeOperator::NONE, offsetof(VRMenuObjectParms, Text), 0},
    {"LocalPose", "Posef", NULL, ovrTypeOperator::NONE, offsetof(VRMenuObjectParms, LocalPose), 0},
    {"LocalScale",
     "Vector3f",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(VRMenuObjectParms, LocalScale),
     0},
    {"TextLocalPose",
     "Posef",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(VRMenuObjectParms, TextLocalPose),
     0},
    {"TextLocalScale",
     "Vector3f",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(VRMenuObjectParms, TextLocalScale),
     0},
    {"FontParms",
     "VRMenuFontParms",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(VRMenuObjectParms, FontParms),
     0},
    {"Color", "Vector4f", NULL, ovrTypeOperator::NONE, offsetof(VRMenuObjectParms, Color), 0},
    {"TextColor",
     "Vector4f",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(VRMenuObjectParms, TextColor),
     0},
    {"Id",
     "TypesafeNumberT< long long, eVRMenuId, INVALID_MENU_ID >",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(VRMenuObjectParms, Id),
     0},
    {"ParentId",
     "TypesafeNumberT< long long, eVRMenuId, INVALID_MENU_ID >",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(VRMenuObjectParms, ParentId),
     0},
    {"Contents",
     "BitFlagsT< eContentFlags >",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(VRMenuObjectParms, Contents),
     0},
    {"Name", "string", NULL, ovrTypeOperator::NONE, offsetof(VRMenuObjectParms, Name), 0},
    {"ParentName",
     "string",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(VRMenuObjectParms, ParentName),
     0},
    {"Tag", "string", NULL, ovrTypeOperator::NONE, offsetof(VRMenuObjectParms, Tag), 0},
    {"TexelCoords",
     "bool",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(VRMenuObjectParms, TexelCoords),
     0},
    {"Selected", "bool", NULL, ovrTypeOperator::NONE, offsetof(VRMenuObjectParms, Selected), 0},
    {}};

ovrMemberInfo ovrSoundLimiter_Reflection[] = {
    {"LastPlayTime",
     "double",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(ovrSoundLimiter, LastPlayTime)},
    {}};

ovrMemberInfo Fader_Reflection[] = {
    {"FadeState", "eFadeState", NULL, ovrTypeOperator::NONE, offsetof(Fader, FadeState)},
    {"PrePauseState", "eFadeState", NULL, ovrTypeOperator::NONE, offsetof(Fader, PrePauseState)},
    {"StartAlpha", "float", NULL, ovrTypeOperator::NONE, offsetof(Fader, StartAlpha)},
    {"FadeAlpha", "float", NULL, ovrTypeOperator::NONE, offsetof(Fader, FadeAlpha)},
    {}};

ovrMemberInfo SineFader_Reflection[] = {{}};

ovrMemberInfo VRMenuComponent_Reflection[] = {
    {"EventFlags",
     "BitFlagsT< eVRMenuEventType, uint64_t >",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(VRMenuComponent, EventFlags)},
    {"Name", "string", NULL, ovrTypeOperator::NONE, offsetof(VRMenuComponent, Name)},
    {}};

ovrMemberInfo OvrDefaultComponent_Reflection[] = {
    {"GazeOverSoundLimiter",
     "ovrSoundLimiter",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(OvrDefaultComponent, GazeOverSoundLimiter)},
    {"DownSoundLimiter",
     "ovrSoundLimiter",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(OvrDefaultComponent, DownSoundLimiter)},
    {"UpSoundLimiter",
     "ovrSoundLimiter",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(OvrDefaultComponent, UpSoundLimiter)},
    {"HilightFader",
     "SineFader",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(OvrDefaultComponent, HilightFader)},
    {"StartFadeInTime",
     "double",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(OvrDefaultComponent, StartFadeInTime)},
    {"StartFadeOutTime",
     "double",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(OvrDefaultComponent, StartFadeOutTime)},
    {"HilightOffset",
     "Vector3f",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(OvrDefaultComponent, HilightOffset)},
    {"HilightScale",
     "float",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(OvrDefaultComponent, HilightScale)},
    {"FadeDuration",
     "float",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(OvrDefaultComponent, FadeDuration)},
    {"FadeDelay", "float", NULL, ovrTypeOperator::NONE, offsetof(OvrDefaultComponent, FadeDelay)},
    {"TextNormalColor",
     "Vector4f",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(OvrDefaultComponent, TextNormalColor)},
    {"TextHilightColor",
     "Vector4f",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(OvrDefaultComponent, TextHilightColor)},
    {"SuppressText",
     "bool",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(OvrDefaultComponent, SuppressText)},
    {"UseSurfaceAnimator",
     "bool",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(OvrDefaultComponent, UseSurfaceAnimator)},
    {"NoHilight", "bool", NULL, ovrTypeOperator::NONE, offsetof(OvrDefaultComponent, NoHilight)},
    {}};

ovrMemberInfo OvrAnimComponent_Reflection[] = {
    {"BaseTime", "double", NULL, ovrTypeOperator::NONE, offsetof(OvrAnimComponent, BaseTime)},
    {"BaseFrame", "int", NULL, ovrTypeOperator::NONE, offsetof(OvrAnimComponent, BaseFrame)},
    {"CurFrame", "int", NULL, ovrTypeOperator::NONE, offsetof(OvrAnimComponent, CurFrame)},
    {"FramesPerSecond",
     "float",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(OvrAnimComponent, FramesPerSecond)},
    {"AnimState",
     "OvrAnimComponent::eAnimState",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(OvrAnimComponent, AnimState)},
    {"Looping", "bool", NULL, ovrTypeOperator::NONE, offsetof(OvrAnimComponent, Looping)},
    {"ForceVisibilityUpdate",
     "bool",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(OvrAnimComponent, ForceVisibilityUpdate)},
    {"FractionalFrame",
     "float",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(OvrAnimComponent, FractionalFrame)},
    {"FloatFrame", "double", NULL, ovrTypeOperator::NONE, offsetof(OvrAnimComponent, FloatFrame)},
    {}};

ovrMemberInfo OvrSurfaceAnimComponent_Reflection[] = {
    {"SurfacesPerFrame",
     "int",
     NULL,
     ovrTypeOperator::NONE,
     offsetof(OvrSurfaceAnimComponent, SurfacesPerFrame)},
    {}};

ovrTypeInfo TypeInfoList[] = {
    {"bool",
     NULL,
     sizeof(bool),
     NULL,
     ParseBool,
     Create_bool,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     NULL},
    {"float",
     NULL,
     sizeof(float),
     NULL,
     ParseFloat,
     Create_float,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     NULL},
    {"double",
     NULL,
     sizeof(double),
     NULL,
     ParseDouble,
     Create_double,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     NULL},
    {"int",
     NULL,
     sizeof(int),
     NULL,
     ParseInt,
     Create_int,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     NULL},
    {"GLuint",
     NULL,
     sizeof(GLuint),
     NULL,
     ParseInt,
     Create_int,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     NULL},

    {"eVRMenuObjectType",
     NULL,
     sizeof(eVRMenuObjectType),
     VRMenuObjectType_Enums,
     ParseEnum,
     Create_int,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     NULL},
    {"eVRMenuObjectFlags",
     NULL,
     sizeof(eVRMenuObjectFlags),
     VRMenuObjectFlag_Enums,
     ParseEnum,
     Create_int,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     NULL},
    {"eVRMenuObjectInitFlags",
     NULL,
     sizeof(eVRMenuObjectInitFlags),
     VRMenuObjectInitFlag_Enums,
     ParseEnum,
     Create_int,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     NULL},
    {"eVRMenuId",
     NULL,
     sizeof(eVRMenuId),
     VRMenuId_Enums,
     ParseEnum,
     Create_int,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     NULL},
    {"eSurfaceTextureType",
     NULL,
     sizeof(eSurfaceTextureType),
     SurfaceTextureType_Enums,
     ParseEnum,
     Create_int,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     NULL},
    {"HorizontalJustification",
     NULL,
     sizeof(HorizontalJustification),
     HorizontalJustification_Enums,
     ParseEnum,
     Create_int,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     NULL},
    {"VerticalJustification",
     NULL,
     sizeof(VerticalJustification),
     VerticalJustification_Enums,
     ParseEnum,
     Create_int,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     NULL},
    {"eContentFlags",
     NULL,
     sizeof(eContentFlags),
     eContentFlags_Enums,
     ParseEnum,
     Create_int,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     NULL},
    {"eVRMenuEventType",
     NULL,
     sizeof(eVRMenuEventType),
     VRMenuEventType_Enums,
     ParseEnum,
     Create_int,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     NULL},
    {"OvrAnimComponent::eAnimState",
     NULL,
     sizeof(OvrAnimComponent::eAnimState),
     AnimState_Enums,
     ParseEnum,
     Create_int,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     NULL},

    {"BitFlagsT< eVRMenuObjectFlags >",
     NULL,
     sizeof(BitFlagsT<eVRMenuObjectFlags>),
     VRMenuObjectFlag_Enums,
     ParseBitFlags,
     NULL,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     NULL},
    {"BitFlagsT< eVRMenuObjectInitFlags >",
     NULL,
     sizeof(BitFlagsT<eVRMenuObjectInitFlags>),
     VRMenuObjectInitFlag_Enums,
     ParseBitFlags,
     NULL,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     NULL},
    {"BitFlagsT< eContentFlags >",
     NULL,
     sizeof(BitFlagsT<eContentFlags>),
     eContentFlags_Enums,
     ParseBitFlags,
     NULL,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     NULL},
    {"BitFlagsT< eVRMenuEventType, uint64_t >",
     NULL,
     sizeof(BitFlagsT<eVRMenuEventType, uint64_t>),
     VRMenuEventType_Enums,
     ParseBitFlags,
     NULL,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     NULL},

    {"string",
     NULL,
     sizeof(std::string),
     NULL,
     ParseString,
     Create_string,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     NULL},
    {"Vector2i",
     NULL,
     sizeof(Vector2i),
     NULL,
     ParseIntVector,
     NULL,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     Vector2i_Reflection},
    {"Vector2f",
     NULL,
     sizeof(Vector2f),
     NULL,
     ParseFloatVector,
     NULL,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     Vector2f_Reflection},
    {"Vector3f",
     NULL,
     sizeof(Vector3f),
     NULL,
     ParseFloatVector,
     NULL,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     Vector3f_Reflection},
    {"Vector4f",
     NULL,
     sizeof(Vector4f),
     NULL,
     ParseFloatVector,
     NULL,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     Vector4f_Reflection},
    {"Quatf",
     NULL,
     sizeof(Quatf),
     NULL,
     ParseFloatVector,
     NULL,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     Quatf_Reflection},
    {"Posef",
     NULL,
     sizeof(Posef),
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     Posef_Reflection},

    {"TypesafeNumberT< long long, eVRMenuId, INVALID_MENU_ID >",
     NULL,
     sizeof(TypesafeNumberT<long long, eVRMenuId, INVALID_MENU_ID>),
     NULL,
     ParseTypesafeNumber_long_long,
     NULL,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     TypesafeNumberT_longlong_eVRMenuId_INVALID_MENU_ID},

    {"std::vector< VRMenuComponent* >",
     NULL,
     sizeof(std::vector<VRMenuComponent*>),
     NULL,
     ParseArray,
     NULL,
     Resize_std_vector_VRMenuComponent_Ptr,
     SetArrayElementFn_std_vector_VRMenuComponent_Ptr,
     ovrArrayType::OVR_POINTER,
     false,
     NULL},
    {"std::vector< VRMenuSurfaceParms >",
     NULL,
     sizeof(std::vector<VRMenuSurfaceParms>),
     NULL,
     ParseArray,
     NULL,
     Resize_std_vector_VRMenuSurfaceParms,
     SetArrayElementFn_std_vector_VRMenuSurfaceParms,
     ovrArrayType::OVR_OBJECT,
     false,
     NULL},
    {"std::vector< VRMenuObjectParms* >",
     NULL,
     sizeof(std::vector<VRMenuObjectParms*>),
     NULL,
     ParseArray,
     NULL,
     Resize_std_vector_VRMenuObjectParms_Ptr,
     SetArrayElementFn_std_vector_VRMenuObjectParms_Ptr,
     ovrArrayType::OVR_POINTER,
     false,
     NULL},

    {"string[]",
     NULL,
     sizeof(std::string),
     NULL,
     ParseArray,
     NULL,
     NULL,
     SetArrayElementFn_string,
     ovrArrayType::C_OBJECT,
     false,
     NULL},
    {"int[]",
     NULL,
     sizeof(int),
     NULL,
     ParseArray,
     NULL,
     NULL,
     SetArrayElementFn_int,
     ovrArrayType::C_OBJECT,
     false,
     NULL},
    {"GLuint[]",
     NULL,
     sizeof(GLuint),
     NULL,
     ParseArray,
     NULL,
     NULL,
     SetArrayElementFn_GLuint,
     ovrArrayType::C_OBJECT,
     false,
     NULL},
    {"eSurfaceTextureType[]",
     NULL,
     sizeof(eSurfaceTextureType),
     NULL,
     ParseArray,
     NULL,
     NULL,
     SetArrayElementFn_eSurfaceTextureType,
     ovrArrayType::C_OBJECT,
     false,
     NULL},

    {"VRMenuSurfaceParms",
     NULL,
     sizeof(VRMenuSurfaceParms),
     NULL,
     NULL,
     Create_VRMenuSurfaceParms,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     VRMenuSurfaceParms_Reflection},
    {"VRMenuFontParms",
     NULL,
     sizeof(VRMenuFontParms),
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     VRMenuFontParms_Reflection},
    {"VRMenuObjectParms",
     NULL,
     sizeof(VRMenuObjectParms),
     NULL,
     NULL,
     Create_VRMenuObjectParms,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     VRMenuObjectParms_Reflection},

    {"Fader",
     NULL,
     sizeof(Fader),
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     Fader_Reflection},
    {"SineFader",
     "Fader",
     sizeof(SineFader),
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     SineFader_Reflection},
    {"ovrSoundLimiter",
     NULL,
     sizeof(ovrSoundLimiter),
     NULL,
     NULL,
     Create_ovrSoundLimiter,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     ovrSoundLimiter_Reflection},
    {"VRMenuComponent",
     NULL,
     sizeof(VRMenuComponent),
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     ovrArrayType::NONE,
     true,
     VRMenuComponent_Reflection},
    {"OvrDefaultComponent",
     "VRMenuComponent",
     sizeof(OvrDefaultComponent),
     NULL,
     NULL,
     Create_OvrDefaultComponent,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     OvrDefaultComponent_Reflection},
    {"OvrAnimComponent",
     "VRMenuComponent",
     sizeof(OvrAnimComponent),
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     ovrArrayType::NONE,
     true,
     OvrAnimComponent_Reflection},
    {"OvrSurfaceAnimComponent",
     "OvrAnimComponent",
     sizeof(OvrSurfaceAnimComponent),
     NULL,
     NULL,
     Create_OvrSurfaceAnimComponent,
     NULL,
     NULL,
     ovrArrayType::NONE,
     false,
     OvrSurfaceAnimComponent_Reflection},

    {}};

} // namespace OVRFW
