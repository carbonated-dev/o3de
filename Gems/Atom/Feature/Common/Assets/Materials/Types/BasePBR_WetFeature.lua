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
-- This functor controls the flag that enables the overall feature for the shader.

function GetMaterialPropertyDependencies()
    return {
        "enable",
        "factorMap",
        "useFactorMap",
        "roughnessMap",
        "useRoughnessMap",
        }
end

function GetShaderOptionDependencies()
    return {
        "o_wet_enabled",
        "o_wet_factor_useTexture",
        "o_wet_roughness_useTexture",
        }
end

function UpdateUseTextureState(context, enable, textureMapPropertyName, useTexturePropertyName, shaderOptionName) 
    local textureMap = context:GetMaterialPropertyValue_Image(textureMapPropertyName)
    local useTextureMap = context:GetMaterialPropertyValue_bool(useTexturePropertyName)
    context:SetShaderOptionValue_bool(shaderOptionName, enable and useTextureMap and textureMap ~= nil)
end

function Process(context)
    local enable = context:GetMaterialPropertyValue_bool("enable")
    context:SetShaderOptionValue_bool("o_wet_enabled", enable)

    UpdateUseTextureState(context, enable, "factorMap",    "useFactorMap",    "o_wet_factor_useTexture")
    UpdateUseTextureState(context, enable, "roughnessMap", "useRoughnessMap", "o_wet_roughness_useTexture")
end

-- Note: property is excluded / hidden if a texture is used.
function UpdateTextureDependentPropertyVisibility(context, propertyName, textureMapPropertyName, useTexturePropertyName, uvPropertyName, uvScalePropertyName)
    local textureMap = context:GetMaterialPropertyValue_Image(textureMapPropertyName)
    local useTexture = context:GetMaterialPropertyValue_bool(useTexturePropertyName)

    if(textureMap == nil) then
        context:SetMaterialPropertyVisibility(propertyName, MaterialPropertyVisibility_Enabled)
        context:SetMaterialPropertyVisibility(useTexturePropertyName, MaterialPropertyVisibility_Hidden)
        context:SetMaterialPropertyVisibility(uvPropertyName, MaterialPropertyVisibility_Hidden)
        context:SetMaterialPropertyVisibility(uvScalePropertyName, MaterialPropertyVisibility_Hidden)
    elseif(not useTexture) then
        context:SetMaterialPropertyVisibility(propertyName, MaterialPropertyVisibility_Enabled)
        context:SetMaterialPropertyVisibility(uvPropertyName, MaterialPropertyVisibility_Hidden)
        context:SetMaterialPropertyVisibility(uvScalePropertyName, MaterialPropertyVisibility_Hidden)
    else
        context:SetMaterialPropertyVisibility(propertyName, MaterialPropertyVisibility_Hidden)
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

    context:SetMaterialPropertyVisibility("factor", mainVisibility)
    context:SetMaterialPropertyVisibility("factorMap", mainVisibility)
    context:SetMaterialPropertyVisibility("factorMapUv", mainVisibility)
    context:SetMaterialPropertyVisibility("factorMapUvScale", mainVisibility)
    context:SetMaterialPropertyVisibility("useRoughnessMap", mainVisibility)
    
    context:SetMaterialPropertyVisibility("roughness", mainVisibility)
    context:SetMaterialPropertyVisibility("roughnessAmount", mainVisibility)
    context:SetMaterialPropertyVisibility("roughnessMap", mainVisibility)
    context:SetMaterialPropertyVisibility("roughnessMapUv", mainVisibility)
    context:SetMaterialPropertyVisibility("roughnessMapUvScale", mainVisibility)
    context:SetMaterialPropertyVisibility("useRoughnessMap", mainVisibility)

    if(enable) then
        UpdateTextureDependentPropertyVisibility(context, "factor",    "factorMap",     "useFactorMap",    "factorMapUv",    "factorMapUvScale")
        UpdateTextureDependentPropertyVisibility(context, "roughness", "roughnessMap",  "useRoughnessMap", "roughnessMapUv", "roughnessMapUvScale")
    end
end
