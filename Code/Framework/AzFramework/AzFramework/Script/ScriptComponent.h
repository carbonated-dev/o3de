/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/Script/ScriptAsset.h>
#include <AzCore/Script/ScriptContext.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Serialization/DynamicSerializableField.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/smart_ptr/intrusive_ptr.h>

// carbonated begin (akostin/mp226-2): Add NetBindable to ScriptComponent
#if defined(CARBONATED)
#include <AzFramework/Network/NetBindable.h>
#endif
// carbonated end

namespace AZ
{
    class ScriptProperty;
}

namespace AzToolsFramework
{
    namespace Components
    {
        class ScriptEditorComponent;
    }
}

namespace AzFramework
{
    struct ScriptCompileRequest;

    // carbonated begin (akostin/mp226-2): Add NetBindable to ScriptComponent
#if defined(CARBONATED)
    class ScriptNetBindingTable;
#endif
    // carbonated end

    struct ScriptCompileRequest
    {
        AZStd::string_view m_errorWindow;
        AZStd::string_view m_sourceFile;
        AZStd::string_view m_fullPath;
        AZStd::string_view m_fileName;
        AZStd::string_view m_tempDirPath;        
        AZ::IO::GenericStream* m_input = nullptr;
        AZStd::string m_destFileName;
        AZStd::string m_destPath;
        AZ::LuaScriptData m_luaScriptDataOut;
    };

    void ConstructScriptAssetPaths(ScriptCompileRequest& request);
    AZ::Outcome<void, AZStd::string> CompileScript(ScriptCompileRequest& request);
    AZ::Outcome<void, AZStd::string> CompileScriptAndAsset(ScriptCompileRequest& request);
    AZ::Outcome<void, AZStd::string> CompileScript(ScriptCompileRequest& request, AZ::ScriptContext& context);
    AZ::Outcome<AZStd::string, AZStd::string> CompileScriptAndSaveAsset(ScriptCompileRequest& request);
    bool SaveLuaAssetData(const AZ::LuaScriptData& data, AZ::IO::GenericStream& stream);

    struct ScriptPropertyGroup
    {
        AZ_TYPE_INFO(ScriptPropertyGroup, "{79682522-2f81-4b36-9fc2-a091c7504f7f}");
        AZStd::string                       m_name;
        AZStd::vector<AZ::ScriptProperty*>  m_properties;
        AZStd::vector<ScriptPropertyGroup>  m_groups;

        // Get the pointer to the specified group in m_groups. Returns nullptr if not found.
        ScriptPropertyGroup* GetGroup(const char* groupName);
        // Get the pointer to the specified property in m_properties. Returns nullptr if not found.
        AZ::ScriptProperty* GetProperty(const char* propertyName);
        // Remove all properties and groups
        void Clear();

        ScriptPropertyGroup() = default;
        ~ScriptPropertyGroup();

        ScriptPropertyGroup(const ScriptPropertyGroup& rhs) = delete;
        ScriptPropertyGroup& operator=(ScriptPropertyGroup&) = delete;
    public:
        ScriptPropertyGroup(ScriptPropertyGroup&& rhs) { *this = AZStd::move(rhs); }
        ScriptPropertyGroup& operator=(ScriptPropertyGroup&& rhs);
    };

    class ScriptComponent
        : public AZ::Component
        , private AZ::Data::AssetBus::Handler
        // carbonated begin (akostin/mp226-2): Add NetBindable to ScriptComponent
#if defined(CARBONATED)
        , public AzFramework::NetBindable
#endif
        // carbonated end
    {
        friend class AzToolsFramework::Components::ScriptEditorComponent;        

    public:
        // carbonated begin (akostin/mp226-2): Add NetBindable to ScriptComponent
#if defined(CARBONATED)
        static const char* NetRPCFieldName;
#endif
        // carbonated end
        static const char* DefaultFieldName;

        // carbonated begin (akostin/mp226-2): Add NetBindable to ScriptComponent
#if defined(CARBONATED)
        AZ_COMPONENT(AzFramework::ScriptComponent, "{8D1BC97E-C55D-4D34-A460-E63C57CD0D4B}", NetBindable);
#else
        AZ_COMPONENT(AzFramework::ScriptComponent, "{8D1BC97E-C55D-4D34-A460-E63C57CD0D4B}", AZ::Component);
#endif
        // carbonated end

        /// \red ComponentDescriptor::Reflect
        static void Reflect(AZ::ReflectContext* reflection);        

        ScriptComponent();
        ~ScriptComponent();

        AZ::ScriptContext* GetScriptContext() const            { return m_context; }
        void                  SetScriptContext(AZ::ScriptContext* context);

        const AZ::Data::Asset<AZ::ScriptAsset>& GetScript() const       { return m_script; }
        void                                    SetScript(const AZ::Data::Asset<AZ::ScriptAsset>& script);

        // Methods used for unit tests
        AZ::ScriptProperty* GetScriptProperty(const char* propertyName);

    protected:
        ScriptComponent(const ScriptComponent&) = delete;
        //////////////////////////////////////////////////////////////////////////
        // Component base
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // AssetBus
        void OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
        //////////////////////////////////////////////////////////////////////////

        // carbonated begin (akostin/mp226-2): Add NetBindable to ScriptComponent
#if defined(CARBONATED)
        //////////////////////////////////////////////////////////////////////////
        // NetBindable
        GridMate::ReplicaChunkPtr GetNetworkBinding() override;
        void SetNetworkBinding(GridMate::ReplicaChunkPtr chunk) override;
        void UnbindFromNetwork() override;
        //////////////////////////////////////////////////////////////////////////
#endif
        // carbonated end

        /// Loads the script into the context/VM, \returns true if the script is loaded
        bool LoadInContext();

        void CreateEntityTable();
        void DestroyEntityTable();

        // carbonated begin (akostin/mp226-2): Add NetBindable to ScriptComponent
#if defined(CARBONATED)
        void CreateNetworkBindingTable(int baseTableIndex, int entityTableIndex);
#endif
        // carbonated end
        
        void CreatePropertyGroup(const ScriptPropertyGroup& group, int propertyGroupTableIndex, int parentIndex, int metatableIndex, bool isRoot);

        AZ::ScriptContext*                  m_context;              ///< Context in which the script will be running
        AZ::ScriptContextId                 m_contextId;            ///< Id of the script context.
        AZ::Data::Asset<AZ::ScriptAsset>    m_script;               ///< Reference to the script asset used for this component.
        int                                 m_table;                ///< Cached table index
        ScriptPropertyGroup                 m_properties;           ///< List with all properties that were tweaked in the editor and should override values in the m_sourceScriptName class inside m_script.

        // carbonated begin (akostin/mp226-2): Add NetBindable to ScriptComponent
#if defined(CARBONATED)
        ScriptNetBindingTable* m_netBindingTable; ///< Table that will hold our networked script values, and manage callbacks
#endif
        // carbonated end

    };        
}   // namespace AZ
