function random_range(min, max)
    return min + math.random() * (max - min)
end

loader.SetShadowType(3) -- VSM
math.randomseed( 1 )
loader.SetCounter(5)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end

if loader.RequestScreenshotCapture ~= nil then
    loader.RequestScreenshotCapture("API" .. selectedAPIIndex .. "_" .. "VSM")
end