local plat = loader.GetPlatformName()

if plat ~= "Unsupported" then 
	loader.SetToggleWindowCentered(1)
	loader.SetrecommendedwindowrectangleOnEdited()
	loader.BorderlessOnEdited()
	
	loader.SetCounter(3)
end
