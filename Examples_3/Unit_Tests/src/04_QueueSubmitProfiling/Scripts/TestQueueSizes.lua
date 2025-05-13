
loader.SetTotalSubmitCount(4)
loader.SetTotalCommands(2000)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end

if loader.RequestScreenshotCapture ~= nil then
	loader.RequestScreenshotCapture("API" .. selectedAPIIndex .. "_" .. "TestQueueSizes")
end