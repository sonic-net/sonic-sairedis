#pragma once

#include <unordered_map>

#include "SelectableEventHandler.h"
#include "swss/sal.h"
#include "swss/selectable.h"
#include "swss/selectabletimer.h"

namespace syncd
{
    // This class keeps track of Selectable being used by an entity and their
    // corresponding EventHandler objects.
    class SelectablesTracker
    {
        private:

            // Not copyable or movable.
            SelectablesTracker(const SelectablesTracker &) = delete;
            SelectablesTracker(SelectablesTracker &&) = delete;
            SelectablesTracker &operator=(const SelectablesTracker &) = delete;
            SelectablesTracker &operator=(SelectablesTracker &&) = delete;

        public:

            SelectablesTracker() = default;

            virtual ~SelectablesTracker() = default;

            // Adds the mapping of Selectable and it's corresponding EventHandler object.
            virtual bool addSelectableToTracker(
                    _In_ swss::Selectable *selectable,
                    _In_ SelectableEventHandler *eventHandler);

            // Removes a Selectable from the map.
            virtual bool removeSelectableFromTracker(
                    _In_ swss::Selectable *selectable);

            // Checks if Selectable is present in the tracker map.
            virtual bool selectableIsTracked(
                    _In_ swss::Selectable *selectable);

            // Gets the EventHandler for a Selectable.
            virtual SelectableEventHandler *getEventHandlerForSelectable(
                    _In_ swss::Selectable *selectable);

        private:

            using SelectableFdToEventHandlerMap = std::unordered_map<int, SelectableEventHandler *>;

            SelectableFdToEventHandlerMap m_selectableFdToEventHandlerMap;
    };

}  // namespace syncd
