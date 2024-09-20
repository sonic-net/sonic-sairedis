#pragma once

namespace syncd
{
    // This class implements handler for Selectable events.
    class SelectableEventHandler
    {
        public:

            virtual ~SelectableEventHandler() = default;

            virtual void handleSelectableEvent() = 0;

        protected:

            SelectableEventHandler() = default;
    };

}  // namespace syncd
