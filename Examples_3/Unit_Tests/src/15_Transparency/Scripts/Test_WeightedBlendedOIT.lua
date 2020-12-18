loader.SetTransparencyType(1)

math.randomseed( os.time() )
loader.SetColorResistance(math.random() + math.random(1.0, 24.0))
loader.SetRangeAdjustment(math.random())
loader.SetDepthRange(math.random() + math.random(0.0, 499.0))
loader.SetOrderingStrength(math.random() + math.random(0.0, 24.0))

-- LIGHT POSITION
x = math.random() + math.random(-10.0, 9.0)
y = math.random() + math.random(-10.0, 9.0)
z = math.random() + math.random(-10.0, 9.0)
loader.SetLightPosition(x, y, z)

loader.SetCounter(5)