local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end

loader.RequestScreenshotCapture("API" .. selectedAPIIndex .. "_ReloadShader")
