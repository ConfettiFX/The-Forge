loader.SetRenderMode(2) 		-- Render Scene with Reflections
loader.SetReflectionType(1) 	-- Stochastic Screen Space Reflections

loader.SetUseHolepatching((loader.GetUseHolepatching()+1) % 2)
loader.SetUseExpensiveHolepatching((loader.GetUseExpensiveHolepatching()+1) % 2)
loader.SetUseFadeEffect((loader.GetUseFadeEffect()+1) % 2)

math.randomseed( os.time() )
loader.SetIntensityofPPR(math.random())
loader.SetNumberofPlanes(math.random(1, 4))
loader.SetSizeofMainPlane(math.random() + math.random(5.0, 99.0))

loader.SetCounter(5)