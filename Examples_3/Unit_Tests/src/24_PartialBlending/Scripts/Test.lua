math.randomseed( os.time() )

loader.SetAutoSetBlendParams(0)
loader.SetClipWeightStand(math.random())
loader.SetJointsWeightStand(math.random())
loader.SetClipWeightWalk(math.random())
loader.SetJointsWeightWalk(math.random())
loader.SetJointIndex(math.random(0, 60))	-- (0, No of Joint Index)