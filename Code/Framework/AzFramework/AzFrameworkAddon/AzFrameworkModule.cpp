/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Module/Module.h>

#include "AzFramework/Network/NetBindingComponent.h"
#include "AzFramework/Network/NetBindingSystemComponent.h"
#include "AzFramework/Network/InterestManagerComponent.h"
#include "AzFramework/TargetManagement/TargetManagementComponent.h"

namespace Legacy
{
    class AzFrameworkAddonModule
        : public AZ::Module
    {
    public:
        AZ_RTTI(AzFrameworkAddonModule, "{35A99BC2-1207-11EE-BE56-0242AC120002}", AZ::Module);

        AzFrameworkAddonModule()
            : AZ::Module()
        {
            m_descriptors.insert(m_descriptors.end(), {
            AzFramework::NetBindingComponent::CreateDescriptor(),
            AzFramework::NetBindingSystemComponent::CreateDescriptor(),
            AzFramework::InterestManagerComponent::CreateDescriptor(),
            AzFramework::TargetManagementComponent::CreateDescriptor(),
            });
        }

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList
            {
                azrtti_typeid<NetBindingSystemComponent>(),
            };
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(Legacy_AzFrameworkAddon, Legacy::AzFrameworkAddonModule)
