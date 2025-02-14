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

#include "BaseManipulator.h"

#include <AzCore/Math/IntersectSegment.h>
#include <AzToolsFramework/Viewport/ViewportMessages.h>
#include <AzToolsFramework/Entity/EditorEntityHelpers.h>

namespace AzToolsFramework
{
    const AZ::Color BaseManipulator::s_defaultMouseOverColor = AZ::Color(1.0f, 1.0f, 0.0f, 1.0f); // yellow

    AZ_CLASS_ALLOCATOR_IMPL(BaseManipulator, AZ::SystemAllocator, 0)

    static bool EntityIdAndEntityComponentIdComparison(
        const AZ::EntityId entityId, const AZ::EntityComponentIdPair& entityComponentId)
    {
        return entityId == entityComponentId.GetEntityId();
    }

    BaseManipulator::~BaseManipulator()
    {
        AZ_Assert(!Registered(), "Manipulator must be unregistered before it is destroyed");
        EndUndoBatch();
    }

    bool BaseManipulator::OnLeftMouseDown(
        const ViewportInteraction::MouseInteraction& interaction, const float rayIntersectionDistance)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        if (m_onLeftMouseDownImpl)
        {
            BeginAction();
            ToolsApplicationRequests::Bus::BroadcastResult(
                m_undoBatch, &ToolsApplicationRequests::Bus::Events::BeginUndoBatch, "ManipulatorLeftMouseDown");

            for (const AZ::EntityComponentIdPair& entityComponentId : m_entityComponentIdPairs)
            {
                ToolsApplicationRequests::Bus::Broadcast(
                    &ToolsApplicationRequests::Bus::Events::AddDirtyEntity, entityComponentId.GetEntityId());
            }

            (*this.*m_onLeftMouseDownImpl)(interaction, rayIntersectionDistance);

            ToolsApplicationNotificationBus::Broadcast(
                &ToolsApplicationNotificationBus::Events::InvalidatePropertyDisplay, Refresh_Values);

            return true;
        }

        return false;
    }

    bool BaseManipulator::OnRightMouseDown(
        const ViewportInteraction::MouseInteraction& interaction, const float rayIntersectionDistance)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        if (m_onRightMouseDownImpl)
        {
            BeginAction();
            ToolsApplicationRequests::Bus::BroadcastResult(
                m_undoBatch, &ToolsApplicationRequests::Bus::Events::BeginUndoBatch, "ManipulatorRightMouseDown");

            for (const AZ::EntityComponentIdPair& entityComponentId : m_entityComponentIdPairs)
            {
                ToolsApplicationRequests::Bus::Broadcast(
                    &ToolsApplicationRequests::Bus::Events::AddDirtyEntity, entityComponentId.GetEntityId());
            }

            (*this.*m_onRightMouseDownImpl)(interaction, rayIntersectionDistance);

            ToolsApplicationNotificationBus::Broadcast(
                &ToolsApplicationNotificationBus::Events::InvalidatePropertyDisplay, Refresh_Values);

            return true;
        }

        return false;
    }

    // note: OnLeft/RightMouseUp will not be called if OnLeft/RightMouseDownImpl have not been
    // attached as no active manipulator will have been set in ManipulatorManager.
    void BaseManipulator::OnLeftMouseUp(const ViewportInteraction::MouseInteraction& interaction)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        EndAction();
        OnLeftMouseUpImpl(interaction);
        EndUndoBatch();
    }

    void BaseManipulator::OnRightMouseUp(const ViewportInteraction::MouseInteraction& interaction)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        EndAction();
        OnRightMouseUpImpl(interaction);
        EndUndoBatch();
    }

    bool BaseManipulator::OnMouseOver(
        const ManipulatorId manipulatorId, const ViewportInteraction::MouseInteraction& interaction)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        UpdateMouseOver(manipulatorId);
        OnMouseOverImpl(manipulatorId, interaction);
        return m_mouseOver;
    }

    void BaseManipulator::OnMouseWheel(const ViewportInteraction::MouseInteraction& interaction)
    {
        OnMouseWheelImpl(interaction);

        ToolsApplicationNotificationBus::Broadcast(
            &ToolsApplicationNotificationBus::Events::InvalidatePropertyDisplay, Refresh_Values);
    }

    void BaseManipulator::OnMouseMove(const ViewportInteraction::MouseInteraction& interaction)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        if (!m_performingAction)
        {
            AZ_Warning(
                "Manipulators", false,
                "MouseMove action received, but this manipulator is not performing an action");

            return;
        }

        // ensure property grid (entity inspector) values are refreshed
        ToolsApplicationNotificationBus::Broadcast(
            &ToolsApplicationNotificationBus::Events::InvalidatePropertyDisplay, Refresh_Values);

        OnMouseMoveImpl(interaction);
    }

    void BaseManipulator::SetBoundsDirty()
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        SetBoundsDirtyImpl();
    }

    void BaseManipulator::Register(const ManipulatorManagerId managerId)
    {
        if (Registered())
        {
            Unregister();
        }

        ManipulatorManagerRequestBus::Event(managerId,
            &ManipulatorManagerRequestBus::Events::RegisterManipulator, shared_from_this());
    }

    void BaseManipulator::Unregister()
    {
        // if the manipulator has already been unregistered, the m_manipulatorManagerId
        // should be invalid which makes the call below a no-op.
        ManipulatorManagerRequestBus::Event(m_manipulatorManagerId,
            &ManipulatorManagerRequestBus::Events::UnregisterManipulator, this);
    }

    void BaseManipulator::Invalidate()
    {
        SetBoundsDirty();
        InvalidateImpl();
        m_mouseOver = false;
        m_performingAction = false;
        m_manipulatorId = InvalidManipulatorId;
        m_manipulatorManagerId = InvalidManipulatorManagerId;
    }

    void BaseManipulator::BeginAction()
    {
        if (m_performingAction)
        {
            AZ_Warning(
                "Manipulators", false,
                "MouseDown action received, but the manipulator (id: %d) is still performing an action",
                GetManipulatorId());

            return;
        }

        m_performingAction = true;
    }

    void BaseManipulator::EndAction()
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        if (!m_performingAction)
        {
            AZ_Warning(
                "Manipulators", false,
                "MouseUp action received, but this manipulator (id: %d) didn't receive MouseDown action before",
                GetManipulatorId());
            return;
        }

        m_performingAction = false;

        // let other systems know that a manipulator has modified an entity component property
        NotifyEntityComponentPropertyChanged();
    }

    void BaseManipulator::EndUndoBatch()
    {
        if (m_undoBatch != nullptr)
        {
            ToolsApplicationRequests::Bus::Broadcast(&ToolsApplicationRequests::Bus::Events::EndUndoBatch);
            m_undoBatch = nullptr;
        }
    }

    void BaseManipulator::AddEntityId(const AZ::EntityId entityId)
    {
        m_entityComponentIdPairs.insert(AZ::EntityComponentIdPair(entityId, AZ::ComponentId{}));
    }

    void BaseManipulator::AddEntityComponentIdPair(const AZ::EntityComponentIdPair& entityIdComponentPair)
    {
        m_entityComponentIdPairs.insert(entityIdComponentPair);
    }

    void BaseManipulator::NotifyEntityComponentPropertyChanged()
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        for (const AZ::EntityComponentIdPair& entityComponentIdPair : m_entityComponentIdPairs)
        {
            // if we have a valid component, send a single message for that component id
            if (entityComponentIdPair.GetComponentId() != AZ::InvalidComponentId)
            {
                PropertyEditorEntityChangeNotificationBus::Event(
                    entityComponentIdPair.GetEntityId(),
                    &PropertyEditorEntityChangeNotifications::OnEntityComponentPropertyChanged,
                    entityComponentIdPair.GetComponentId());
            }
            else
            {
                AZ_Warning("Manipulators", false,
                    "This Manipulator was only registered with an EntityId and not an EntityComponentIdPair. "
                    "Please use AddEntityComponentIdPair() instead of AddEntityId() when registering what this "
                    "Manipulator is changing.");

                // if we do not have a valid component id, send the message to all components on the entity,
                // this is inefficient but will guarantee the component that changed does get the message.
                const AZ::Entity* entity = GetEntityById(entityComponentIdPair.GetEntityId());
                for (const AZ::Component* component : entity->GetComponents())
                {
                    PropertyEditorEntityChangeNotificationBus::Event(
                        entity->GetId(), &PropertyEditorEntityChangeNotifications::OnEntityComponentPropertyChanged,
                        component->GetId());
                }
            }
        }
    }

    AZStd::unordered_set<AZ::EntityComponentIdPair>::iterator BaseManipulator::RemoveEntityId(const AZ::EntityId entityId)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        auto afterErased = m_entityComponentIdPairs.end();

        bool allErased = false;
        while (!allErased)
        {
            // look for a match (keep looking in case we have several entity ids with different component ids)
            const auto entityComponentPairId =
                m_entityComponentIdPairs.find_as(
                    entityId, AZStd::hash<AZ::EntityId>(),
                    &EntityIdAndEntityComponentIdComparison);

            // update the afterErased variable so we can return an iterator
            // to the correct position in the container.
            if (entityComponentPairId != m_entityComponentIdPairs.end())
            {
                afterErased = m_entityComponentIdPairs.erase(entityComponentPairId);
            }
            else
            {
                allErased = true;
            }
        }

        return afterErased;
    }

    AZStd::unordered_set<AZ::EntityComponentIdPair>::iterator BaseManipulator::RemoveEntityComponentIdPair(
        const AZ::EntityComponentIdPair& entityComponentIdPair)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        const auto entityIdIt = m_entityComponentIdPairs.find(entityComponentIdPair);

        if (entityIdIt != m_entityComponentIdPairs.end())
        {
            return m_entityComponentIdPairs.erase(entityIdIt);
        }

        return entityIdIt;
    }

    bool BaseManipulator::HasEntityId(const AZ::EntityId entityId) const
    {
        return m_entityComponentIdPairs.find_as(
            entityId, AZStd::hash<AZ::EntityId>(),
            &EntityIdAndEntityComponentIdComparison) != m_entityComponentIdPairs.end();
    }

    bool BaseManipulator::HasEntityComponentIdPair(const AZ::EntityComponentIdPair& entityComponentIdPair) const
    {
        return m_entityComponentIdPairs.find(entityComponentIdPair) != m_entityComponentIdPairs.end();
    }

    void Manipulators::Register(const ManipulatorManagerId manipulatorManagerId)
    {
        ProcessManipulators([manipulatorManagerId](BaseManipulator* manipulator)
        {
            manipulator->Register(manipulatorManagerId);
        });
    }

    void Manipulators::Unregister()
    {
        ProcessManipulators([](BaseManipulator* manipulator)
        {
            if (manipulator->Registered())
            {
                manipulator->Unregister();
            }
        });
    }

    void Manipulators::SetBoundsDirty()
    {
        ProcessManipulators([](BaseManipulator* manipulator)
        {
            manipulator->SetBoundsDirty();
        });
    }

    void Manipulators::AddEntityId(const AZ::EntityId entityId)
    {
        ProcessManipulators([entityId](BaseManipulator* manipulator)
        {
            manipulator->AddEntityId(entityId);
        });
    }

    void Manipulators::AddEntityComponentIdPair(const AZ::EntityComponentIdPair& entityComponentIdPair)
    {
        ProcessManipulators([&entityComponentIdPair](BaseManipulator* manipulator)
        {
            manipulator->AddEntityComponentIdPair(entityComponentIdPair);
        });
    }

    void Manipulators::RemoveEntityComponentIdPair(const AZ::EntityComponentIdPair& entityComponentIdPair)
    {
        ProcessManipulators([&entityComponentIdPair](BaseManipulator* manipulator)
        {
            manipulator->RemoveEntityComponentIdPair(entityComponentIdPair);
        });
    }

    void Manipulators::RemoveEntityId(const AZ::EntityId entityId)
    {
        ProcessManipulators([entityId](BaseManipulator* manipulator)
        {
            manipulator->RemoveEntityId(entityId);
        });
    }

    bool Manipulators::PerformingAction()
    {
        bool performingAction = false;
        ProcessManipulators([&performingAction](BaseManipulator* manipulator)
        {
            if (manipulator->PerformingAction())
            {
                performingAction = true;
            }
        });

        return performingAction;
    }

    bool Manipulators::Registered()
    {
        bool registered = false;
        ProcessManipulators([&registered](BaseManipulator* manipulator)
        {
            if (manipulator->Registered())
            {
                registered = true;
            }
        });

        return registered;
    }

    namespace Internal
    {
        bool CalculateRayPlaneIntersectingPoint(const AZ::Vector3& rayOrigin, const AZ::Vector3& rayDirection,
            const AZ::Vector3& pointOnPlane, const AZ::Vector3& planeNormal, AZ::Vector3& resultIntersectingPoint)
        {
            float t = 0.0f;
            if (AZ::Intersect::IntersectRayPlane(rayOrigin, rayDirection, pointOnPlane, planeNormal, t) > 0)
            {
                resultIntersectingPoint = rayOrigin + t * rayDirection;
                return true;
            }

            return false;
        }
    } // namespace Internal
} // namespace AzToolsFramework
