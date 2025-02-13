loader.RandomizeClipsTimeOnEdited()

loader.SetNumberofRigs(4096)

loader.SetDrawPlane(1)

loader.SetShowBindPose(0)

loader.SetDrawAttachedObject(0)

loader.SetEnableThreading(1)

loader.SetAutomateThreading(1)

loader.SetAnimation(0);
loader.RunAnimationOnEdited(10)

-- Set after 'RunAnimationOnEdited' gets called as that will reset anim/ik param settings to default state...
loader.SetAimIK(1)
loader.SetTwoBoneIK(1)
loader.SetFoottwoboneIK(0.5)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end

if loader.RequestScreenshotCapture ~= nil then
	loader.RequestScreenshotCapture("API" .. selectedAPIIndex .. "_" .. "Test0")
end


