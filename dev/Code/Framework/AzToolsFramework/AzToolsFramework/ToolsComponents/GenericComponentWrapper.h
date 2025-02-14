/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#pragma once

#include <AzCore/Slice/SliceBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

namespace AzFramework
{
    class EditorEntityEvents;
}

namespace AzToolsFramework
{
    namespace Components
    {
        class GenericComponentWrapperDescriptor;

        /**
         * GenericComponentWrapper wraps around a component in the
         * editor. It is used to add components without a specialized
         * editor component to an entity.
         */
        class GenericComponentWrapper
            : public EditorComponentBase
            , private AzFramework::EntityDebugDisplayEventBus::Handler
        {
        public:
            AZ_CLASS_ALLOCATOR(GenericComponentWrapper, AZ::SystemAllocator, 0);
            AZ_RTTI(GenericComponentWrapper, "{68D358CA-89B9-4730-8BA6-E181DEA28FDE}", EditorComponentBase);

            static AZ::ComponentDescriptor* CreateDescriptor();

            GenericComponentWrapper();
            GenericComponentWrapper(const AZ::SerializeContext::ClassData* templateClassData);
            GenericComponentWrapper(AZ::Component* templateClass);
            ~GenericComponentWrapper();

            const char* GetDisplayName();
            const char* GetDisplayDescription();

            // AZ::Component
            void Init() override;
            void Activate() override;
            void Deactivate() override;
            const AZ::TypeId& GetUnderlyingComponentType() const override;

            // AzFramework::DebugDisplayRequestBus
            void DisplayEntityViewport(
                const AzFramework::ViewportInfo& viewportInfo,
                AzFramework::DebugDisplayRequests& debugDisplay) override;

            void BuildGameEntity(AZ::Entity* gameEntity) override;
            void SetPrimaryAsset(const AZ::Data::AssetId& assetId) override;

            AZ::ComponentValidationResult ValidateComponentRequirements(
                const AZ::ImmutableEntityVector& sliceEntities) const override;

            AZ::Component* GetTemplate() const { return m_template; }

            /// Forget about, and release ownership of, template component.
            AZ::Component* ReleaseTemplate();

            static void Reflect(AZ::ReflectContext* context);

        protected:

            AZ::Component* m_template = nullptr;
            AzFramework::EditorEntityEvents* m_templateEvents = nullptr;
            AZStd::string m_displayName;
            AZStd::string m_displayDescription;
        };
    } // namespace Components

    /// Returns the component's type ID.
    /// If the component is a GenericComponentWrapper,
    /// then the type ID of the wrapped component is returned.
    const AZ::Uuid& GetUnderlyingComponentType(const AZ::Component& component);

    /**
     * Find the component of the specified type on an entity.
     * This function is often used to find components that don't have editor-time counterparts and thus are wrapped in \ref GenericComponentWrapper.
     * @param entity The pointer to an entity.
     * @return A pointer to the component found on the entity. If multiple components are found the first one is returned.
     */
    template <typename ComponentType>
    ComponentType* FindWrappedComponentForEntity(const AZ::Entity* entity)
    {
        if (!entity)
        {
            return nullptr;
        }

        AZStd::vector<Components::GenericComponentWrapper*> genericComponentsArray = entity->FindComponents<Components::GenericComponentWrapper>();
        if (genericComponentsArray.empty())
        {
            return nullptr;
        }

        for (Components::GenericComponentWrapper* genericComponent : genericComponentsArray)
        {
            auto componentType = GetUnderlyingComponentType(*genericComponent);
            if (componentType == azrtti_typeid<ComponentType>())
            {
                return static_cast<ComponentType*>(genericComponent->GetTemplate());
            }
        }
        return nullptr;
    }
} // namespace AzToolsFramework