/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <QModelIndex>

#include <AzCore/Slice/SliceComponent.h>
#include <AzCore/EBus/EBus.h>

namespace AzToolsFramework
{
    //! Bus to request UI operations on the Entity Outliner widget.
    class EntityOutlinerRequests : public AZ::EBusTraits
    {
    public:
        //! Request to trigger the rename workflow on the Entity Outliner for the entity provided.
        virtual void TriggerRenameEntityUi(const AZ::EntityId& entityId) = 0;

#if defined(CARBONATED) // Add "Collapse outliner tree view" action
        //! Request to collapse the tree view on the Entity Outliner, except for the entity provided.
        virtual void CollapseTreeView(const AZ::EntityId& entityId) = 0;
#endif
    };
    using EntityOutlinerRequestBus = AZ::EBus<EntityOutlinerRequests>;

} // namespace AzToolsFramework
