local plat = loader.GetPlatformName()

if plat ~= "Unsupported" then 
	loader.FullscreenOnEdited()
	loader.SetCounter(3)
end
