local TEXTURE_RESOLUTION = "2K" -- default value. Application sets this.

TEXTURE_RESOLUTION = loader.GetTextureResolution()
SKIP_LOADING_TEXTURES = loader.GetSkipLoadingTexturesFlag()

local textures = {
		-- ===========================================================================
		-- METALS
		-- ===========================================================================
        "PBR/round_aluminum_panel_01/" .. TEXTURE_RESOLUTION .. "/Albedo.png",
        "PBR/round_aluminum_panel_01/" .. TEXTURE_RESOLUTION .. "/Normal.png",
        "PBR/Metallic_on.png",
        "PBR/round_aluminum_panel_01/" .. TEXTURE_RESOLUTION .. "/Roughness.png",
        "PBR/round_aluminum_panel_01/" .. TEXTURE_RESOLUTION .. "/AO.png",
        --//------------------------------
        -- "PBR/painted_metal_02/" .. TEXTURE_RESOLUTION .. "/Albedo.png",
        -- "PBR/painted_metal_02/" .. TEXTURE_RESOLUTION .. "/Normal.png",
        -- "PBR/painted_metal_02/" .. TEXTURE_RESOLUTION .. "/Metallic.png",
        -- "PBR/painted_metal_02/" .. TEXTURE_RESOLUTION .. "/Roughness.png",
        -- "PBR/painted_metal_02/" .. TEXTURE_RESOLUTION .. "/AO.png",
         "PBR/scratched_gold_01/" .. TEXTURE_RESOLUTION .. "/Albedo.png",
         "PBR/scratched_gold_01/" .. TEXTURE_RESOLUTION .. "/Normal.png",
         "PBR/Metallic_on.png",
         "PBR/scratched_gold_01/" .. TEXTURE_RESOLUTION .. "/Roughness.png",
         "PBR/scratched_gold_01/" .. TEXTURE_RESOLUTION .. "/AO.png",
        --//------------------------------
        "PBR/copper_tiles_01/" .. TEXTURE_RESOLUTION .. "/Albedo.png",
        "PBR/copper_tiles_01/" .. TEXTURE_RESOLUTION .. "/Normal.png",
        "PBR/Metallic_on.png",
        "PBR/copper_tiles_01/" .. TEXTURE_RESOLUTION .. "/Roughness.png",
        "PBR/copper_tiles_01/" .. TEXTURE_RESOLUTION .. "/AO.png",
        --//------------------------------
        "PBR/metal_tiles_01/" .. TEXTURE_RESOLUTION .. "/Albedo.png",
        "PBR/metal_tiles_01/" .. TEXTURE_RESOLUTION .. "/Normal.png",
        "PBR/Metallic_on.png",
        "PBR/metal_tiles_01/" .. TEXTURE_RESOLUTION .. "/Roughness.png",
        "PBR/metal_tiles_01/" .. TEXTURE_RESOLUTION .. "/AO.png",
        --//------------------------------
        "PBR/old_iron_01/" .. TEXTURE_RESOLUTION .. "/Albedo.png",
        "PBR/old_iron_01/" .. TEXTURE_RESOLUTION .. "/Normal.png",
        "PBR/old_iron_01/" .. TEXTURE_RESOLUTION .. "/Metallic.png",
        "PBR/old_iron_01/" .. TEXTURE_RESOLUTION .. "/Roughness.png",
        "PBR/Metallic_on.png",
        --//------------------------------
        "PBR/bronze_01/" .. TEXTURE_RESOLUTION .. "/Albedo.png",
        "PBR/bronze_01/" .. TEXTURE_RESOLUTION .. "/Normal.png",
        "PBR/bronze_01/" .. TEXTURE_RESOLUTION .. "/Metallic.png",
        "PBR/bronze_01/" .. TEXTURE_RESOLUTION .. "/Roughness.png",
        "PBR/Metallic_on.png",
        --//------------------------------


		-- ===========================================================================
		-- WOOD 
		-- ===========================================================================
        "PBR/wooden_planks_05/" .. TEXTURE_RESOLUTION .. "/Albedo.png",
        "PBR/wooden_planks_05/" .. TEXTURE_RESOLUTION .. "/Normal.png",
        "PBR/wooden_planks_05/" .. TEXTURE_RESOLUTION .. "/Metallic.png",
        "PBR/wooden_planks_05/" .. TEXTURE_RESOLUTION .. "/Roughness.png",
        "PBR/wooden_planks_05/" .. TEXTURE_RESOLUTION .. "/AO.png",
        --//------------------------------
        "PBR/wooden_planks_06/" .. TEXTURE_RESOLUTION .. "/Albedo.png",
        "PBR/wooden_planks_06/" .. TEXTURE_RESOLUTION .. "/Normal.png",
        "PBR/wooden_planks_06/" .. TEXTURE_RESOLUTION .. "/Metallic.png",
        "PBR/wooden_planks_06/" .. TEXTURE_RESOLUTION .. "/Roughness.png",
        "PBR/wooden_planks_06/" .. TEXTURE_RESOLUTION .. "/AO.png",
        --//------------------------------
        "PBR/wood_#03/" .. TEXTURE_RESOLUTION .. "/Albedo.jpg",
        "PBR/wood_#03/" .. TEXTURE_RESOLUTION .. "/Normal.jpg",
        "PBR/wood_#03/" .. TEXTURE_RESOLUTION .. "/Metallic.png",
        "PBR/wood_#03/" .. TEXTURE_RESOLUTION .. "/Roughness.jpg",
        "PBR/wood_#03/" .. TEXTURE_RESOLUTION .. "/AO.png",
        --//------------------------------
        "PBR/wood_#08/" .. TEXTURE_RESOLUTION .. "/Albedo.jpg",
        "PBR/wood_#08/" .. TEXTURE_RESOLUTION .. "/Normal.jpg",
        "PBR/wood_#08/" .. TEXTURE_RESOLUTION .. "/Metallic.png",
        "PBR/wood_#08/" .. TEXTURE_RESOLUTION .. "/Roughness.jpg",
        "PBR/wood_#08/" .. TEXTURE_RESOLUTION .. "/AO.png",
        --//------------------------------
        "PBR/wood_#16/" .. TEXTURE_RESOLUTION .. "/Albedo.jpg",
        "PBR/wood_#16/" .. TEXTURE_RESOLUTION .. "/Normal.jpg",
        "PBR/wood_#16/" .. TEXTURE_RESOLUTION .. "/Metallic.png",
        "PBR/wood_#16/" .. TEXTURE_RESOLUTION .. "/Roughness.jpg",
        "PBR/wood_#16/" .. TEXTURE_RESOLUTION .. "/AO.png",
        --//------------------------------
        "PBR/wood_#18/" .. TEXTURE_RESOLUTION .. "/Albedo.jpg",
        "PBR/wood_#18/" .. TEXTURE_RESOLUTION .. "/Normal.jpg",
        "PBR/wood_#18/" .. TEXTURE_RESOLUTION .. "/Metallic.png",
        "PBR/wood_#18/" .. TEXTURE_RESOLUTION .. "/Roughness.jpg",
        "PBR/wood_#18/" .. TEXTURE_RESOLUTION .. "/AO.png",
        --//------------------------------
}

local empty_textures = {
		-- ===========================================================================
		-- METALS
		-- ===========================================================================
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        --//------------------------------
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        --//------------------------------
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        --//------------------------------
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        --//------------------------------
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        --//------------------------------
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        --//------------------------------

		-- ===========================================================================
		-- WOOD 
		-- ===========================================================================
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        --//------------------------------
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        --//------------------------------
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        --//------------------------------
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        --//------------------------------
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        --//------------------------------
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        "PBR/Metallic_on.png",
        --//------------------------------
}

if (SKIP_LOADING_TEXTURES == 0) then
	loader.LoadTextureMaps(textures, 1)
else
	loader.LoadTextureMaps(empty_textures, 0)
end