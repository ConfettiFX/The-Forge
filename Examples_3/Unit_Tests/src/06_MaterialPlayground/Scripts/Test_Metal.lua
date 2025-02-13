if loader.GetMaterialType() == 1 then
	intensity = loader.GetEnvironmentLightIntensity()
	loader.SetEnvironmentLightIntensity(intensity - 0.02)

	intensity = loader.GetAmbientLightIntensity()
	loader.SetAmbientLightIntensity(intensity - 0.02)

	intensity = loader.GetDirectionalLightIntensity()
	loader.SetDirectionalLightIntensity(intensity - 0.2)
end

loader.SetMaterialType(0)
loader.SetAnimateCamera(1)
loader.SetSkybox(0)
loader.SetEnvironmentLighting(0)
loader.SetCounter(10)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end

if loader.RequestScreenshotCapture ~= nil then
	loader.RequestScreenshotCapture("API" .. selectedAPIIndex .. "_" .. "Metal")
end

