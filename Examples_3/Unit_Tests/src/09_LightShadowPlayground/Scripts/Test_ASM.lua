loader.SetShadowType(1) -- ASM

loader.SetParallaxcorrected((loader.GetParallaxcorrected()+1) % 2)

loader.SetDisplayASMDebugTextures((loader.GetDisplayASMDebugTextures()+1) % 2)
loader.DisplayASMDebugTexturesOnDeactivatedAfterEdit()

loader.ResetLightDirOnEdited()

math.randomseed( 0 )

loader.SetCounter(5)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end

if loader.RequestScreenshotCapture ~= nil then
	loader.RequestScreenshotCapture("API" .. selectedAPIIndex .. "_" .. "ASM")
end 