loader.SetShadowType(0) -- ESM
math.randomseed( 0 )

esm_control = math.random() + math.random(1, 300)
loader.SetESMControl(esm_control)

loader.SetCounter(5)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end

if loader.RequestScreenshotCapture ~= nil then
	loader.RequestScreenshotCapture("API" .. selectedAPIIndex .. "_" .. "ESM")
end