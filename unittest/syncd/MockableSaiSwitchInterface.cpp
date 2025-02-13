#include "MockableSaiSwitchInterface.h"
#include "VidManager.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"

using namespace unittests;

MockableSaiSwitchInterface::MockableSaiSwitchInterface(
        _In_ sai_object_id_t switchVid,
        _In_ sai_object_id_t switchRid):
    SaiSwitchInterface(switchVid, switchRid)
{
    SWSS_LOG_ENTER();
}

std::unordered_map<sai_object_id_t, sai_object_id_t> MockableSaiSwitchInterface::getVidToRidMap() const
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

std::unordered_map<sai_object_id_t, sai_object_id_t> MockableSaiSwitchInterface::getRidToVidMap() const
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

bool MockableSaiSwitchInterface::isDiscoveredRid(
        _In_ sai_object_id_t rid) const
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

bool MockableSaiSwitchInterface::isColdBootDiscoveredRid(
        _In_ sai_object_id_t rid) const
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

bool MockableSaiSwitchInterface::isSwitchObjectDefaultRid(
        _In_ sai_object_id_t rid) const
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

bool MockableSaiSwitchInterface::isNonRemovableRid(
        _In_ sai_object_id_t rid) const
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

std::set<sai_object_id_t> MockableSaiSwitchInterface::getDiscoveredRids() const
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

void MockableSaiSwitchInterface::removeExistingObject(
        _In_ sai_object_id_t rid)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

void MockableSaiSwitchInterface::removeExistingObjectReference(
        _In_ sai_object_id_t rid)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

void MockableSaiSwitchInterface::getDefaultMacAddress(
        _Out_ sai_mac_t& mac) const
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

void MockableSaiSwitchInterface::getVxlanDefaultRouterMacAddress(
        _Out_ sai_mac_t& mac) const
{
    SWSS_LOG_ENTER();

    mac[0] = 1;
    mac[1] = 2;
    mac[2] = 3;
    mac[3] = 4;
    mac[4] = 5;
    mac[5] = 6;
}

sai_object_id_t MockableSaiSwitchInterface::getDefaultValueForOidAttr(
        _In_ sai_object_id_t rid,
        _In_ sai_attr_id_t attr_id)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

std::set<sai_object_id_t> MockableSaiSwitchInterface::getColdBootDiscoveredVids() const
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

std::set<sai_object_id_t> MockableSaiSwitchInterface::getWarmBootDiscoveredVids() const
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

void MockableSaiSwitchInterface::onPostPortCreate(
        _In_ sai_object_id_t port_rid,
        _In_ sai_object_id_t port_vid)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

void MockableSaiSwitchInterface::postPortRemove(
        _In_ sai_object_id_t portRid)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

void MockableSaiSwitchInterface::collectPortRelatedObjects(
        _In_ sai_object_id_t portRid)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}
