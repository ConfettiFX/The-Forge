if loader.SetShowthreadsplot ~= nil then
	loader.SetShowthreadsplot(0)
end

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end
loader.SetScreenshotName("API" .. selectedAPIIndex .. "_" .. "TurnOffPlots")
loader.TakeScreenshotOnEdited()