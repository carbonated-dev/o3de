/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <VolumetricFog/EditorVolumetricFogComponent.h>
#include <AtomLyIntegration/CommonFeatures/VolumetricFog/VolumetricFogComponentConfig.h>

namespace AZ::Render
{
    void EditorVolumetricFogComponent::Reflect(AZ::ReflectContext* context)
    {
        BaseClass::Reflect(context);

        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EditorVolumetricFogComponent, BaseClass>()
                ->Version(1);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<EditorVolumetricFogComponent>(
                    "Volumetric Fog", "Volumetric fog component that renders 3D fog")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Graphics/Environment")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Icons/Components/Component_Placeholder.svg")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Icons/Components/Viewport/Component_Placeholder.svg")
                    ->Attribute(
                        Edit::Attributes::AppearsInAddComponentMenu,
                        AZStd::vector<AZ::Crc32>({ AZ_CRC("Level", 0x9aeacc13), AZ_CRC("Game", 0x232b318c) }))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ;

                editContext->Class<VolumetricFogComponentController>(
                    "VolumetricFogComponentController", "")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &VolumetricFogComponentController::m_configuration, "Configuration", "")
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
                    ;

                editContext->Class<VolumetricFogComponentConfig>(
                    "VolumetricFogComponentConfig", "")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)

                    ->DataElement(Edit::UIHandlers::CheckBox,
                            &VolumetricFogComponentConfig::m_enabled,
                            "Enable Volumetric Fog",
                            "Enable Volumetric Fog.")
                            ->Attribute(Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)

                    ->DataElement(
                        Edit::UIHandlers::ComboBox,
                        &VolumetricFogComponentConfig::m_quality,
                        "Quality",
                        "The number of froxels to use (16x16, 8x8 or 4x4).")
                        ->Attribute(AZ::Edit::Attributes::EnumValues, AZ::Edit::GetEnumConstantsFromTraits<Render::VolumetricFogQuality>())

                    ->ClassElement(Edit::ClassElements::Group, "Distance")
                        ->Attribute(Edit::Attributes::AutoExpand, true)

                    ->DataElement(
                        AZ::Edit::UIHandlers::Slider,
                        &VolumetricFogComponentConfig::m_fogNear,
                        "Start Distance", "The distance from the viewer when the fog starts")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                        ->Attribute(AZ::Edit::Attributes::Max, 5000.0f)
                        ->Attribute(AZ::Edit::Attributes::SoftMin, 0.0f)
                        ->Attribute(AZ::Edit::Attributes::SoftMax, 10.0f)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)

                    ->DataElement(
                        AZ::Edit::UIHandlers::Slider,
                        &VolumetricFogComponentConfig::m_fogFar,
                        "End Distance", "At what distance from the viewer does the fog take over and mask the background scene out.")
                    ->Attribute(AZ::Edit::Attributes::Min, &VolumetricFogComponentConfig::m_fogNear)
                        ->Attribute(AZ::Edit::Attributes::Max, 5000.0f)
                        ->Attribute(AZ::Edit::Attributes::SoftMin, 0.0f)
                        ->Attribute(AZ::Edit::Attributes::SoftMax, 500.0f)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)

#undef CONFIG_COMPONENT_NAME
#define CONFIG_COMPONENT_NAME VolumetricFogComponentConfig
#include <VolumetricFog//EditorVolumetricFogVolumeComponent.inl>
                    
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &VolumetricFogComponentConfig::m_lightingChannelConfig, "Lighting Channels", "")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ::Edit::PropertyRefreshLevels::AttributesAndValues)

                    ->ClassElement(Edit::ClassElements::Group, "Temporal Reprojection")
                        ->Attribute(Edit::Attributes::AutoExpand, false)

                    ->DataElement(
                        AZ::Edit::UIHandlers::Slider,
                        &VolumetricFogComponentConfig::m_sequenceLength,
                        "Sequence Length",
                        "Number of frames to create different jitter positions")
                        ->Attribute(AZ::Edit::Attributes::Min, 0)
                        ->Attribute(AZ::Edit::Attributes::Max, VolumetricFogMaxSequenceLength)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)

                    ->DataElement(
                        AZ::Edit::UIHandlers::Slider,
                        &VolumetricFogComponentConfig::m_blendPercentage,
                        "Blend Percentage",
                        "How much of the last frame to blend")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                        ->Attribute(AZ::Edit::Attributes::Max, 100.0f)
                        ->Attribute(AZ::Edit::Attributes::SoftMin, 0.0f)
                        ->Attribute(AZ::Edit::Attributes::SoftMax, 10.0f)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)
                    ;
            }
        }

        if (auto* behaviorContext = azrtti_cast<BehaviorContext*>(context))
        {
            behaviorContext->Class<EditorVolumetricFogComponent>()
                ->RequestBus("VolumetricFogRequestsBus");
        }
    }

    EditorVolumetricFogComponent::EditorVolumetricFogComponent(const VolumetricFogComponentConfig& config)
        : BaseClass(config)
    {
    }

    AZ::u32 EditorVolumetricFogComponent::OnConfigurationChanged()
    {
        m_controller.OnConfigChanged();
        return Edit::PropertyRefreshLevels::AttributesAndValues;
    }

    void EditorVolumetricFogComponent::OnEntityVisibilityChanged(bool visibility)
    {
        if(auto featureProcessor = m_controller.m_featureProcessorInterface)
        {
            AZ_UNUSED(visibility);
        }
    }
}
