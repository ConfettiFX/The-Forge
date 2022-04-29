--[[ Use test frame count to set counter, adjust when script is not read after switching --]]
local testingFrameCount = 120
local x = loader.SizeSelectAPI()
local c = loader.GetSelectAPI()

local newIndex = (c + 1) % x
if newIndex ~= 0 then
	loader.SetSelectAPI(newIndex)
	loader.SelectAPIOnEdited()
	loader.SetCounter(testingFrameCount / x)
end
