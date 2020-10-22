#include "SelectableChannel.h"

#include "swss/logger.h"

using namespace syncd;

SelectableChannel::SelectableChannel(
        _In_ int pri):
    Selectable(pri)
{
    SWSS_LOG_ENTER();

    // empty
}
