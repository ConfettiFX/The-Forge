loader.SetEnableVariableRateShading(1)
loader.EnableVariableRateShadingOnEdited()
loader.SetCounter(5)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end
loader.SetScreenshotName("API" .. selectedAPIIndex .. "_" .. "VRS")
loader.TakeScreenshotOnEdited()