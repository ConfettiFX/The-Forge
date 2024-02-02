loader.SetMaterialType(2)
loader.SetAnimateCamera(0)
loader.SetCounter(10)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end
loader.SetScreenshotName("API" .. selectedAPIIndex .. "_" .. "Hair")
loader.TakeScreenshotOnEdited()