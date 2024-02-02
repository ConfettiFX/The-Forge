loader.SetRenderMode(3)
loader.SetCounter(5)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end
loader.SetScreenshotName("API" .. selectedAPIIndex .. "_" .. "WaveGetMaxActiveIndex")
loader.TakeScreenshotOnEdited()