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
    return {"silhouetteType"}
end

SilhouetteMode_AlwaysDraw = 0
SilhouetteMode_Visible = 1
SilhouetteMode_XRay = 2
SilhouetteMode_NeverDraw = 3

function ProcessEditor(context)
    local silhouetteMode = context:GetMaterialPropertyValue_enum("silhouetteType")
    local mainVisibility
    if (silhouetteMode == SilhouetteMode_NeverDraw) then
        mainVisibility = MaterialPropertyVisibility_Hidden
    else
        mainVisibility = MaterialPropertyVisibility_Enabled
    end

    context:SetMaterialPropertyVisibility("color", mainVisibility)
    context:SetMaterialPropertyVisibility("alpha", mainVisibility)
    context:SetMaterialPropertyVisibility("depthBias", mainVisibility)
    context:SetMaterialPropertyVisibility("outlineSize", mainVisibility)
    context:SetMaterialPropertyVisibility("outlineOnly", mainVisibility)    
end
