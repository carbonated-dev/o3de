--------------------------------------------------------------------------------------
--
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
--
--
--
----------------------------------------------------------------------------------------------------

-- CARBONATED Addition.
-- This functor controls flags that enables features for the shader.

function GetMaterialPropertyDependencies()
    return {
        "enable",
        "opacityMap",
        "useOpacityMap",
        "factorMap",
        "useFactorMap",
        "albedoMap",
        "useAlbedoMap",
        "normalMap", 
        "useNormalMap", 
        "roughnessMap", 
        "useRoughnessMap", 
        }
end

function GetShaderOptionDependencies()
    return {
        "o_snow_enabled",
        "o_snow_opacity_useTexture",
        "o_snow_factor_useTexture",
        "o_snow_albedo_useTexture",
        "o_snow_normal_useTexture",
        "o_snow_roughness_useTexture",
        }
end

function UpdateUseTextureState(context, enable, textureMapPropertyName, useTexturePropertyName, shaderOptionName) 
    local textureMap = context:GetMaterialPropertyValue_Image(textureMapPropertyName)
    local useTextureMap = context:GetMaterialPropertyValue_bool(useTexturePropertyName)
    context:SetShaderOptionValue_bool(shaderOptionName, enable and useTextureMap and textureMap ~= nil)
end

function Process(context)
    local enable = context:GetMaterialPropertyValue_bool("enable")
    context:SetShaderOptionValue_bool("o_snow_enabled", enable)

    UpdateUseTextureState(context, enable, "opacityMap",   "useOpacityMap",   "o_snow_opacity_useTexture")
    UpdateUseTextureState(context, enable, "factorMap",    "useFactorMap",    "o_snow_factor_useTexture")
    UpdateUseTextureState(context, enable, "albedoMap",    "useAlbedoMap",    "o_snow_albedo_useTexture")
    UpdateUseTextureState(context, enable, "normalMap",    "useNormalMap",    "o_snow_normal_useTexture")    
    UpdateUseTextureState(context, enable, "roughnessMap", "useRoughnessMap", "o_snow_roughness_useTexture")
end

-- Note this logic matches that of the UseTextureFunctor class.
function UpdateTextureDependentPropertyVisibility(context, textureMapPropertyName, useTexturePropertyName, uvPropertyName)
    local textureMap = context:GetMaterialPropertyValue_Image(textureMapPropertyName)
    local useTexture = context:GetMaterialPropertyValue_bool(useTexturePropertyName)

    if(textureMap == nil) then
        context:SetMaterialPropertyVisibility(useTexturePropertyName, MaterialPropertyVisibility_Hidden)
        context:SetMaterialPropertyVisibility(uvPropertyName, MaterialPropertyVisibility_Hidden)
    elseif(not useTexture) then
        context:SetMaterialPropertyVisibility(uvPropertyName, MaterialPropertyVisibility_Disabled)
    end
end

function UpdateFactorPropertyVisibility(context, textureMapPropertyName, useTexturePropertyName)
    local textureMap = context:GetMaterialPropertyValue_Image(textureMapPropertyName)
    local useTexture = context:GetMaterialPropertyValue_bool(useTexturePropertyName)

    if(textureMap == nil) or (not useTexture) then
        context:SetMaterialPropertyVisibility("factor", MaterialPropertyVisibility_Enabled)
    else
        context:SetMaterialPropertyVisibility("factor", MaterialPropertyVisibility_Hidden)
    end
end

function UpdateNormalStrengthPropertyVisibility(context, textureMapPropertyName, useTexturePropertyName)
    local textureMap = context:GetMaterialPropertyValue_Image(textureMapPropertyName)
    local useTexture = context:GetMaterialPropertyValue_bool(useTexturePropertyName)

    if(textureMap == nil) or (not useTexture) then
        context:SetMaterialPropertyVisibility("normalStrength", MaterialPropertyVisibility_Hidden)
    else
        context:SetMaterialPropertyVisibility("normalStrength", MaterialPropertyVisibility_Enabled)
    end
end


function ProcessEditor(context)
    local enable = context:GetMaterialPropertyValue_bool("enable")
    
    local mainVisibility
    if(enable) then
        mainVisibility = MaterialPropertyVisibility_Enabled
    else
        mainVisibility = MaterialPropertyVisibility_Hidden
    end

    context:SetMaterialPropertyVisibility("amount", mainVisibility)
    context:SetMaterialPropertyVisibility("opacityMap", mainVisibility)
    context:SetMaterialPropertyVisibility("useOpacityMap", mainVisibility)
    context:SetMaterialPropertyVisibility("opacityMapUv", mainVisibility)
    
    context:SetMaterialPropertyVisibility("factor", mainVisibility)
    context:SetMaterialPropertyVisibility("factorMap", mainVisibility)
    context:SetMaterialPropertyVisibility("useFactorMap", mainVisibility)
    context:SetMaterialPropertyVisibility("factorMapUv", mainVisibility)
    
    context:SetMaterialPropertyVisibility("tintColor", mainVisibility)
    context:SetMaterialPropertyVisibility("albedoMap", mainVisibility)
    context:SetMaterialPropertyVisibility("useAlbedoMap", mainVisibility)
    context:SetMaterialPropertyVisibility("albedoMapUv", mainVisibility)
    
    context:SetMaterialPropertyVisibility("normalMap", mainVisibility)
    context:SetMaterialPropertyVisibility("useNormalMap", mainVisibility)
    context:SetMaterialPropertyVisibility("normalMapUv", mainVisibility)
    context:SetMaterialPropertyVisibility("normalStrength", mainVisibility)
    
    context:SetMaterialPropertyVisibility("roughness", mainVisibility)
    context:SetMaterialPropertyVisibility("roughnessMap", mainVisibility)
    context:SetMaterialPropertyVisibility("useRoughnessMap", mainVisibility)
    context:SetMaterialPropertyVisibility("roughnessMapUv", mainVisibility)

    if(enable) then
        UpdateTextureDependentPropertyVisibility(context, "opacityMap", "useOpacityMap", "opacityMapUv")
        UpdateTextureDependentPropertyVisibility(context, "factorMap",  "useFactorMap",  "factorMapUv")
        UpdateFactorPropertyVisibility(context, "factorMap", "useFactorMap")
        UpdateTextureDependentPropertyVisibility(context, "albedoMap",  "useAlbedoMap",  "albedoMapUv")
        UpdateTextureDependentPropertyVisibility(context, "normalMap",  "useNormalMap",  "normalMapUv")
        UpdateNormalStrengthPropertyVisibility(context, "normalMap", "useNormalMap")
        UpdateTextureDependentPropertyVisibility(context, "roughnessMap", "useRoughnessMap", "roughnessMapUv")
    end
end
