#include "VppTransaction.h"

#include "swss/logger.h"

#include <exception>

using namespace saivs;

namespace
{
    thread_local VppTransaction* t_current = nullptr;

    // stays inactive: it is never the transaction of a scope
    thread_local VppTransaction t_inactive;
}

VppTransaction& VppTransaction::current()
{
    SWSS_LOG_ENTER();

    return t_current ? *t_current : t_inactive;
}

bool VppTransaction::active() const
{
    SWSS_LOG_ENTER();

    return m_active;
}

void VppTransaction::onRollback(
        _In_ const std::string& desc,
        _In_ Undo undo)
{
    SWSS_LOG_ENTER();

    if (!m_active)
    {
        SWSS_LOG_DEBUG("no transaction open, not recording undo of %s", desc.c_str());
        return;
    }

    if (m_rollingBack)
    {
        SWSS_LOG_DEBUG("rolling back, not recording undo of %s", desc.c_str());
        return;
    }

    SWSS_LOG_DEBUG("recording undo of %s", desc.c_str());

    m_journal.push_back({desc, std::move(undo)});
}

size_t VppTransaction::savepoint() const
{
    SWSS_LOG_ENTER();

    return m_journal.size();
}

sai_status_t VppTransaction::rollbackTo(
        _In_ size_t savepoint)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;

    m_rollingBack = true;

    while (m_journal.size() > savepoint)
    {
        Entry entry = std::move(m_journal.back());
        m_journal.pop_back();

        int ret = -1;

        try
        {
            ret = entry.undo();
        }
        catch (const std::exception& e)
        {
            SWSS_LOG_ERROR("rollback: undo of %s threw: %s", entry.desc.c_str(), e.what());
            status = SAI_STATUS_FAILURE;
            continue;
        }

        if (ret != 0)
        {
            SWSS_LOG_ERROR("rollback: undo of %s failed (%d)", entry.desc.c_str(), ret);
            status = SAI_STATUS_FAILURE;
        }
        else
        {
            SWSS_LOG_NOTICE("rollback: undid %s", entry.desc.c_str());
        }
    }

    m_rollingBack = false;

    return status;
}

sai_status_t VppTransaction::rollback()
{
    SWSS_LOG_ENTER();

    return rollbackTo(0);
}

void VppTransaction::commit()
{
    SWSS_LOG_ENTER();

    m_journal.clear();
}

TxnScope::TxnScope():
    m_txn(t_current),
    m_previous(t_current),
    m_savepoint(0)
{
    SWSS_LOG_ENTER();

    if (m_txn == nullptr || m_txn->m_rollingBack)
    {
        m_root.m_active = true;
        m_txn = &m_root;
        t_current = m_txn;
    }
    else
    {
        m_savepoint = m_txn->savepoint();
    }
}

TxnScope::~TxnScope()
{
    SWSS_LOG_ENTER();

    if (!m_done && rollback() != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("incomplete rollback of a transaction scope left without commit");
    }

    if (m_txn == &m_root)
    {
        t_current = m_previous;
    }
}

VppTransaction& TxnScope::txn()
{
    SWSS_LOG_ENTER();

    return *m_txn;
}

void TxnScope::commit()
{
    SWSS_LOG_ENTER();

    if (m_txn == &m_root)
    {
        m_root.commit();
    }

    m_done = true;
}

sai_status_t TxnScope::rollback()
{
    SWSS_LOG_ENTER();

    m_done = true;

    return m_txn->rollbackTo(m_savepoint);
}
