--[[ Use test frame count to set counter, adjust when script is not read after switching --]]
if loader.SizeSelectAPI ~= nil then
	local testingFrameCount = loader.GetDefaultAutomationFrameCount()
	local apiCount = loader.SizeSelectAPI()
	local selectedAPIIndex = loader.GetSelectAPI()

	local newIndex = (selectedAPIIndex + 1) % apiCount
	if newIndex ~= 0 then -- false if we only have 1 API or when the test runs again with the last API (we don't want to start on API 0 again)
		loader.SetSelectAPI(newIndex)
		loader.SelectAPIOnEdited()
		loader.SetCounter(testingFrameCount / apiCount)
	end

	-- Screenshots requested here will capture the first frame rendered with newIndex API
end