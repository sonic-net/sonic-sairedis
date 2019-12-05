#include "VirtualObjectIdManager.h"

#include "meta/sai_serialize.h"
#include "swss/logger.h"

extern "C" {
#include "saimetadata.h"
}

// for redis client
#include "sai_redis.h"

#define SAI_OBJECT_ID_BITS_SIZE (8 * sizeof(sai_object_id_t))

static_assert(SAI_OBJECT_ID_BITS_SIZE == 64, "sai_object_id_t must have 64 bits");
static_assert(sizeof(sai_object_id_t) == sizeof(uint64_t), "SAI object ID size should be uint64_t");

#define SAI_REDIS_SWITCH_INDEX_BITS_SIZE ( 8 )
#define SAI_REDIS_SWITCH_INDEX_MAX ( (1ULL << SAI_REDIS_SWITCH_INDEX_BITS_SIZE) - 1 )
#define SAI_REDIS_SWITCH_INDEX_MASK (SAI_REDIS_SWITCH_INDEX_MAX)

#define SAI_REDIS_GLOBAL_CONTEXT_BITS_SIZE ( 8 )
#define SAI_REDIS_GLOBAL_CONTEXT_MAX ( (1ULL << SAI_REDIS_GLOBAL_CONTEXT_BITS_SIZE) - 1 )
#define SAI_REDIS_GLOBAL_CONTEXT_MASK (SAI_REDIS_GLOBAL_CONTEXT_MAX)

#define SAI_REDIS_OBJECT_TYPE_BITS_SIZE ( 8 )
#define SAI_REDIS_OBJECT_TYPE_MAX ( (1ULL << SAI_REDIS_OBJECT_TYPE_BITS_SIZE) - 1 )
#define SAI_REDIS_OBJECT_TYPE_MASK (SAI_REDIS_OBJECT_TYPE_MAX)

#define SAI_REDIS_OBJECT_INDEX_BITS_SIZE ( 40 )
#define SAI_REDIS_OBJECT_INDEX_MAX ( (1ULL << SAI_REDIS_OBJECT_INDEX_BITS_SIZE) - 1 )
#define SAI_REDIS_OBJECT_INDEX_MASK (SAI_REDIS_OBJECT_INDEX_MAX)

#define SAI_REDIS_OBJECT_ID_BITS_SIZE (      \
        SAI_REDIS_SWITCH_INDEX_BITS_SIZE +   \
        SAI_REDIS_GLOBAL_CONTEXT_BITS_SIZE + \
        SAI_REDIS_OBJECT_TYPE_BITS_SIZE +    \
        SAI_REDIS_OBJECT_INDEX_BITS_SIZE )

static_assert(SAI_REDIS_OBJECT_ID_BITS_SIZE == SAI_OBJECT_ID_BITS_SIZE, "redis object id size must be equal to SAI object id size");

/*
 * This condition must be met, since we need to be able to encode SAI object
 * type in object id on defined number of bits.
 */
static_assert(SAI_OBJECT_TYPE_EXTENSIONS_MAX < SAI_REDIS_OBJECT_TYPE_MAX, "redis max object type value must be greater than supported SAI max objeect type value");

/*
 * Current OBJECT ID format:
 *
 * bits 63..56 - switch index
 * bits 55..48 - SAI object type
 * bits 47..40 - global context
 * bits 40..0  - object index
 *
 * So large number of bits is required, otherwise we would need to have map of
 * OID to some struct that will have all those values.  But having all this
 * information in OID itself is more convenient.
 */

#define SAI_REDIS_GET_OBJECT_INDEX(oid) \
    ( ((uint64_t)oid) & ( SAI_REDIS_OBJECT_INDEX_MASK ) )

#define SAI_REDIS_GET_GLOBAL_CONTEXT(oid) \
    ( (((uint64_t)oid) >> (SAI_REDIS_OBJECT_INDEX_BITS_SIZE) ) & ( SAI_REDIS_OBJECT_TYPE_MASK ) )

#define SAI_REDIS_GET_OBJECT_TYPE(oid) \
    ( (((uint64_t)oid) >> ( SAI_REDIS_GLOBAL_CONTEXT_BITS_SIZE + SAI_REDIS_OBJECT_INDEX_BITS_SIZE) ) & ( SAI_REDIS_OBJECT_TYPE_MASK ) )

#define SAI_REDIS_GET_SWITCH_INDEX(oid) \
    ( (((uint64_t)oid) >> ( SAI_REDIS_OBJECT_TYPE_BITS_SIZE + SAI_REDIS_GLOBAL_CONTEXT_BITS_SIZE + SAI_REDIS_OBJECT_INDEX_BITS_SIZE) ) & ( SAI_REDIS_OBJECT_TYPE_MASK ) )

//#define SAI_REDIS_CONSTRUCT_OID(switch_index, object_type, global_context, object_index) 
//   (((uint64_t)(switch_index & ( SAI_REDIS_OBJECT_INDEX_MASK ))) | 

#define SAI_REDIS_TEST_OID (0x0123456789abcdef)

static_assert(SAI_REDIS_GET_SWITCH_INDEX(SAI_REDIS_TEST_OID) == 0x01, "test switch index");
static_assert(SAI_REDIS_GET_OBJECT_TYPE(SAI_REDIS_TEST_OID) == 0x23, "test object type");
static_assert(SAI_REDIS_GET_GLOBAL_CONTEXT(SAI_REDIS_TEST_OID) == 0x45, "test global context");
static_assert(SAI_REDIS_GET_OBJECT_INDEX(SAI_REDIS_TEST_OID) == 0x6789abcdef, "test object index");

using namespace sairedis;

VirtualObjectIdManager::VirtualObjectIdManager(
        _In_ uint32_t globalContext):
    m_globalContext(globalContext)
{
    SWSS_LOG_ENTER();

    if (globalContext > SAI_REDIS_GLOBAL_CONTEXT_MAX)
    {
        SWSS_LOG_THROW("specified globalContext(0x%x) > maximum global context 0x%llx",
                globalContext,
                SAI_REDIS_GLOBAL_CONTEXT_MAX);
    }
}

sai_object_id_t VirtualObjectIdManager::saiSwitchIdQuery(
        _In_ sai_object_id_t objectId) const
{
    SWSS_LOG_ENTER();

    if (objectId == SAI_NULL_OBJECT_ID)
    {
        return SAI_NULL_OBJECT_ID;
    }

    sai_object_type_t objectType = saiObjectTypeQuery(objectId);

    if (objectType == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_THROW("invalid object type of oid %s", 
                sai_serialize_object_id(objectId).c_str());
    }

    if (objectType == SAI_OBJECT_TYPE_SWITCH)
    {
        return objectId;
    }

    // NOTE: we could also check:
    // - if object id has correct global context
    // - if object id has existing switch index
    // but then this method can't be made static

    uint32_t switchIndex = (uint32_t)SAI_REDIS_GET_SWITCH_INDEX(objectId);

    // TODO for static - use SAI_REDIS_GET_GLOBAL_CONTEXT

    return constructObjectId(SAI_OBJECT_TYPE_SWITCH, switchIndex, switchIndex, m_globalContext);
}

sai_object_type_t VirtualObjectIdManager::saiObjectTypeQuery(
        _In_ sai_object_id_t objectId) const
{
    SWSS_LOG_ENTER();

    if (objectId == SAI_NULL_OBJECT_ID)
    {
        return SAI_OBJECT_TYPE_NULL;
    }

    sai_object_type_t objectType = (sai_object_type_t)(SAI_REDIS_GET_OBJECT_TYPE(objectId));

    if (objectType == SAI_OBJECT_TYPE_NULL || objectType >= SAI_OBJECT_TYPE_EXTENSIONS_MAX)
    {
        SWSS_LOG_ERROR("invalid object id 0x%s",
                sai_serialize_object_id(objectId).c_str());

        /*
         * We can't throw here, since it would give no meaningful message.
         * Throwing at one level up is better.
         */

        return SAI_OBJECT_TYPE_NULL;
    }

    // NOTE: we could also check:
    // - if object id has correct global context
    // - if object id has existing switch index
    // but then this method can't be made static

    return objectType;
}

void VirtualObjectIdManager::clear()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("clearing switch index set");

    m_switchIndexes.clear();
}

uint32_t VirtualObjectIdManager::allocateNewSwitchIndex()
{
    SWSS_LOG_ENTER();

    for (uint32_t index = 0; index < SAI_REDIS_SWITCH_INDEX_MAX; ++index)
    {
        if (m_switchIndexes.find(index) != m_switchIndexes.end())
            continue;

        m_switchIndexes.insert(index);

        SWSS_LOG_NOTICE("allocated new switch index 0x%x", index);

        return index;
    }

    SWSS_LOG_THROW("no more available switch indexes (used count is: %zu)", m_switchIndexes.size());
}

void VirtualObjectIdManager::releaseSwitchIndex(
        _In_ uint32_t index)
{
    SWSS_LOG_ENTER();

    auto it = m_switchIndexes.find(index);

    if (it == m_switchIndexes.end())
    {
        SWSS_LOG_THROW("switch index 0x%x is invalid! programming error", index);
    }

    m_switchIndexes.erase(it);

    SWSS_LOG_DEBUG("released switch index 0x%x", index);
}


sai_object_id_t VirtualObjectIdManager::allocateNewObjectId(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t switchId)
{
    SWSS_LOG_ENTER();

    if ((objectType <= SAI_OBJECT_TYPE_NULL) || (objectType >= SAI_OBJECT_TYPE_EXTENSIONS_MAX))
    {
        SWSS_LOG_THROW("invalid objct type: %d", objectType);
    }

    if (objectType == SAI_OBJECT_TYPE_SWITCH)
    {
        if (switchId)
        {
            SWSS_LOG_WARN("switchId should be NULL when allocating new switch, but value is %s",
                    sai_serialize_object_id(switchId).c_str());
        }

        uint32_t switchIndex = allocateNewSwitchIndex();

        // in case of object type switch, object index is same as switch index
        sai_object_id_t objectId = constructObjectId(SAI_OBJECT_TYPE_SWITCH, switchIndex, switchIndex, m_globalContext);

        SWSS_LOG_NOTICE("created SWITCH VID %s", 
                sai_serialize_object_id(objectId).c_str());

        return objectId;
    }

    sai_object_type_t switchObjectType = saiObjectTypeQuery(switchId);

    if (switchObjectType != SAI_OBJECT_TYPE_SWITCH)
    {
        SWSS_LOG_THROW("object type of switch %s is %s, should be SWITCH",
                sai_serialize_object_id(switchId).c_str(),
                sai_serialize_object_type(switchObjectType).c_str());
    }

    uint32_t switchIndex = (uint32_t)SAI_REDIS_GET_SWITCH_INDEX(switchId);

    // TODO this must be extracted to constructor and wrapped to some object
    uint64_t objectIndex = g_redisClient->incr("VIDCOUNTER"); // TODO use sairedis common

    if (objectIndex > SAI_REDIS_OBJECT_INDEX_MAX)
    {
        SWSS_LOG_THROW("no more object indexes available, given: 0x%lx but limit is 0x%llx",
                objectIndex,
                SAI_REDIS_OBJECT_INDEX_MAX);
    }

    sai_object_id_t objectId = constructObjectId(objectType, switchIndex, objectIndex, m_globalContext);

    SWSS_LOG_DEBUG("created VID %s",
            sai_serialize_object_id(objectId).c_str());

    return objectId;
}

void VirtualObjectIdManager::releaseObjectId(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (saiObjectTypeQuery(objectId) == SAI_OBJECT_TYPE_SWITCH)
    {
        releaseSwitchIndex((uint32_t)SAI_REDIS_GET_SWITCH_INDEX(objectId));
    }
}

sai_object_id_t VirtualObjectIdManager::constructObjectId(
        _In_ sai_object_type_t objectType,
        _In_ uint32_t switchIndex,
        _In_ uint64_t objectIndex,
        _In_ uint32_t globalContext)
{
    SWSS_LOG_ENTER();

    return (sai_object_id_t)(
            ((uint64_t)switchIndex << (SAI_REDIS_OBJECT_TYPE_BITS_SIZE + SAI_REDIS_GLOBAL_CONTEXT_BITS_SIZE + SAI_REDIS_OBJECT_INDEX_BITS_SIZE)) | 
            ((uint64_t)objectType << (SAI_REDIS_GLOBAL_CONTEXT_BITS_SIZE + SAI_REDIS_OBJECT_INDEX_BITS_SIZE)) | 
            ((uint64_t)globalContext << (SAI_REDIS_OBJECT_INDEX_BITS_SIZE)) | 
            objectIndex);
}

