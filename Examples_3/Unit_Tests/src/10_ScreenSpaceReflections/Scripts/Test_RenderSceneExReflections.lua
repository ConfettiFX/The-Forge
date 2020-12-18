loader.SetRenderMode(3) 		-- Render Scene with exclusive Reflections
loader.SetReflectionType(0) 	-- Pixel Projected Reflections

loader.SetUseHolepatching(1)
loader.SetUseExpensiveHolepatching(1)
loader.SetUseFadeEffect(1)

math.randomseed( os.time() )
loader.SetIntensityofPPR(math.random())
loader.SetNumberofPlanes(math.random(1, 4))
loader.SetSizeofMainPlane(math.random() + math.random(5.0, 99.0))

loader.SetCounter(5)