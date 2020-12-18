--loader.SetTransparencyType(4)      -- Not handled on every platform if not supported

math.randomseed( os.time() )

-- LIGHT POSITION
x = math.random() + math.random(-10.0, 9.0)
y = math.random() + math.random(-10.0, 9.0)
z = math.random() + math.random(-10.0, 9.0)
loader.SetLightPosition(x, y, z)

loader.SetCounter(5)