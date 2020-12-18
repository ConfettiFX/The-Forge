loader.SetFillMode((loader.GetFillMode() + 1) % 2)	-- SOLID / WIREFRAME
loader.SetWindMode((loader.GetWindMode() + 1) % 2)	-- STRAIGHT / RADIAL

wind_speed = loader.GetWindSpeed()
loader.SetWindSpeed((wind_speed + 25.0) % 100.0)

wave_width = loader.GetWaveWidth()
loader.SetWaveWidth((wave_width + 6.0) % 20.0)

wind_strength = loader.GetWindStrength()
loader.SetWindStrength((wind_strength + 15.0) % 100.0)

math.randomseed( os.time() )
random_level = math.random(10)
loader.SetMaxTessellationLevel(random_level)