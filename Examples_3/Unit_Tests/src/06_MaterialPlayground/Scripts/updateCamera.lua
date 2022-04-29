--[[
Copyright (c) 2017-2022 The Forge Interactive Inc.
]]--

--[[
local loopSize = 500
local sleepSize = 10
local x = -6.12865686
local y = 12.2564745
local z = 59.3652649

for i = 1,loopSize do
    luaCamera.MoveCamera(x, y, z)
    x = x + math.sin(i * 3.141592653589793 / 180.0)
    conffLib.sleep(sleepSize)
end

]]--

local x, y, z = loader.GetCameraPosition()
local t = 0
local period = 2 * math.pi
local amplitude = 10.0
local animate = 0

function Update(dt)
	animate = loader.GetIsCameraAnimated()
	if (animate == 0) then
		return
	end

    new_x = x + math.sin( t ) * amplitude
    loader.SetCameraPosition(new_x, y, z)
	loader.LookAtWorldOrigin()
    t = t + dt
    while (t > period) do
        t = t - period
    end
end

function Exit(dt)
    loader.SetCameraPosition(x,y,z)
end