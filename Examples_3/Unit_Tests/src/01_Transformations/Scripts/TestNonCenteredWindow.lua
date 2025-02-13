local plat = loader.GetPlatformName()

if plat ~= "Unsupported" then 
	loader.SetClientRectangleXOffset(10)
	loader.SetClientRectangleYOffset(10)
	loader.SetClientRectangleWidth(1280)
	loader.SetClientRectangleHeight(700)
		
	--[[ At this point window is non-centered, along with size, positoin should be updated ]]--
	loader.SetclientrectangleOnEdited()
	
	loader.SetCounter(2)
end