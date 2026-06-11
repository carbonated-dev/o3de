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
        "silhouetteType"
        }
end

local SilhouetteType_AlwaysDraw = 0
local SilhouetteType_Visible = 1
local SilhouetteType_XRay = 2
local SilhouetteType_NeverDraw = 3

local ComparisonFunc_Never = 0
local ComparisonFunc_Less = 1
local ComparisonFunc_Equal = 2
local ComparisonFunc_LessEqual = 3
local ComparisonFunc_Greater = 4
local ComparisonFunc_NotEqual = 5
local ComparisonFunc_GreaterEqual = 6
local ComparisonFunc_Always = 7

function Process(context)
    -- CARBONATED Begin
    if(context:HasShaderWithTag("silhouetteGather")) then
    -- CARBONATED End
        local silhouetteType = SilhouetteType_NeverDraw;
        local shaderItem = context:GetShaderByTag("silhouetteGather")
        -- CARBONATED Begin
        if(context:HasMaterialProperty("silhouetteType")) then
            silhouetteType = context:GetMaterialPropertyValue_enum("silhouetteType")
        end

        if(silhouetteType == SilhouetteType_NeverDraw) then
            shaderItem:SetEnabled(false)
        else

            shaderItem:SetEnabled(true)
            shaderItem:SetStencilRefOverride(0xFF)
        -- CARBONATED End
            if(silhouetteType == SilhouetteType_AlwaysDraw) then
                -- Always draws in ignores depth check, but won't draw where silhouettes are blocked
                shaderItem:GetRenderStatesOverride():SetDepthEnabled(false)
                shaderItem:GetRenderStatesOverride():SetDepthComparisonFunc(ComparisonFunc_Never)
                -- if you want to let the full silhouette draw even on top of meshes that block
                -- silhouettes, you can disable the stencil check using SetStencilEnabled()
                -- e.g. shaderItem:GetRenderStatesOverride():SetStencilEnabled(false)
            elseif(silhouetteType == SilhouetteType_Visible) then
                -- Visible draws where the silhouette is NOT obscured
                shaderItem:GetRenderStatesOverride():SetDepthEnabled(true)
                shaderItem:GetRenderStatesOverride():SetDepthComparisonFunc(ComparisonFunc_GreaterEqual)
            elseif(silhouetteType == SilhouetteType_XRay) then
                -- XRay draws where the silhouette IS obscured
                shaderItem:GetRenderStatesOverride():SetDepthEnabled(true)
                shaderItem:GetRenderStatesOverride():SetDepthComparisonFunc(ComparisonFunc_Less)
            elseif(silhouetteType == SilhouetteType_NeverDraw) then
                -- Never doesn't draw the silhouette
                shaderItem:GetRenderStatesOverride():SetDepthEnabled(true)
                shaderItem:GetRenderStatesOverride():SetDepthComparisonFunc(ComparisonFunc_Never)
            end
        end
    end
end
