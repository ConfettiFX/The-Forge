loader.SetRaytracingTechnique(0) -- Raytracing using ray query technique
loader.SetCounter(10)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end
loader.SetScreenshotName("API" .. selectedAPIIndex .. "_" .. "RayQuery")
loader.TakeScreenshotOnEdited()
