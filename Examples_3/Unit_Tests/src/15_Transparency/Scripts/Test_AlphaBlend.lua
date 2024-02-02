loader.SetTransparencyType(0)
loader.SetSortObjects(1)
loader.SetSortParticles(1)

loader.SetCounter(5)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end
loader.SetScreenshotName("API" .. selectedAPIIndex .. "_" .. "AlphaBlend")
loader.TakeScreenshotOnEdited()