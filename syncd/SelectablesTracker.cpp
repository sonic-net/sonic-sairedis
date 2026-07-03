#include "SelectablesTracker.h"

#include "swss/logger.h"

using namespace syncd;

bool SelectablesTracker::addSelectableToTracker(
        _In_ swss::Selectable *selectable,
        _In_ std::shared_ptr<SelectableEventHandler> eventHandler)
{
    SWSS_LOG_ENTER();

    std::lock_guard<std::mutex> lock(m_mutex);

    if (selectable == nullptr)
    {
        SWSS_LOG_ERROR("Invalid Selectable:Selectable is NULL.");

        return false;
    }

    if (eventHandler == nullptr)
    {
        SWSS_LOG_ERROR("Event handler for Selectable with fd: %d is NULL.",
                selectable->getFd());

        return false;
    }

    int fd = selectable->getFd();

    if (m_selectableFdToEventHandlerMap.count(fd) != 0)
    {
        SWSS_LOG_ERROR("Selectable with fd %d is already in use.", fd);

        return false;
    }

    SWSS_LOG_INFO("Adding the Selectable with fd: %d.", fd);
    m_selectableFdToEventHandlerMap[fd] = eventHandler;

    return true;
}

bool SelectablesTracker::removeSelectableFromTracker(
        _In_ swss::Selectable *selectable)
{
    SWSS_LOG_ENTER();

    std::lock_guard<std::mutex> lock(m_mutex);

    if (selectable == nullptr)
    {
        SWSS_LOG_ERROR("Invalid Selectable:Selectable is NULL.");

        return false;
    }

    int fd = selectable->getFd();

    SWSS_LOG_INFO("Removing the Selectable with fd: %d.", fd);

    if (m_selectableFdToEventHandlerMap.erase(fd) == 0)
    {
        SWSS_LOG_ERROR("Selectable with fd %d is not present in the map!", fd);

        return false;
    }

    return true;
}

bool SelectablesTracker::selectableIsTracked(
        _In_ swss::Selectable *selectable) const
{
    SWSS_LOG_ENTER();

    std::lock_guard<std::mutex> lock(m_mutex);

    if ((selectable == nullptr) ||
        (m_selectableFdToEventHandlerMap.count(selectable->getFd()) == 0))
    {
        return false;
    }

    return true;
}

std::shared_ptr<SelectableEventHandler> SelectablesTracker::getEventHandlerForSelectable(
        _In_ swss::Selectable *selectable) const
{
    SWSS_LOG_ENTER();

    std::lock_guard<std::mutex> lock(m_mutex);

    if (selectable == nullptr)
    {
        SWSS_LOG_ERROR("Invalid Selectable:Selectable is NULL.");

        return nullptr;
    }

    int fd = selectable->getFd();
    auto it = m_selectableFdToEventHandlerMap.find(fd);

    if (it == m_selectableFdToEventHandlerMap.end())
    {
        SWSS_LOG_ERROR("Selectable with fd %d is not present in the map!", fd);

        return nullptr;
    }

    return it->second;
}
