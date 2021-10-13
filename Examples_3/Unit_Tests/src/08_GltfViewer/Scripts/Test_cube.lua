loader.SetLoadModel(3)

math.randomseed( os.time() )
loader.SetLightAzimuth(math.random() + math.random(-180, 179))
loader.SetLightElevation(math.random() + math.random(210, 330))
loader.SetAmbientLightColor(math.random(0, 4294967142))
loader.SetAmbientLightIntensity(math.random() + math.random(0, 5))

loader.SetEnableTemporalAA((loader.GetEnableTemporalAA()+1) % 2)
loader.SetEnableVignetting((loader.GetEnableVignetting()+1) % 2)

loader.SetCounter(5)