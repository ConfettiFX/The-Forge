loader.SetTransparencyType(2)

math.randomseed( 0 )
loader.SetOpacitySensitivity(math.random() + math.random(1.0, 24.0))
loader.SetWeightBias(math.random() + math.random(0.0, 24.0))
loader.SetPrecisionScalar(math.random() + math.random(100.0, 99999.0))
loader.SetMaximumWeight(math.random() + math.random(0.0, 99.0))
loader.SetMaximumColorValue(math.random() + math.random(100.0, 9999.0))
loader.SetAdditiveSensitivity(math.random() + math.random(0.0, 24.0))
loader.SetEmissiveSensitivity(math.random())

loader.SetCounter(5)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end
loader.SetScreenshotName("API" .. selectedAPIIndex .. "_" .. "WeightedBlendedOITVoilition")
loader.TakeScreenshotOnEdited()