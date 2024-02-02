math.randomseed( 0 )

loader.SetCameraHorizontalFoV(math.random() + math.random(30.0, 178.0))
loader.SetPaniniDParameter(math.random())
loader.SetPaniniSParameter(math.random())
loader.SetScreenScale(math.random() + math.random(1.0, 9.0))

loader.SetCounter(5)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end
loader.SetScreenshotName("API" .. selectedAPIIndex .. "_" .. "Test0")
loader.TakeScreenshotOnEdited()