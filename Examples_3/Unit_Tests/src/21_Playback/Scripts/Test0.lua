loader.SetAutoSetBlendParams(0)
math.randomseed( os.time() )
loader.SetClipWeightStand(math.random())
loader.SetThreshold(math.random())

loader.SetAnimationTime(0.01)
loader.SetPlaybackSpeed(math.random() + math.random(-5.0, 4.0))

loader.SetCounter(5)