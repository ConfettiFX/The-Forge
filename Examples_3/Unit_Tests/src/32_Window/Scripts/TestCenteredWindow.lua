loader.WindowedOnEdited()

loader.Setx(10)
loader.Sety(10)
loader.Setw(800)
loader.Seth(600)

loader.SetCentered(1)

--[[ At this point window is centered, so x & y should not have any effect. ]]--
loader.SetwindowrectangleOnEdited()

loader.SetCounter(3)