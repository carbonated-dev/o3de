/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <VolumetricFog/EditorFogVolumeComponent.h>
#include <AtomLyIntegration/CommonFeatures/VolumetricFog/FogVolumeComponentConfig.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzToolsFramework/API/EntityCompositionRequestBus.h>
#include <LmbrCentral/Shape/EditorShapeComponentBus.h>
#include <LmbrCentral/Shape/SphereShapeComponentBus.h>
#include <LmbrCentral/Shape/BoxShapeComponentBus.h>

namespace AZ::Render
{
    EditorFogVolumeComponent::EditorFogVolumeComponent(const FogVolumeComponentConfig& config)
        : BaseClass(config)
    {
    }

    void EditorFogVolumeComponent::Reflect(AZ::ReflectContext* context)
    {
        BaseClass::Reflect(context);

        if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
        {
            sc->Class<EditorFogVolumeComponent, BaseClass>()
                ->Version(1);

            if (AZ::EditContext* ec = sc->GetEditContext())
            {
                ec->Class<EditorFogVolumeComponent>(
                    "Fog Volume",
                    "A local volume that modifies the volumetric fog medium within its bounds. "
                    "Requires a Box or Sphere shape component on the same entity.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Graphics/Environment")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Icons/Components/Component_Placeholder.svg")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Icons/Components/Viewport/Component_Placeholder.svg")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu,
                            AZStd::vector<AZ::Crc32>({ AZ_CRC_CE("Game") }))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ;

                ec->Class<FogVolumeComponentController>("FogVolumeComponentController", "")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default,
                        &FogVolumeComponentController::m_config,
                        "Configuration", "")
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
                    ;

                ec->Class<FogVolumeComponentConfig>("FogVolumeComponentConfig", "")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)

                    ->DataElement(Edit::UIHandlers::ComboBox, &FogVolumeComponentConfig::m_shape, "Shape", "Shape of the volume.")
                        ->EnumAttribute(FogVolumeShape::Unknown, "Choose a Shape type")
                        ->EnumAttribute(FogVolumeShape::Box, "Box")
                        ->EnumAttribute(FogVolumeShape::Sphere, "Sphere")

                    // ---- Blend mode ---------------------------------------------
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Blend")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)

                    ->DataElement(AZ::Edit::UIHandlers::ComboBox,
                        &FogVolumeComponentConfig::m_blendMode,
                        "Blend Mode",
                        "How this volume's contribution is composited with existing froxel data.\n"
                        "  Additive  — adds on top of existing fog\n"
                        "  Multiply  — multiplies into existing fog (darkens/thins)\n"
                        "  Overwrite — replaces fog inside the volume entirely\n"
                        "  Min / Max — take per-froxel minimum or maximum")
                        ->Attribute(AZ::Edit::Attributes::EnumValues, AZ::Edit::GetEnumConstantsFromTraits<Render::FogVolumeBlendMode>())
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ::Edit::PropertyRefreshLevels::ValuesOnly)

                    ->DataElement(AZ::Edit::UIHandlers::SpinBox,
                        &FogVolumeComponentConfig::m_priority,
                        "Priority",
                        "Volumes with higher priority are applied last. Matters most for Overwrite blend mode.")
                        ->Attribute(AZ::Edit::Attributes::Min, 0)
                        ->Attribute(AZ::Edit::Attributes::Max, 255)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ::Edit::PropertyRefreshLevels::ValuesOnly)

                    // ---- Edge fade ----------------------------------------------
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Edge Fade")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)

                    ->DataElement(AZ::Edit::UIHandlers::Vector3,
                        &FogVolumeComponentConfig::m_edgeFade,
                        "Edge Fade",
                        "Per-axis fade fraction [0,1]. 0 = hard edge, 1 = fades to zero at center.\n"
                        "For sphere volumes only the X component is used.")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                        ->Attribute(AZ::Edit::Attributes::Max, 1.0f)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ::Edit::PropertyRefreshLevels::ValuesOnly)

#undef CONFIG_COMPONENT_NAME
#define CONFIG_COMPONENT_NAME FogVolumeComponentConfig
#include <VolumetricFog//EditorVolumetricFogVolumeComponent.inl>

                    ;
            }
        }

        if (auto* bc = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            bc->Class<EditorFogVolumeComponent>()
                ->RequestBus("FogVolumeRequestsBus");
        }
    }

    void EditorFogVolumeComponent::Activate()
    {
        // Override the shape component so that this component controls the color.
        LmbrCentral::EditorShapeComponentRequestsBus::Event(
            GetEntityId(), &LmbrCentral::EditorShapeComponentRequests::SetShapeColorIsEditable, false);
        LmbrCentral::EditorShapeComponentRequestsBus::Event(
            GetEntityId(), &LmbrCentral::EditorShapeComponentRequests::SetShapeColor, AZ::Color(m_controller.GetFogAlbedo()));

        BaseClass::Activate();
    }

    void EditorFogVolumeComponent::Deactivate()
    {
        LmbrCentral::EditorShapeComponentRequestsBus::Event(
            GetEntityId(), &LmbrCentral::EditorShapeComponentRequests::SetShapeColorIsEditable, true);

        BaseClass::Deactivate();
    }

    AZ::u32 EditorFogVolumeComponent::OnConfigurationChanged()
    {
        bool needsFullRefresh = HandleShapeTypeChange();
        m_controller.OnConfigChanged();
        LmbrCentral::EditorShapeComponentRequestsBus::Event(
            GetEntityId(), &LmbrCentral::EditorShapeComponentRequests::SetShapeColor, AZ::Color(m_controller.GetFogAlbedo()));
        LmbrCentral::EditorShapeComponentRequestsBus::Event(
            GetEntityId(), &LmbrCentral::EditorShapeComponentRequests::SetShapeWireframeColor,  AZ::Color(m_controller.GetFogAlbedo()));

        if (needsFullRefresh)
        {
            return Edit::PropertyRefreshLevels::EntireTree;
        }
        return AZ::Edit::PropertyRefreshLevels::AttributesAndValues;
    }

     bool EditorFogVolumeComponent::HandleShapeTypeChange()
    {
         if (m_shape == FogVolumeShape::Unknown)
         {
            // Shape is unknown, see if it can be determined from a shape component.
            Crc32 shapeType = Crc32(0);
            LmbrCentral::ShapeComponentRequestsBus::EventResult(
                shapeType, GetEntityId(), &LmbrCentral::ShapeComponentRequestsBus::Events::GetShapeType);

            constexpr Crc32 SphereShapeTypeId = AZ_CRC_CE("Sphere");
            constexpr Crc32 BoxShapeTypeId = AZ_CRC_CE("Box");

            switch (shapeType)
            {
            case SphereShapeTypeId:
                m_shape = FogVolumeShape::Sphere;
                break;
            case BoxShapeTypeId:
                m_shape = FogVolumeShape::Box;
                break;
            default:
                break; // Shape type can't be deduced.
            }
        }

        if (m_shape == m_controller.GetShape())
        {
            // No change, nothing to do
            return false;
        }

        // Update the cached light type.
        m_shape = m_controller.GetShape();

        // components may be removed or added here, so deactivate now and reactivate the entity when everything is done shifting around.
        GetEntity()->Deactivate();

        // Check if there is already a shape components and remove it.
        for (Component* component : GetEntity()->GetComponents())
        {
            ComponentDescriptor::DependencyArrayType provided;
            ComponentDescriptor* componentDescriptor = nullptr;
            ComponentDescriptorBus::EventResult(
                componentDescriptor, component->RTTI_GetType(), &ComponentDescriptorBus::Events::GetDescriptor);
            AZ_Assert(
                componentDescriptor,
                "Component class %s descriptor is not created! It must be before you can use it!",
                component->RTTI_GetTypeName());
            componentDescriptor->GetProvidedServices(provided, component);
            auto providedItr = AZStd::find(provided.begin(), provided.end(), AZ_CRC_CE("ShapeService"));
            if (providedItr != provided.end())
            {
                AZStd::vector<Component*> componentsToRemove = { component };
                AzToolsFramework::EntityCompositionRequests::RemoveComponentsOutcome outcome =
                    AZ::Failure(AZStd::string("Failed to remove old shape component."));
                AzToolsFramework::EntityCompositionRequestBus::BroadcastResult(
                    outcome, &AzToolsFramework::EntityCompositionRequests::RemoveComponents, componentsToRemove);
                break;
            }
        }

        // Add a new shape component for light types that require it.

        auto addComponentOfType = [&](AZ::Uuid type)
        {
            AzToolsFramework::EntityCompositionRequests::AddComponentsOutcome outcome =
                AZ::Failure(AZStd::string("Failed to add shape component for light type"));

            const AZStd::vector<AZ::EntityId> entityList = { GetEntityId() };

            AzToolsFramework::EntityCompositionRequestBus::BroadcastResult(
                outcome, &AzToolsFramework::EntityCompositionRequests::AddComponentsToEntities, entityList, ComponentTypeList({ type }));
        };

        switch (m_shape)
        {
        case FogVolumeShape::Sphere:
            addComponentOfType(LmbrCentral::EditorSphereShapeComponentTypeId);
            break;
        case FogVolumeShape::Box:
            addComponentOfType(LmbrCentral::EditorBoxShapeComponentTypeId);
            break;
        default:
            break;
        }

        GetEntity()->Activate();

        // Set more reasonable default values for certain shapes.
        switch (m_shape)
        {
        case FogVolumeShape::Sphere:
            LmbrCentral::SphereShapeComponentRequestsBus::Event(
                GetEntityId(), &LmbrCentral::SphereShapeComponentRequests::SetRadius, 1.0f);
            break;
        case FogVolumeShape::Box:
            LmbrCentral::BoxShapeComponentRequestsBus::Event(
                GetEntityId(), &LmbrCentral::BoxShapeComponentRequests::SetBoxDimensions, AZ::Vector3(1.0f, 1.0f, 1.0f));
            break;
        }

        return true;
    }

} // namespace AZ::Render
