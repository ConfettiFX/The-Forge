--[[
Copyright (c) 2017-2022 The Forge Interactive Inc.
]]--

local TEXTURE_RESOLUTION = "2K" -- default value. Application sets this.

TEXTURE_RESOLUTION = loader.GetTextureResolution()
SKIP_LOADING_TEXTURES = loader.GetSkipLoadingTexturesFlag()

local textures = {
		-- ===========================================================================
		-- METALS
		-- ===========================================================================
        "PBR/round_aluminum_panel_01/" .. TEXTURE_RESOLUTION .. "/Albedo",
        "PBR/round_aluminum_panel_01/" .. TEXTURE_RESOLUTION .. "/Normal",
        "PBR/Metallic_on",
        "PBR/round_aluminum_panel_01/" .. TEXTURE_RESOLUTION .. "/Roughness",
        "PBR/round_aluminum_panel_01/" .. TEXTURE_RESOLUTION .. "/AO",
        --//------------------------------
        -- "PBR/painted_metal_02/" .. TEXTURE_RESOLUTION .. "/Albedo",
        -- "PBR/painted_metal_02/" .. TEXTURE_RESOLUTION .. "/Normal",
        -- "PBR/painted_metal_02/" .. TEXTURE_RESOLUTION .. "/Metallic",
        -- "PBR/painted_metal_02/" .. TEXTURE_RESOLUTION .. "/Roughness",
        -- "PBR/painted_metal_02/" .. TEXTURE_RESOLUTION .. "/AO",
         "PBR/scratched_gold_01/" .. TEXTURE_RESOLUTION .. "/Albedo",
         "PBR/scratched_gold_01/" .. TEXTURE_RESOLUTION .. "/Normal",
         "PBR/Metallic_on",
         "PBR/scratched_gold_01/" .. TEXTURE_RESOLUTION .. "/Roughness",
         "PBR/scratched_gold_01/" .. TEXTURE_RESOLUTION .. "/AO",
        --//------------------------------
        "PBR/copper_tiles_01/" .. TEXTURE_RESOLUTION .. "/Albedo",
        "PBR/copper_tiles_01/" .. TEXTURE_RESOLUTION .. "/Normal",
        "PBR/Metallic_on",
        "PBR/copper_tiles_01/" .. TEXTURE_RESOLUTION .. "/Roughness",
        "PBR/copper_tiles_01/" .. TEXTURE_RESOLUTION .. "/AO",
        --//------------------------------
        "PBR/metal_tiles_01/" .. TEXTURE_RESOLUTION .. "/Albedo",
        "PBR/metal_tiles_01/" .. TEXTURE_RESOLUTION .. "/Normal",
        "PBR/Metallic_on",
        "PBR/metal_tiles_01/" .. TEXTURE_RESOLUTION .. "/Roughness",
        "PBR/metal_tiles_01/" .. TEXTURE_RESOLUTION .. "/AO",
        --//------------------------------
        "PBR/old_iron_01/" .. TEXTURE_RESOLUTION .. "/Albedo",
        "PBR/old_iron_01/" .. TEXTURE_RESOLUTION .. "/Normal",
        "PBR/old_iron_01/" .. TEXTURE_RESOLUTION .. "/Metallic",
        "PBR/old_iron_01/" .. TEXTURE_RESOLUTION .. "/Roughness",
        "PBR/Metallic_on",
        --//------------------------------
        "PBR/bronze_01/" .. TEXTURE_RESOLUTION .. "/Albedo",
        "PBR/bronze_01/" .. TEXTURE_RESOLUTION .. "/Normal",
        "PBR/bronze_01/" .. TEXTURE_RESOLUTION .. "/Metallic",
        "PBR/bronze_01/" .. TEXTURE_RESOLUTION .. "/Roughness",
        "PBR/Metallic_on",
        --//------------------------------


		-- ===========================================================================
		-- WOOD 
		-- ===========================================================================
        "PBR/wooden_planks_05/" .. TEXTURE_RESOLUTION .. "/Albedo",
        "PBR/wooden_planks_05/" .. TEXTURE_RESOLUTION .. "/Normal",
        "PBR/wooden_planks_05/" .. TEXTURE_RESOLUTION .. "/Metallic",
        "PBR/wooden_planks_05/" .. TEXTURE_RESOLUTION .. "/Roughness",
        "PBR/wooden_planks_05/" .. TEXTURE_RESOLUTION .. "/AO",
        --//------------------------------
        "PBR/wooden_planks_06/" .. TEXTURE_RESOLUTION .. "/Albedo",
        "PBR/wooden_planks_06/" .. TEXTURE_RESOLUTION .. "/Normal",
        "PBR/wooden_planks_06/" .. TEXTURE_RESOLUTION .. "/Metallic",
        "PBR/wooden_planks_06/" .. TEXTURE_RESOLUTION .. "/Roughness",
        "PBR/wooden_planks_06/" .. TEXTURE_RESOLUTION .. "/AO",
        --//------------------------------
        "PBR/wood_#03/" .. TEXTURE_RESOLUTION .. "/Albedo",
        "PBR/wood_#03/" .. TEXTURE_RESOLUTION .. "/Normal",
        "PBR/wood_#03/" .. TEXTURE_RESOLUTION .. "/Metallic",
        "PBR/wood_#03/" .. TEXTURE_RESOLUTION .. "/Roughness",
        "PBR/wood_#03/" .. TEXTURE_RESOLUTION .. "/AO",
        --//------------------------------
        "PBR/wood_#08/" .. TEXTURE_RESOLUTION .. "/Albedo",
        "PBR/wood_#08/" .. TEXTURE_RESOLUTION .. "/Normal",
        "PBR/wood_#08/" .. TEXTURE_RESOLUTION .. "/Metallic",
        "PBR/wood_#08/" .. TEXTURE_RESOLUTION .. "/Roughness",
        "PBR/wood_#08/" .. TEXTURE_RESOLUTION .. "/AO",
        --//------------------------------
        "PBR/wood_#16/" .. TEXTURE_RESOLUTION .. "/Albedo",
        "PBR/wood_#16/" .. TEXTURE_RESOLUTION .. "/Normal",
        "PBR/wood_#16/" .. TEXTURE_RESOLUTION .. "/Metallic",
        "PBR/wood_#16/" .. TEXTURE_RESOLUTION .. "/Roughness",
        "PBR/wood_#16/" .. TEXTURE_RESOLUTION .. "/AO",
        --//------------------------------
        "PBR/wood_#18/" .. TEXTURE_RESOLUTION .. "/Albedo",
        "PBR/wood_#18/" .. TEXTURE_RESOLUTION .. "/Normal",
        "PBR/wood_#18/" .. TEXTURE_RESOLUTION .. "/Metallic",
        "PBR/wood_#18/" .. TEXTURE_RESOLUTION .. "/Roughness",
        "PBR/wood_#18/" .. TEXTURE_RESOLUTION .. "/AO",
        --//------------------------------
}

local empty_textures = {
		-- ===========================================================================
		-- METALS
		-- ===========================================================================
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        --//------------------------------
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        --//------------------------------
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        --//------------------------------
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        --//------------------------------
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        --//------------------------------
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        --//------------------------------

		-- ===========================================================================
		-- WOOD 
		-- ===========================================================================
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        --//------------------------------
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        --//------------------------------
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        --//------------------------------
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        --//------------------------------
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        --//------------------------------
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        "PBR/Metallic_on",
        --//------------------------------
}

if (SKIP_LOADING_TEXTURES == 0) then
	loader.LoadTextureMaps(textures)
else
	loader.LoadTextureMaps(empty_textures)
end