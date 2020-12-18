loader.SetTransparencyType(2)

math.randomseed( os.time() )
loader.SetOpacitySensitivity(math.random() + math.random(1.0, 24.0))
loader.SetWeightBias(math.random() + math.random(0.0, 24.0))
loader.SetPrecisionScalar(math.random() + math.random(100.0, 99999.0))
loader.SetMaximumWeight(math.random() + math.random(0.0, 99.0))
loader.SetMaximumColorValue(math.random() + math.random(100.0, 9999.0))
loader.SetAdditiveSensitivity(math.random() + math.random(0.0, 24.0))
loader.SetEmissiveSensitivity(math.random())

-- LIGHT POSITION
x = math.random() + math.random(-10.0, 9.0)
y = math.random() + math.random(-10.0, 9.0)
z = math.random() + math.random(-10.0, 9.0)
loader.SetLightPosition(x, y, z)

loader.SetCounter(5)