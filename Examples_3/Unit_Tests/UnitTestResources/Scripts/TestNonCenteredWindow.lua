local plat = loader.GetPlatformName()

if plat ~= "Unsupported" then 
	loader.SetWindowXOffset(10)
	loader.SetWindowYOffset(10)
	loader.SetWindowWidth(1280)
	loader.SetWindowHeight(700)
	
	loader.SetToggleWindowCentered(0)
	
	--[[ At this point window is non-centered, along with size, positoin should be updated ]]--
	loader.SetwindowrectangleOnEdited()
	
	loader.SetCounter(3)
end