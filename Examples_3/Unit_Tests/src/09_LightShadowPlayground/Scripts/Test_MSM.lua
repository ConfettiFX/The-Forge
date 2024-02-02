function random_range(min, max)
    return min + math.random() * (max - min)
end

loader.SetShadowType(4) -- MSM
math.randomseed( 1 )

loader.SetRoundingErrorCorrection(random_range(1e-6, 6e-5))
loader.SetMSMBleedingReduction(random_range(0.0, 1.0))

loader.SetGaussianBlurKernelRadius(math.random(1, 16))
loader.SetGaussianBlurSigma(random_range(0.1, 5.0))
loader.SetCounter(5)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end
loader.SetScreenshotName("API" .. selectedAPIIndex .. "_" .. "MSM")
loader.TakeScreenshotOnEdited()