/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

// include the required headers
#include "CommandSystemConfig.h"
#include <MCore/Source/Command.h>
#include <EMotionFX/Source/Importer/Importer.h>
#include "CommandManager.h"


namespace CommandSystem
{

#if defined(CARBONATED) && defined(CARBONATED_EMOTIONFX_PREFAB_SYSTEM)
    MCORE_DEFINECOMMAND(CommandImportWeaponPrefab, "ImportWeaponPrefab", "Import weapon prefab", false)
    MCORE_DEFINECOMMAND(CommandRemoveWeaponPrefab, "RemoveWeaponPrefab", "Remove weapon prefab", false)
    MCORE_DEFINECOMMAND(CommandPrefabLoaded, "PrefabLoaded", "Prefab loaded", false)
    // add prefab
    MCORE_DEFINECOMMAND_START(CommandImportPrefab, "Import prefab", true)
public:
    uint32 m_previouslyUsedId;
    bool m_oldWorkspaceDirtyFlag;
    MCORE_DEFINECOMMAND_END
#endif

    // add actor
    MCORE_DEFINECOMMAND_START(CommandImportActor, "Import actor", true)
public:
    uint32  m_previouslyUsedId;
    uint32  m_oldIndex;
    bool    m_oldWorkspaceDirtyFlag;
    MCORE_DEFINECOMMAND_END

    // add motion
        MCORE_DEFINECOMMAND_START(CommandImportMotion, "Import motion", true)
public:
    uint32          m_oldMotionId;
    AZStd::string   m_oldFileName;
    bool            m_oldWorkspaceDirtyFlag;
    MCORE_DEFINECOMMAND_END

} // namespace CommandSystem
