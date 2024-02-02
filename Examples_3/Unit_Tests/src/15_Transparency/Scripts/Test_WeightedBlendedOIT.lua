loader.SetTransparencyType(1)

math.randomseed( 0 )
loader.SetColorResistance(math.random() + math.random(1.0, 24.0))
loader.SetRangeAdjustment(math.random())
loader.SetDepthRange(math.random() + math.random(0.0, 499.0))
loader.SetOrderingStrength(math.random() + math.random(0.0, 24.0))

loader.SetCounter(5)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end
loader.SetScreenshotName("API" .. selectedAPIIndex .. "_" .. "WeightedBlendedOIT")
loader.TakeScreenshotOnEdited()