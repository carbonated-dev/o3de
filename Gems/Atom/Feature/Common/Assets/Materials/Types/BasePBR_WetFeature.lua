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
    return { "useWet", "textureMap", "useTexture" }
end

function GetShaderOptionDependencies()
    return { "o_useWet", "o_wet_useTexture" }
end

function Process(context)
    local useWet = context:GetMaterialPropertyValue_bool("useWet")
    context:SetShaderOptionValue_bool("o_useWet", useWet)

    local textureMap = context:GetMaterialPropertyValue_Image("textureMap")
    local useTexture = context:GetMaterialPropertyValue_bool("useTexture")
    context:SetShaderOptionValue_bool("o_wet_useTexture", useTexture and textureMap ~= nil)
end

function ProcessEditor(context)
    local textureMap = context:GetMaterialPropertyValue_Image("textureMap")
    local useTexture = context:GetMaterialPropertyValue_bool("useTexture")

    if(nil == textureMap) then
        context:SetMaterialPropertyVisibility("useTexture", MaterialPropertyVisibility_Hidden)
        context:SetMaterialPropertyVisibility("textureMapUv", MaterialPropertyVisibility_Hidden)
        context:SetMaterialPropertyVisibility("wetFactor", MaterialPropertyVisibility_Enabled)
    elseif(not useTexture) then
        context:SetMaterialPropertyVisibility("useTexture", MaterialPropertyVisibility_Enabled)
        context:SetMaterialPropertyVisibility("textureMapUv", MaterialPropertyVisibility_Disabled)
        context:SetMaterialPropertyVisibility("wetFactor", MaterialPropertyVisibility_Enabled)
    else
        context:SetMaterialPropertyVisibility("useTexture", MaterialPropertyVisibility_Enabled)
        context:SetMaterialPropertyVisibility("textureMapUv", MaterialPropertyVisibility_Enabled)
        context:SetMaterialPropertyVisibility("wetFactor", MaterialPropertyVisibility_Hidden)
    end
end
