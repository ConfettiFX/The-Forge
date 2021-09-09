/************************************************************************************

Filename    :   TinyUI.cpp
Content     :   Componentized wrappers for GuiSys
Created     :   July 2020
Authors     :   Federico Schliemann
Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#include "TinyUI.h"

#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>

#include "GUI/GuiSys.h"
#include "GUI/DefaultComponent.h"
#include "GUI/ActionComponents.h"
#include "GUI/VRMenu.h"
#include "GUI/VRMenuObject.h"
#include "GUI/VRMenuMgr.h"
#include "GUI/Reflection.h"
#include "Render/DebugLines.h"

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

bool TinyUI::Init(const xrJava* context, OVRFW::ovrFileSys* FileSys) {
    const xrJava* java = context;

    /// Leftovers that aren't used
    auto SoundEffectPlayer = new OvrGuiSys::ovrDummySoundEffectPlayer();
    if (nullptr == SoundEffectPlayer) {
        ALOGE("Couldn't create SoundEffectPlayer");
        return false;
    }
    auto DebugLines = OvrDebugLines::Create();
    if (nullptr == DebugLines) {
        ALOGE("Couldn't create DebugLines");
        return false;
    }
    DebugLines->Init();

    /// Needed for FONTS
    Locale = ovrLocale::Create(*java->Env, java->ActivityObject, "default");
    if (nullptr == Locale) {
        ALOGE("Couldn't create Locale");
        return false;
    }
    std::string fontName;
    GetLocale().GetLocalizedString("@string/font_name", "efigs.fnt", fontName);

    GuiSys = OvrGuiSys::Create(context);
    if (nullptr == GuiSys) {
        ALOGE("Couldn't create GUI");
        return false;
    }
    GuiSys->Init(FileSys, *SoundEffectPlayer, fontName.c_str(), DebugLines);

    return true;
}

void TinyUI::Shutdown() {
    OvrGuiSys::Destroy(GuiSys);
}

void TinyUI::AddHitTestRay(const OVR::Posef& ray, bool isClicking) {
    OVRFW::TinyUI::HitTestDevice device;
    device.pointerStart = ray.Transform({0.0f, 0.0f, 0.0f});
    device.pointerEnd = ray.Transform({0.0f, 0.0f, -1.0f});
    device.clicked = isClicking;
    Devices.push_back(device);
}

void TinyUI::Update(const OVRFW::ovrApplFrameIn& in) {
    /// Hit test

    /// clear previous frame
    for (auto& device : PreviousFrameDevices) {
        if (device.hitObject) {
            device.hitObject->SetSurfaceColor(0, BackgroundColor);
        }
    }

    /// hit tesst
    for (auto& device : Devices) {
        Vector3f pointerStart = device.pointerStart;
        Vector3f pointerEnd = device.pointerEnd;
        Vector3f pointerDir = (pointerEnd - pointerStart).Normalized();
        Vector3f targetEnd = pointerStart + pointerDir * 10.0f;

        HitTestResult hit = GuiSys->TestRayIntersection(pointerStart, pointerDir);
        if (hit.HitHandle.IsValid()) {
            device.pointerEnd = pointerStart + hit.RayDir * hit.t - pointerDir * 0.025f;
            device.hitObject = GuiSys->GetVRMenuMgr().ToObject(hit.HitHandle);
            if (device.hitObject != nullptr) {
                /// we hit a menu, make sure it is a button with a registered handler
                auto it = ButtonHandlers.find(device.hitObject);
                if (it != ButtonHandlers.end()) {
                    /// hover highlight
                    device.hitObject->SetSurfaceColor(0, HoverColor);
                    pointerEnd = targetEnd;
                    if (device.clicked) {
                        // click highlight
                        device.hitObject->SetSurfaceColor(0, HighlightColor);
                        // run event handler
                        it->second();
                    }
                }
            }
        }
    }

    /// Save these for later
    PreviousFrameDevices = Devices;
}

void TinyUI::Render(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out) {
    const Matrix4f& traceMat = out.FrameMatrices.CenterView.Inverted();
    GuiSys->Frame(in, out.FrameMatrices.CenterView, traceMat);
    GuiSys->AppendSurfaceList(out.FrameMatrices.CenterView, &out.Surfaces);
}

OVRFW::VRMenuObject* TinyUI::CreateMenu(
    const std::string& labelText,
    const OVR::Vector3f& position,
    const OVR::Vector2f& size) {
    /// common naming
    static uint32_t menuIndex = 3000;
    menuIndex++;
    std::stringstream ss;
    ss << std::setprecision(4) << std::fixed;
    ss << "tinyui_menu_" << menuIndex << "_";
    std::string menuName = ss.str();
    VRMenu* m = SimpleTargetMenu::Create(*GuiSys, *Locale, menuName, labelText);
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
            /// remember everything
            AllElements.push_back(mo);
            return mo;
        }
    }
    return nullptr;
}

OVRFW::VRMenuObject* TinyUI::AddLabel(
    const std::string& labelText,
    const OVR::Vector3f& position,
    const OVR::Vector2f& size) {
    return CreateMenu(labelText, position, size);
}

OVRFW::VRMenuObject* TinyUI::AddButton(
    const std::string& label,
    const OVR::Vector3f& position,
    const OVR::Vector2f& size,
    const std::function<void(void)>& handler) {
    auto b = CreateMenu(label, position, size);
    if (b && handler) {
        ButtonHandlers[b] = handler;
    }
    return b;
}

OVRFW::VRMenuObject* TinyUI::AddToggleButton(
    const std::string& labelTextOn,
    const std::string& labelTextOff,
    bool* value,
    const OVR::Vector3f& position,
    const OVR::Vector2f& size,
    const std::function<void(void)>& postHandler) {
    auto b = CreateMenu("", position, size);
    const std::function<void(void)>& handler = [=]() {
        if (b) {
            *value = !(*value);
            b->SetText(*value ? labelTextOn.c_str() : labelTextOff.c_str());
            if (postHandler) {
                postHandler();
            }
        }
    };
    if (b && handler && value) {
        b->SetText(*value ? labelTextOn.c_str() : labelTextOff.c_str());
        ButtonHandlers[b] = handler;
    }
    return b;
}

OVRFW::VRMenuObject* TinyUI::AddSlider(
    const std::string& label,
    const OVR::Vector3f& position,
    float* value,
    const float defaultValue,
    const float delta) {
    VRMenuObject* lb = CreateMenu(label, position, {150.0f, 50.0f});
    VRMenuObject* lt = CreateMenu("-", position + Vector3f{0.20f, 0.0f, 0.0f}, {50.0f, 50.0f});
    VRMenuObject* val = CreateMenu("0.0", position + Vector3f{0.35f, 0.0f, 0.0f}, {100.0f, 50.0f});
    VRMenuObject* gt = CreateMenu("+", position + Vector3f{0.50f, 0.0f, 0.0f}, {50.0f, 50.0f});

    auto udpateText = [=]() {
        std::stringstream ss;
        ss << std::setprecision(4) << std::fixed;
        ss << *value;
        val->SetText(ss.str().c_str());
    };
    ButtonHandlers[lt] = [=]() {
        *value -= delta;
        udpateText();
    };
    ButtonHandlers[gt] = [=]() {
        *value += delta;
        udpateText();
    };
    ButtonHandlers[val] = [=]() {
        *value = defaultValue;
        udpateText();
    };
    ButtonHandlers[lb] = [=]() { udpateText(); };
    udpateText();
    return lb;
}

void TinyUI::ShowAll() {
    ForAll([](VRMenuObject* menu) { menu->SetVisible(true); });
}
void TinyUI::HideAll(const std::vector<VRMenuObject*>& exceptions) {
    ForAll([=](VRMenuObject* menu) {
        for (const VRMenuObject* e : exceptions) {
            if (e == menu)
                return;
        }
        menu->SetVisible(false);
    });
}

void TinyUI::ForAll(const std::function<void(VRMenuObject*)>& handler) {
    if (handler) {
        for (VRMenuObject* menu : AllElements) {
            if (menu) {
                handler(menu);
            }
        }
    }
}

} // namespace OVRFW
