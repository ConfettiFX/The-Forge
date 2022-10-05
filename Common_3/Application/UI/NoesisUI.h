#pragma once

#include "../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"

struct NoesisView;
struct NoesisXaml;
struct NoesisTexture;
struct NoesisRenderTarget;
struct Renderer;
struct ReloadDesc;
struct RenderTarget;
struct Texture;
struct Cmd;
struct InputActionContext;
struct Queue;

FORGE_API void initNoesisUI(IApp::Settings const* pSettings, Renderer* pRenderer, uint32_t maxDrawBatches, Queue* pGraphicsQueue);
FORGE_API void exitNoesisUI();
FORGE_API void setNoesisUIResources(char const* appResourcePath, uint32_t fallbackFontCount, char const** fallbackFontPaths);
FORGE_API void loadNoesisUI(NoesisView* view, ReloadDesc const* pReloadDesc, RenderTarget* pRenderTarget);
FORGE_API void unloadNoesisUI(NoesisView* view, ReloadDesc const* pReloadDesc);
FORGE_API void updateNoesisUI(NoesisView* view, float deltaTime);
FORGE_API void drawNoesisUI(NoesisView* view, Cmd* pCmd, uint32_t frameIndex, RenderTarget* pRenderTarget);
FORGE_API void inputNoesisUI(NoesisView* view, InputActionContext const* ctx);
FORGE_API NoesisXaml* addNoesisUIXaml(const char* xamlName);
FORGE_API void removeNoesisUIXaml(NoesisXaml* xaml);
FORGE_API NoesisView* addNoesisUIView(NoesisXaml* xaml);
FORGE_API void removeNoesisUIView(NoesisView* view);
FORGE_API NoesisTexture* addNoesisUITexture(Texture* pTexture, TinyImageFormat format);
FORGE_API void removeNoesisUITexture(NoesisTexture* texture);
FORGE_API NoesisRenderTarget* addNoesisUIRenderTarget(RenderTarget* pTarget, Renderer* pRenderer, TinyImageFormat format);
FORGE_API void removeNoesisUIRenderTarget(NoesisRenderTarget* target);
FORGE_API NoesisTexture* getNoesisUIRenderTargetTexture(NoesisRenderTarget* target);
FORGE_API RenderTarget* getNoesisUIRenderTargetTF(NoesisRenderTarget* target);
