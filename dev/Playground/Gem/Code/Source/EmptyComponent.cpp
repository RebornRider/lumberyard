#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>

#include <EmptyComponent.h>

namespace Playground
{
	void EmptyComponent::Reflect(AZ::ReflectContext* reflection)
	{
		AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
		if (serializeContext)
		{
			// we must include any fields we want to expose to the editor or lua in the serialize context
			serializeContext->Class<EmptyComponent>()
				->Version(1)
				->Field("Example property", &EmptyComponent::m_someProperty);


			// expose this component and a single variable to the editor
			AZ::EditContext* editContext = serializeContext->GetEditContext();
			if (editContext)
			{
				editContext->Class<EmptyComponent>("EmptyComponent", "Example Component")
					->ClassElement(AZ::Edit::ClassElements::EditorData, "")
					->Attribute(AZ::Edit::Attributes::Category, "SomeCategoryName") //can be any name you want and it will put items in a category together if the names match.
					->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game")) // that AZ_CRC() macro is the magic you need.
					->DataElement(0, &EmptyComponent::m_someProperty, "SomeProperty", "Example property");
			}
		}
	}

	void EmptyComponent::Activate()
	{
	}

	void EmptyComponent::Deactivate()
	{

	}
}