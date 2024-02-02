loader.SetMSAA(0) -- MSAA Off
loader.MSAAOnEdited()
loader.SetCounter(5)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end
loader.SetScreenshotName("API" .. selectedAPIIndex .. "_" .. "MSAA0")
loader.TakeScreenshotOnEdited()
