/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#if !defined(Q_MOC_RUN)
#include <QWidget>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>
#include <Source/Editor/EditorCommon.h>
#endif

namespace Ui
{
    class FlipbookSettingWidget;
}

namespace ImageProcessingAtomEditor
{
    //! Qt widget rendered inside the Texture Settings editor panel when the selected texture is detected as a flipbook (sprite sheet).
    //! Exposes rows/columns and frame-rate fields.
    class FlipbookSettingWidget
        : public QWidget
        , public AzToolsFramework::IPropertyEditorNotify
        , protected EditorInternalNotificationBus::Handler
    {
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(FlipbookSettingWidget, AZ::SystemAllocator);
        explicit FlipbookSettingWidget(EditorTextureSetting& textureSetting, QWidget* parent = nullptr);
        ~FlipbookSettingWidget();

        // IPropertyEditorNotify
        void BeforePropertyModified([[maybe_unused]] AzToolsFramework::InstanceDataNode*) override {}
        //! Propagates the changed flipbook property back to EditorTextureSetting and triggers an asset rebuild.
        void AfterPropertyModified(AzToolsFramework::InstanceDataNode* node) override;
        void SetPropertyEditingActive([[maybe_unused]] AzToolsFramework::InstanceDataNode*) override {}
        void SetPropertyEditingComplete([[maybe_unused]] AzToolsFramework::InstanceDataNode*) override {}
        void SealUndoStack() override {}

    protected:
        ////////////////////////////////////////////////////////////////////////
        // EditorInternalNotificationBus
        //! Refreshes the widget if the platform selection changes (flipbook columns/rows can differ per platform).
        void OnEditorSettingsChanged(bool needRefresh, const AZStd::string& platform) override;
        ////////////////////////////////////////////////////////////////////////

    private:
        QScopedPointer<Ui::FlipbookSettingWidget> m_ui;
        EditorTextureSetting* m_textureSetting; // non-owning pointer to the parent EditorTextureSetting; valid for the lifetime of the panel
    };
} //namespace ImageProcessingAtomEditor
