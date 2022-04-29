local plat = loader.GetPlatformName()

if plat ~= "Unsupported" then 
	loader.WindowedOnEdited()
	
	loader.SetWindowXOffset(10)
	loader.SetWindowYOffset(10)
	loader.SetWindowWidth(800)
	loader.SetWindowHeight(600)
	
	loader.SetToggleWindowCentered(1)
	
	--[[ At this point window is centered, so x & y should not have any effect. ]]--
	loader.SetwindowrectangleOnEdited()
	
	loader.SetCounter(3)
end