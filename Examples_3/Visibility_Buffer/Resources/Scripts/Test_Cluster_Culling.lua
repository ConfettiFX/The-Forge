loader.SetClusterCulling((loader.GetClusterCulling()+1) % 2)
loader.SetCounter(2)

local selectedAPIIndex = 0
if loader.GetSelectAPI ~= nil then
	selectedAPIIndex = loader.GetSelectAPI()
end
loader.SetScreenshotName("API" .. selectedAPIIndex .. "_" .. "ClusterCulling")
loader.TakeScreenshotOnEdited()