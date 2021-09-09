loader.SetShadowType(0) -- ESM
math.randomseed( os.time() )

sun_control_x = math.random() + math.random(-3, 3)
sun_control_y = math.random() + math.random(-3, 3)
loader.SetSunControl(sun_control_x, sun_control_y)

esm_control = math.random() + math.random(1, 300)
loader.SetESMControl(esm_control)

loader.SetCounter(5)