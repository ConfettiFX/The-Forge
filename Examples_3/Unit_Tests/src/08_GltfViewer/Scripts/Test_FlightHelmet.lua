loader.SetLoadModel(1)

math.randomseed( os.time() )
loader.SetLightAzimuth(math.random() + math.random(-180, 179))
loader.SetLightElevation(math.random() + math.random(210, 330))
loader.SetLight2Color(math.random(0, 4294967295))
loader.SetLight2Intensity(math.random() + math.random(0, 5))

loader.SetEnableFXAA((loader.GetEnableFXAA()+1) % 2)
loader.SetEnableFXAA((loader.GetEnableVignetting()+1) % 2)

loader.SetCounter(5)