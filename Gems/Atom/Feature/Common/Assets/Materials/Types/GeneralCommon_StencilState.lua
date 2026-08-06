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

 
function GetMaterialPropertyDependencies()
    return {
        "blockSilhouette",
        "blockColorGrading"
    }
end

function Process(context)
    local tags = {"forward", "forward_customZ"}
    for _, tag in ipairs(tags) do
        if(context:HasShaderWithTag(tag)) then
            local shaderItem = context:GetShaderByTag(tag)
            local blockSilhouette = false
            local blockColorGrading = false
            if(context:HasMaterialProperty("blockSilhouette")) then
                blockSilhouette = context:GetMaterialPropertyValue_bool("blockSilhouette")
            end

            if(context:HasMaterialProperty("blockColorGrading")) then
                blockColorGrading = context:GetMaterialPropertyValue_bool("blockColorGrading")
            end

            local stencilMask = 0
            if(blockSilhouette) then
                stencilMask = stencilMask | 0x40
            end

            if(blockColorGrading) then
                Debug.Log("blockColorGrading es true")
                stencilMask = stencilMask | 0x20
            else
                Debug.Log("blockColorGrading es false")
            end
            Debug.Log("stencilMask es " .. stencilMask)
            shaderItem:SetStencilRefOverride(stencilMask)
        end
    end
end
