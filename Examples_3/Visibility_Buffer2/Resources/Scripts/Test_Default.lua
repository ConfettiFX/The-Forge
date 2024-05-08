--[[ Captures a screenshot with default parameters, this test is run before any test defined in the UT IApp::init function --]]

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end
loader.SetScreenshotName("API" .. selectedAPIIndex .. "_" .. "Default")
loader.TakeScreenshotOnEdited()
