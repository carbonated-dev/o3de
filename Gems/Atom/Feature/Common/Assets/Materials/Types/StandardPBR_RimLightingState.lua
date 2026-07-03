--------------------------------------------------------------------------------------
--
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
--
----------------------------------------------------------------------------------------------------

function GetMaterialPropertyDependencies()
    return {"enable"}
end

function Process(context)
end

function ProcessEditor(context)
    local enable = context:GetMaterialPropertyValue_bool("enable")

    local visibility
    if(enable) then
        visibility = MaterialPropertyVisibility_Enabled
    else
        visibility = MaterialPropertyVisibility_Hidden
    end

    context:SetMaterialPropertyVisibility("color", visibility)
    context:SetMaterialPropertyVisibility("intensity", visibility)
    context:SetMaterialPropertyVisibility("power", visibility)
    context:SetMaterialPropertyVisibility("bias", visibility)
end
