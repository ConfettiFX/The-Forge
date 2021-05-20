--[[ Use test frame count to set counter, adjust when script is not read after switching --]]
local testingFrameCount = 120
local x = loader.SizeSelectAPI()
local c = loader.GetSelectAPI()

loader.SetSelectAPI((c + 1) % x)
loader.SelectAPIOnEdited()

loader.SetCounter(testingFrameCount / x)