loader.SetMaterialType(1)
loader.SetSkybox(1)
loader.SetEnvironmentLighting(1)

intensity = loader.GetEnvironmentLightIntensity()
loader.SetEnvironmentLightIntensity(intensity + 0.02)

intensity = loader.GetAmbientLightIntensity()
loader.SetAmbientLightIntensity(intensity + 0.02)

intensity = loader.GetDirectionalLightIntensity()
loader.SetDirectionalLightIntensity(intensity + 0.2)

x, y, z = loader.GetLightPosition()
loader.SetLightPosition(x, y, -z)

r, g, b = loader.GetLightColor()
loader.SetLightColor(r, g, 0)

loader.SetCounter(10)