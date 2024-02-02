loader.SetMSAA(2) -- 4 Samples
loader.MSAAOnEdited()
loader.SetCounter(5)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end
loader.SetScreenshotName("API" .. selectedAPIIndex .. "_" .. "MSAA4")
loader.TakeScreenshotOnEdited()