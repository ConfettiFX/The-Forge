local plat = loader.GetPlatformName()

if plat ~= "Unsupported" then 
	loader.SetToggleWindowCentered(1)
	loader.BorderlessOnEdited()
	loader.SetrecommendedwindowrectangleOnEdited()
	
	loader.SetCounter(2)
end
