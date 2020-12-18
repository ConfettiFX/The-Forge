loader.SetShadowType(2) -- SDF

loader.SetAutomaticSunMovement((loader.GetAutomaticSunMovement()+1) % 2)

math.randomseed( os.time() )
loader.SetLightSourceAngle(math.random() + math.random(0.0, 3.0))
--loader.SetDisplaybakedSDFmeshdataonthescreen((loader.GetDisplaybakedSDFmeshdataonthescreen()+1) % 2)

loader.SetCounter(5)