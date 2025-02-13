loader.SetMaterialType(2)
loader.SetAnimateCamera(0)
loader.SetCounter(10)

loader.CameraLookAtFromEye(24.78, 3.495, 11.407, -95.964, -7.98, -26.96)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end

if loader.RequestScreenshotCapture ~= nil then
	loader.RequestScreenshotCapture("API" .. selectedAPIIndex .. "_" .. "Hair")
end