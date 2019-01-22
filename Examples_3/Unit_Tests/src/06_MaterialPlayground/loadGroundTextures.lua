local TEXTURE_RESOLUTION = "2K"

local groundTextures = {
    "PBR/snow_white_tiles_02/" .. TEXTURE_RESOLUTION .. "/Albedo.png",
    "PBR/snow_white_tiles_02/" .. TEXTURE_RESOLUTION .. "/Normal.png",
    "PBR/Metallic_on.png",
    "PBR/snow_white_tiles_02/" .. TEXTURE_RESOLUTION .. "/Roughness2.png",
    "PBR/snow_white_tiles_02/" .. TEXTURE_RESOLUTION .. "/AO.png"
}

loader.LoadTextureMaps(groundTextures, 1)
