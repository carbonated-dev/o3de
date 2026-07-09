--------------------------------------------------------------------------------------
--
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
--
----------------------------------------------------------------------------------------------------

function GetMaterialPropertyDependencies()
    return {"mode"}
end

function Process(context)
end

function ProcessEditor(context)
    local mode = context:GetMaterialPropertyValue_enum("mode")
    local modeOff = 0
    local visibility
    if(mode == modeOff) then
        visibility = MaterialPropertyVisibility_Hidden
    else
        visibility = MaterialPropertyVisibility_Enabled
    end

    context:SetMaterialPropertyVisibility("tint", visibility)
    context:SetMaterialPropertyVisibility("intensity", visibility)
    context:SetMaterialPropertyVisibility("power", visibility)
end
