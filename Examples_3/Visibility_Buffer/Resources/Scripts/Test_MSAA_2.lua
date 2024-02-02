loader.SetMSAA(1) -- 2 Samples
loader.MSAAOnEdited()
loader.SetCounter(5)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end
loader.SetScreenshotName("API" .. selectedAPIIndex .. "_" .. "MSAA2")
loader.TakeScreenshotOnEdited()