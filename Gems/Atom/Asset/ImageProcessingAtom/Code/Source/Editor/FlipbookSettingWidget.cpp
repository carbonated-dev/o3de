/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "FlipbookSettingWidget.h"
#include <Source/Editor/ui_FlipbookSettingWidget.h>

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzToolsFramework/UI/PropertyEditor/ReflectedPropertyEditor.hxx>

namespace ImageProcessingAtomEditor
{
    using namespace ImageProcessingAtom;

    FlipbookSettingWidget::FlipbookSettingWidget(EditorTextureSetting& textureSetting, QWidget* parent /*= nullptr*/)
        : QWidget(parent)
        , m_ui(new Ui::FlipbookSettingWidget)
        , m_textureSetting(&textureSetting)
    {
        m_ui->setupUi(this);

        AZ::SerializeContext* serializeContext = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(serializeContext, &AZ::ComponentApplicationBus::Events::GetSerializeContext);
        AZ_Assert(serializeContext, "Serialization context not available");

        m_ui->propertyEditor->SetAutoResizeLabels(true);
        m_ui->propertyEditor->Setup(serializeContext, this, true, 250);
        m_ui->propertyEditor->ClearInstances();

        TextureSettings::FlipbookSettings* flipbookSettings = &m_textureSetting->GetMultiplatformTextureSetting().m_flipBookSettings;
        m_ui->propertyEditor->AddInstance(flipbookSettings, AZ::SerializeTypeInfo<TextureSettings::FlipbookSettings>::GetUuid());
        m_ui->propertyEditor->InvalidateAll();
        m_ui->propertyEditor->ExpandAll();

        EditorInternalNotificationBus::Handler::BusConnect();
    }

    FlipbookSettingWidget::~FlipbookSettingWidget()
    {
        EditorInternalNotificationBus::Handler::BusDisconnect();
    }

    void FlipbookSettingWidget::AfterPropertyModified(AzToolsFramework::InstanceDataNode* /*node*/)
    {
        m_textureSetting->PropagateCommonSettings();
        m_ui->propertyEditor->InvalidateValues();
        EditorInternalNotificationBus::Broadcast(&EditorInternalNotificationBus::Events::OnEditorSettingsChanged, false, BuilderSettingManager::s_defaultPlatform);
    }

    void FlipbookSettingWidget::OnEditorSettingsChanged(bool needRefresh, const AZStd::string& /*platform*/)
    {
        if (needRefresh)
        {
            m_ui->propertyEditor->InvalidateValues();
        }
    }
}//namespace ImageProcessingAtomEditor
#include <Source/Editor/moc_FlipbookSettingWidget.cpp>
