--[[
Copyright (c) 2017-2022 The Forge Interactive Inc.
]]--

local TEXTURE_RESOLUTION = "2K"

local groundTextures = {
    "PBR/snow_white_tiles_02/" .. TEXTURE_RESOLUTION .. "/Albedo",
    "PBR/snow_white_tiles_02/" .. TEXTURE_RESOLUTION .. "/Normal",
    "PBR/Metallic_off",
    "PBR/snow_white_tiles_02/" .. TEXTURE_RESOLUTION .. "/Roughness2",
    "PBR/snow_white_tiles_02/" .. TEXTURE_RESOLUTION .. "/AO"
}

loader.LoadTextureMaps(groundTextures)
