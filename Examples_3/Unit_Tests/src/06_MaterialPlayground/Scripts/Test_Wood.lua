if loader.GetMaterialType() ~= 1 then
	intensity = loader.GetEnvironmentLightIntensity()
	loader.SetEnvironmentLightIntensity(intensity + 0.02)

	intensity = loader.GetAmbientLightIntensity()
	loader.SetAmbientLightIntensity(intensity + 0.02)

	intensity = loader.GetDirectionalLightIntensity()
	loader.SetDirectionalLightIntensity(intensity + 0.2)
end

loader.SetMaterialType(1)
loader.SetAnimateCamera(1)
loader.SetSkybox(1)
loader.SetEnvironmentLighting(1)
loader.SetCounter(10)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end
loader.SetScreenshotName("API" .. selectedAPIIndex .. "_" .. "Wood")
loader.TakeScreenshotOnEdited()
