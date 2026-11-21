#include "OidRefCounter.h"

#include "swss/logger.h"

#include <inttypes.h>

using namespace saimeta;

namespace
{
    struct RefBatch
    {
        bool active;
        OidRefCounter* counter;
        sai_object_id_t oid[8];
        uint32_t n[8];
        int count;
    };

    thread_local RefBatch g_ref_batch;
}

void OidRefCounter::clear()
{
    SWSS_LOG_ENTER();

    m_hash.clear();
}

bool OidRefCounter::objectReferenceExists(
        _In_ sai_object_id_t oid) const
{
    SWSS_LOG_ENTER();

    bool exists = m_hash.find(oid) != m_hash.end();

    SWSS_LOG_DEBUG("object 0x%" PRIx64 " reference: %s", oid, exists ? "exists" : "missing");

    return exists;
}

void OidRefCounter::startBulkReferenceIncrements()
{
    SWSS_LOG_ENTER();

    if (g_ref_batch.active && g_ref_batch.counter == this)
    {
        flushBulkReferenceIncrements();
    }

    g_ref_batch.active = true;
    g_ref_batch.counter = this;
    g_ref_batch.count = 0;
}

void OidRefCounter::flushBulkReferenceIncrements()
{
    SWSS_LOG_ENTER();

    const bool mine = g_ref_batch.active && g_ref_batch.counter == this;

    g_ref_batch.active = false;

    if (!mine)
    {
        return;
    }

    for (int i = 0; i < g_ref_batch.count; ++i)
    {
        const sai_object_id_t oid = g_ref_batch.oid[i];
        auto it = m_hash.find(oid);

        if (it == m_hash.end())
        {
            g_ref_batch.count = 0;
            g_ref_batch.counter = nullptr;

            SWSS_LOG_THROW("FATAL: object oid 0x%" PRIx64 " not in reference map", oid);
        }

        it->second += (int32_t)g_ref_batch.n[i];
    }

    g_ref_batch.count = 0;
    g_ref_batch.counter = nullptr;
}

void OidRefCounter::objectReferenceIncrement(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    if (oid == SAI_NULL_OBJECT_ID)
    {
        // We don't keep track of NULL object id's.
        return;
    }

    if (g_ref_batch.active && g_ref_batch.counter == this)
    {
        for (int i = 0; i < g_ref_batch.count; ++i)
        {
            if (g_ref_batch.oid[i] == oid)
            {
                g_ref_batch.n[i]++;

                return;
            }
        }

        if (g_ref_batch.count < 8)
        {
            if (!objectReferenceExists(oid))
            {
                SWSS_LOG_THROW("FATAL: object oid 0x%" PRIx64 " not in reference map", oid);
            }

            g_ref_batch.oid[g_ref_batch.count] = oid;
            g_ref_batch.n[g_ref_batch.count] = 1;
            g_ref_batch.count++;

            return;
        }
    }

    if (!objectReferenceExists(oid))
    {
        SWSS_LOG_THROW("FATAL: object oid 0x%" PRIx64 " not in reference map", oid);
    }

    m_hash[oid]++;

    SWSS_LOG_DEBUG("increased reference on oid 0x%" PRIx64 " to %d", oid, m_hash[oid]);
}

void OidRefCounter::objectReferenceIncrement(
        _In_ const sai_object_list_t& list)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < list.count; ++i)
    {
        objectReferenceIncrement(list.list[i]);
    }
}

void OidRefCounter::objectReferenceDecrement(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    if (oid == SAI_NULL_OBJECT_ID)
    {
        // We don't keep track of NULL object id's.
        return;
    }

    if (!objectReferenceExists(oid))
    {
        SWSS_LOG_THROW("FATAL: object oid 0x%" PRIx64 " not in reference map", oid);
    }

    m_hash[oid]--;

    if (m_hash[oid] < 0)
    {
        SWSS_LOG_THROW("FATAL: object oid 0x%" PRIx64 " reference count is negative!", oid);
    }

    SWSS_LOG_DEBUG("decreased reference on oid 0x%" PRIx64 " to %d", oid, m_hash[oid]);
}

void OidRefCounter::objectReferenceDecrement(
        _In_ const sai_object_list_t& list)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < list.count; ++i)
    {
        objectReferenceDecrement(list.list[i]);
    }
}

void OidRefCounter::objectReferenceInsert(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    if (objectReferenceExists(oid))
    {
        SWSS_LOG_THROW("FATAL: object oid 0x%" PRIx64 " already in reference map", oid);
    }

    m_hash[oid] = 0;

    SWSS_LOG_DEBUG("inserted reference on 0x%" PRIx64 "", oid);
}

void OidRefCounter::objectReferenceRemove(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    if (objectReferenceExists(oid))
    {
        int32_t count = getObjectReferenceCount(oid);

        if (count > 0)
        {
            SWSS_LOG_THROW("FATAL: removing object oid 0x%" PRIx64 " but reference count is: %d", oid, count);
        }
    }
    else
    {
        SWSS_LOG_THROW("FATAL: object oid 0x%" PRIx64 " reference not in map", oid);
    }

    SWSS_LOG_DEBUG("removing object oid 0x%" PRIx64 " reference", oid);

    m_hash.erase(oid);
}

int32_t OidRefCounter::getObjectReferenceCount(
        _In_ sai_object_id_t oid) const
{
    SWSS_LOG_ENTER();

    if (objectReferenceExists(oid))
    {
        int32_t count = m_hash.at(oid);

        SWSS_LOG_DEBUG("reference count on oid 0x%" PRIx64 " is %d", oid, count);

        return count;
    }

    SWSS_LOG_THROW("FATAL: object oid 0x%" PRIx64 " reference not in map", oid);
}

bool OidRefCounter::isObjectInUse(
        _In_ sai_object_id_t oid) const
{
    SWSS_LOG_ENTER();

    return getObjectReferenceCount(oid) > 0;
}

std::unordered_map<sai_object_id_t, int32_t> OidRefCounter::getAllReferences() const
{
    SWSS_LOG_ENTER();

    return m_hash; // copy
}

std::vector<sai_object_id_t> OidRefCounter::getAllOids() const
{
    SWSS_LOG_ENTER();

    std::vector<sai_object_id_t> vec;

    for (auto& it: m_hash)
    {
        vec.push_back(it.first);
    }

    return vec;
}

void OidRefCounter::objectReferenceClear(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    if (objectReferenceExists(oid))
    {
        SWSS_LOG_DEBUG("removing object oid 0x%" PRIx64 " reference", oid);

        m_hash.erase(oid);
    }
    else
    {
        SWSS_LOG_THROW("FATAL: object oid 0x%" PRIx64 " reference not in map", oid);
    }
}
