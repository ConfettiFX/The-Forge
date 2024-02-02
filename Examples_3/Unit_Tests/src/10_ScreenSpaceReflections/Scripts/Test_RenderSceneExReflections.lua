loader.SetRenderMode(3) 		-- Render Scene with exclusive Reflections
loader.SetReflectionType(0) 	-- Pixel Projected Reflections

loader.SetUseHolepatching(1)
loader.SetUseExpensiveHolepatching(1)
loader.SetUseFadeEffect(1)

math.randomseed( 0 )
loader.SetIntensityofPPR(math.random())
loader.SetNumberofPlanes(math.random(1, 4))
loader.SetSizeofMainPlane(math.random() + math.random(200.0, 350.0))

loader.SetCounter(5)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end
loader.SetScreenshotName("API" .. selectedAPIIndex .. "_" .. "RenderSceneExReflections")
loader.TakeScreenshotOnEdited()