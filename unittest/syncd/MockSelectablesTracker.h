#pragma once

#include <memory>

#include "SelectablesTracker.h"
#include "gmock/gmock.h"

namespace syncd
{

     class MockSelectablesTracker : public SelectablesTracker
     {
          public:

               MockSelectablesTracker() : SelectablesTracker() {}

               ~MockSelectablesTracker() override {}

               MOCK_METHOD2(addSelectableToTracker, bool(swss::Selectable *selectable,
                              std::shared_ptr<SelectableEventHandler> eventHandler));

               MOCK_METHOD1(removeSelectableFromTracker,
                              bool(swss::Selectable *selectable));

               MOCK_CONST_METHOD1(selectableIsTracked,
                              bool(swss::Selectable *selectable));

               MOCK_CONST_METHOD1(getEventHandlerForSelectable,
                              std::shared_ptr<SelectableEventHandler>(swss::Selectable *selectable));
     };
}  // namespace syncd
