local plat = loader.GetPlatformName()

if plat ~= "Unsupported" then 
	loader.WindowedOnEdited()
	
	loader.SetClientRectangleXOffset(10)
	loader.SetClientRectangleYOffset(10)
	loader.SetClientRectangleWidth(800)
	loader.SetClientRectangleHeight(600)
	
	loader.CenterOnEdited()
	
	--[[ At this point window is centered, so x & y should not have any effect. ]]--
	loader.SetclientrectangleOnEdited()
	
	loader.SetCounter(2)
end