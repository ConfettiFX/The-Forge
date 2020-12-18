loader.SetShadowType(1) -- ASM

loader.SetSuncanmove((loader.GetSuncanmove()+1) % 2)
loader.SetParallaxcorrected((loader.GetParallaxcorrected()+1) % 2)

loader.SetDisplayASMDebugTextures((loader.GetDisplayASMDebugTextures()+1) % 2)
loader.DisplayASMDebugTexturesOnDeactivatedAfterEdit()

loader.ResetLightDirOnEdited()

math.randomseed( os.time() )
loader.SetPenumbraSize(math.random() + math.random(1.0, 149.0))
loader.SetParallaxStepDistance(math.random() + math.random(1.0, 99.0))
loader.SetParallaxStepZBias(math.random() + math.random(1.0, 199.0))

loader.SetCounter(5)