local plat = loader.GetPlatformName()

if plat ~= "Unsupported" then 
	loader.CenterOnEdited()
	loader.BorderlessOnEdited()
	loader.SetrecommendedclientrectangleOnEdited()
	
	loader.SetCounter(2)
end
