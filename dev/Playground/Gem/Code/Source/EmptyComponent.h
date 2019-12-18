#pragma once
#include <AzCore\Component\Component.h>

namespace Playground
{
	class EmptyComponent
		: public AZ::Component
	{
	public:
		~EmptyComponent() override = default;
		AZ_COMPONENT(EmptyComponent, "{50486823-4BE6-44FC-AB13-B0C26FF5745D}");

		static void Reflect(AZ::ReflectContext* reflection);

		// AZComponent
		void Init() override {};
		void Activate() override;
		void Deactivate() override;

	private:
		// we'll expose this property to the editor
		AZStd::string m_someProperty;
	};
}