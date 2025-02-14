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

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

#include <Integration/AnimationBus.h>
#include <Integration/EMotionFXBus.h>

#include <CrySystemBus.h> // Immediate-mode CryRendering only

#if defined (EMOTIONFXANIMATION_EDITOR)
#   include <AzCore/Debug/Timer.h>
#   include <AzToolsFramework/API/ToolsApplicationAPI.h>
#   include <AzToolsFramework/API/EditorAnimationSystemRequestBus.h>
#   include <AzToolsFramework/AssetBrowser/AssetBrowserBus.h>
#endif // EMOTIONFXANIMATION_EDITOR

namespace AZ
{
    namespace Data
    {
        class AssetHandler;
    }
}

namespace EMotionFX
{
    namespace Integration
    {
        class EMotionFXEventHandler;

        class SystemComponent
            : public AZ::Component
            , private SystemRequestBus::Handler
            , private AZ::TickBus::Handler
            , private CrySystemEventBus::Handler
            , private EMotionFXRequestBus::Handler
            , private RaycastRequestBus::Handler
#if defined (EMOTIONFXANIMATION_EDITOR)
            , private AzToolsFramework::EditorEvents::Bus::Handler
            , private AzToolsFramework::EditorAnimationSystemRequestsBus::Handler
            , private AzToolsFramework::AssetBrowser::AssetBrowserInteractionNotificationBus::Handler
#endif // EMOTIONFXANIMATION_EDITOR
        {
        public:
            AZ_COMPONENT(SystemComponent, "{7AE4102B-387C-4157-B8C7-8D1EA3BCFD60}");

            SystemComponent();
            ~SystemComponent() override;

            static void ReflectEMotionFX(AZ::ReflectContext* context);
            static void Reflect(AZ::ReflectContext* context);

            static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
            static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
            static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
            static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

            static int emfx_updateEnabled;
            static int emfx_actorRenderEnabled;

        private:
            //unique_ptr cannot be copied -> vector of unique_ptrs cannot be copied -> class cannot be copied
            SystemComponent(const SystemComponent&) = delete;

            ////////////////////////////////////////////////////////////////////////
            // AZ::Component interface implementation
            void Init() override;
            void Activate() override;
            void Deactivate() override;
            ////////////////////////////////////////////////////////////////////////

            ////////////////////////////////////////////////////////////////////////
            // AZ::TickBus::Handler
            void OnTick(float delta, AZ::ScriptTimePoint timePoint) override;
            int GetTickOrder() override;
            ////////////////////////////////////////////////////////////////////////

            ////////////////////////////////////////////////////////////////////////
            // CrySystemEventBus
            void OnCrySystemInitialized(ISystem&, const SSystemInitParams&) override;
            void OnCrySystemShutdown(ISystem&) override;
            void OnCryEditorInitialized() override;
            ////////////////////////////////////////////////////////////////////////

            ////////////////////////////////////////////////////////////////////////
            // EMotionFXRequestBus
            void RegisterAnimGraphObjectType(EMotionFX::AnimGraphObject* objectTemplate) override;
            ////////////////////////////////////////////////////////////////////////

            ////////////////////////////////////////////////////////////////////////
            // RaycastRequestBus
            void EnableRayRequests() override;
            void DisableRayRequests() override;
            RaycastRequests::RaycastResult Raycast(AZ::EntityId entityId, const RaycastRequests::RaycastRequest& rayRequest) override;
            ////////////////////////////////////////////////////////////////////////
            

            void RegisterAssetTypesAndHandlers();
            void SetMediaRoot(const char* alias);

#if defined (EMOTIONFXANIMATION_EDITOR)
            void UpdateAnimationEditorPlugins(float delta);
            void NotifyRegisterViews() override;
            bool IsSystemActive(EditorAnimationSystemRequests::AnimationSystem systemType);

            //////////////////////////////////////////////////////////////////////////////////////
            // AzToolsFramework::AssetBrowser::AssetBrowserInteractionNotificationBus::Handler
            AzToolsFramework::AssetBrowser::SourceFileDetails GetSourceFileDetails(const char* fullSourceFileName) override;
            //////////////////////////////////////////////////////////////////////////////////////

            AZ::Debug::Timer m_updateTimer;
            AZStd::vector<AzToolsFramework::PropertyHandlerBase*> m_propertyHandlers;
#endif // EMOTIONFXANIMATION_EDITOR

            AZ::u32 m_numThreads;

        private:
            AZStd::vector<AZStd::unique_ptr<AZ::Data::AssetHandler> > m_assetHandlers;
            AZStd::unique_ptr<EMotionFXEventHandler> m_eventHandler;
        };
    }
}
