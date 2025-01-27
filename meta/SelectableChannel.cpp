#include "SelectableChannel.h"

#include "swss/logger.h"

using namespace otairedis;

SelectableChannel::SelectableChannel(
        _In_ int pri):
    Selectable(pri)
{
    SWSS_LOG_ENTER();

    // empty
}
