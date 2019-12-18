#pragma once

#include <AzCore/Component/Component.h>

#include <Playground/PlaygroundBus.h>

namespace Playground
{
    class PlaygroundSystemComponent
        : public AZ::Component
        , protected PlaygroundRequestBus::Handler
    {
    public:
        AZ_COMPONENT(PlaygroundSystemComponent, "{1FA3FFAA-E764-4581-934B-008015BD130D}");

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

    protected:
        ////////////////////////////////////////////////////////////////////////
        // PlaygroundRequestBus interface implementation

        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////
    };
}
