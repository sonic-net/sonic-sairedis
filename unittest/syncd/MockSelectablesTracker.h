#pragma once

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
                              SelectableEventHandler *eventHandler) override);

               MOCK_METHOD1(removeSelectableFromTracker,
                              bool(swss::Selectable *selectable) override);
     };
}  // namespace syncd
