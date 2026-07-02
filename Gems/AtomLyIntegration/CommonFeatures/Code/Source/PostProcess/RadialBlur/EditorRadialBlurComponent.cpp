/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <PostProcess/RadialBlur/EditorRadialBlurComponent.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>

namespace AZ
{
    namespace Render
    {
        void EditorRadialBlurComponent::Reflect(AZ::ReflectContext* context)
        {
            BaseClass::Reflect(context);

            if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serializeContext->Class<EditorRadialBlurComponent, BaseClass>()->Version(0);

                if (AZ::EditContext* editContext = serializeContext->GetEditContext())
                {
                    editContext->Class<EditorRadialBlurComponent>("Radial Blur", "Controls the Radial Blur post-processing effect")
                        ->ClassElement(Edit::ClassElements::EditorData, "")
                        ->Attribute(Edit::Attributes::Category, "Graphics/PostFX")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Icons/Components/Component_Placeholder.svg")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Icons/Components/Viewport/Component_Placeholder.svg")
                        ->Attribute(Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game", 0x232b318c))
                        ->Attribute(Edit::Attributes::AutoExpand, true)
                        ->Attribute(Edit::Attributes::HelpPageURL, "https://o3de.org/docs/user-guide/components/reference/atom/RadialBlur/");

                    editContext->Class<RadialBlurComponentController>("RadialBlurComponentController", "")
                        ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->DataElement(AZ::Edit::UIHandlers::Default, &RadialBlurComponentController::m_configuration, "Configuration", "")
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);

                    editContext->Class<RadialBlurComponentConfig>("RadialBlurComponentConfig", "")
                        ->DataElement(Edit::UIHandlers::CheckBox, &RadialBlurComponentConfig::m_enabled, "Enable Radial Blur", "Enable Radial Blur.")
                        ->Attribute(Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)

                        ->DataElement(Edit::UIHandlers::Default, &RadialBlurComponentConfig::m_center, "Center", "Normalized screen position for the blur center.")
                        ->Attribute(Edit::Attributes::Min, 0.0f)
                        ->Attribute(Edit::Attributes::Max, 1.0f)
                        ->Attribute(Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)
                        ->Attribute(Edit::Attributes::ReadOnly, &RadialBlurComponentConfig::ArePropertiesReadOnly)

                        ->DataElement(Edit::UIHandlers::Slider, &RadialBlurComponentConfig::m_amount, "Amount", "Amount of radial blur.")
                        ->Attribute(Edit::Attributes::Min, 0.0f)
                        ->Attribute(Edit::Attributes::Max, 1.0f)
                        ->Attribute(Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)
                        ->Attribute(Edit::Attributes::ReadOnly, &RadialBlurComponentConfig::ArePropertiesReadOnly)

                        ->DataElement(Edit::UIHandlers::Slider, &RadialBlurComponentConfig::m_innerRadius, "Inner Radius", "Unaffected distance from center as a percentage.")
                        ->Attribute(Edit::Attributes::Min, 0.0f)
                        ->Attribute(Edit::Attributes::Max, 1.0f)
                        ->Attribute(Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)
                        ->Attribute(Edit::Attributes::ReadOnly, &RadialBlurComponentConfig::ArePropertiesReadOnly)

                        ->DataElement(Edit::UIHandlers::ComboBox, &RadialBlurComponentConfig::m_sampleCount, "Sample Count", "Number of radial blur samples.")
                        ->Attribute("EnumValues", &RadialBlurComponentConfig::GetSampleCountOptions)
                        ->Attribute(Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)
                        ->Attribute(Edit::Attributes::ReadOnly, &RadialBlurComponentConfig::ArePropertiesReadOnly)

                        ->ClassElement(AZ::Edit::ClassElements::Group, "Overrides")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, false)

#define EDITOR_CLASS RadialBlurComponentConfig
#include <Atom/Feature/ParamMacros/StartOverrideEditorContext.inl>
#include <Atom/Feature/PostProcess/RadialBlur/RadialBlurParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
#undef EDITOR_CLASS
                        ;
                }
            }

            if (auto behaviorContext = azrtti_cast<BehaviorContext*>(context))
            {
                behaviorContext->Class<EditorRadialBlurComponent>()->RequestBus("RadialBlurRequestBus");

                behaviorContext
                    ->ConstantProperty("EditorRadialBlurComponentTypeId", BehaviorConstant(Uuid(RadialBlur::EditorRadialBlurComponentTypeId)))
                    ->Attribute(AZ::Script::Attributes::Module, "render")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation);
            }
        }

        EditorRadialBlurComponent::EditorRadialBlurComponent(const RadialBlurComponentConfig& config)
            : BaseClass(config)
        {
        }

        u32 EditorRadialBlurComponent::OnConfigurationChanged()
        {
            m_controller.OnConfigChanged();
            return Edit::PropertyRefreshLevels::AttributesAndValues;
        }
    } // namespace Render
} // namespace AZ
