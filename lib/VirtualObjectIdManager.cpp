#include "VirtualObjectIdManager.h"

#include "meta/otai_serialize.h"
#include "swss/logger.h"
#include <inttypes.h>

extern "C" {
#include "otaimetadata.h"
}

#define OTAI_OBJECT_ID_BITS_SIZE (8 * sizeof(otai_object_id_t))

static_assert(OTAI_OBJECT_ID_BITS_SIZE == 64, "otai_object_id_t must have 64 bits");
static_assert(sizeof(otai_object_id_t) == sizeof(uint64_t), "OTAI object ID size should be uint64_t");

#define OTAI_REDIS_LINECARD_INDEX_BITS_SIZE ( 8 )
#define OTAI_REDIS_LINECARD_INDEX_MAX ( (1ULL << OTAI_REDIS_LINECARD_INDEX_BITS_SIZE) - 1 )
#define OTAI_REDIS_LINECARD_INDEX_MASK (OTAI_REDIS_LINECARD_INDEX_MAX)

#define OTAI_REDIS_GLOBAL_CONTEXT_BITS_SIZE ( 8 )
#define OTAI_REDIS_GLOBAL_CONTEXT_MAX ( (1ULL << OTAI_REDIS_GLOBAL_CONTEXT_BITS_SIZE) - 1 )
#define OTAI_REDIS_GLOBAL_CONTEXT_MASK (OTAI_REDIS_GLOBAL_CONTEXT_MAX)

#define OTAI_REDIS_OBJECT_TYPE_BITS_SIZE ( 8 )
#define OTAI_REDIS_OBJECT_TYPE_MAX ( (1ULL << OTAI_REDIS_OBJECT_TYPE_BITS_SIZE) - 1 )
#define OTAI_REDIS_OBJECT_TYPE_MASK (OTAI_REDIS_OBJECT_TYPE_MAX)

#define OTAI_REDIS_OBJECT_TYPE_EXTENSIONS_FLAG_BITS_SIZE ( 1 )
#define OTAI_REDIS_OBJECT_TYPE_EXTENSIONS_FLAG_MAX ( (1ULL << OTAI_REDIS_OBJECT_TYPE_EXTENSIONS_FLAG_BITS_SIZE) - 1 )
#define OTAI_REDIS_OBJECT_TYPE_EXTENSIONS_FLAG_MASK (OTAI_REDIS_OBJECT_TYPE_EXTENSIONS_FLAG_MAX)

#define OTAI_REDIS_OBJECT_INDEX_BITS_SIZE ( 39 )
#define OTAI_REDIS_OBJECT_INDEX_MAX ( (1ULL << OTAI_REDIS_OBJECT_INDEX_BITS_SIZE) - 1 )
#define OTAI_REDIS_OBJECT_INDEX_MASK (OTAI_REDIS_OBJECT_INDEX_MAX)

#define OTAI_REDIS_OBJECT_ID_BITS_SIZE (      \
        OTAI_REDIS_LINECARD_INDEX_BITS_SIZE +   \
        OTAI_REDIS_GLOBAL_CONTEXT_BITS_SIZE + \
        OTAI_REDIS_OBJECT_TYPE_EXTENSIONS_FLAG_BITS_SIZE + \
        OTAI_REDIS_OBJECT_TYPE_BITS_SIZE +    \
        OTAI_REDIS_OBJECT_INDEX_BITS_SIZE )

static_assert(OTAI_REDIS_OBJECT_ID_BITS_SIZE == OTAI_OBJECT_ID_BITS_SIZE, "redis object id size must be equal to OTAI object id size");

/*
 * This condition must be met, since we need to be able to encode OTAI object
 * type in object id on defined number of bits.
 */
static_assert(OTAI_OBJECT_TYPE_MAX < 256, "object type must be possible to encode on 1 byte");
//static_assert((OTAI_OBJECT_TYPE_EXTENSIONS_RANGE_END - OTAI_OBJECT_TYPE_EXTENSIONS_RANGE_START) < 256,
//        "extensions object type must be possible to encode on 1 byte");

/*
 * Current OBJECT ID format:
 *
 * bits 63..56 - switch index
 * bits 55..48 - OTAI object type
 * bits 47..40 - global context
 * bits 39..39 - object type extensions flag
 * bits 38..0  - object index
 *
 * So large number of bits is required, otherwise we would need to have map of
 * OID to some struct that will have all those values.  But having all this
 * information in OID itself is more convenient.
 *
 * To be backward compatible with previous otairedis, we will still encode base
 * object type on bit's 55..48, and extensions which will now start from range
 * 0x20000000, will be encoded from 0x0, but extensions flag will be set to 1.
 *
 * For example OTAI_OBJECT_TYPE_VIRTUAL_ROUTER oid will be encoded as 0x0003000000000001,
 * OTAI_OBJECT_TYPE_DASH_ACL_GROUP oid will be encoded as 0x0003008000000001.
 */

#define OTAI_REDIS_GET_OBJECT_INDEX(oid) \
    ( ((uint64_t)oid) & ( OTAI_REDIS_OBJECT_INDEX_MASK ) )

#define OTAI_REDIS_GET_OBJECT_TYPE_EXTENSIONS_FLAG(oid) \
    ( (((uint64_t)oid) >> (OTAI_REDIS_OBJECT_INDEX_BITS_SIZE) ) & ( OTAI_REDIS_OBJECT_TYPE_EXTENSIONS_FLAG_MAX ) )

#define OTAI_REDIS_GET_GLOBAL_CONTEXT(oid) \
    ( (((uint64_t)oid) >> (OTAI_REDIS_OBJECT_TYPE_EXTENSIONS_FLAG_BITS_SIZE + OTAI_REDIS_OBJECT_INDEX_BITS_SIZE) ) & ( OTAI_REDIS_GLOBAL_CONTEXT_MASK ) )

#define OTAI_REDIS_GET_OBJECT_TYPE(oid) \
    ( (((uint64_t)oid) >> ( OTAI_REDIS_GLOBAL_CONTEXT_BITS_SIZE + OTAI_REDIS_OBJECT_TYPE_EXTENSIONS_FLAG_BITS_SIZE + OTAI_REDIS_OBJECT_INDEX_BITS_SIZE) ) & ( OTAI_REDIS_OBJECT_TYPE_MASK ) )

#define OTAI_REDIS_GET_LINECARD_INDEX(oid) \
    ( (((uint64_t)oid) >> ( OTAI_REDIS_OBJECT_TYPE_BITS_SIZE + OTAI_REDIS_GLOBAL_CONTEXT_BITS_SIZE + OTAI_REDIS_OBJECT_TYPE_EXTENSIONS_FLAG_BITS_SIZE + OTAI_REDIS_OBJECT_INDEX_BITS_SIZE) ) & ( OTAI_REDIS_LINECARD_INDEX_MASK ) )

#define OTAI_REDIS_TEST_OID (0x012345e789abcdef)

static_assert(OTAI_REDIS_GET_LINECARD_INDEX(OTAI_REDIS_TEST_OID) == 0x01, "test switch index");
static_assert(OTAI_REDIS_GET_OBJECT_TYPE(OTAI_REDIS_TEST_OID) == 0x23, "test object type");
static_assert(OTAI_REDIS_GET_GLOBAL_CONTEXT(OTAI_REDIS_TEST_OID) == 0x45, "test global context");
static_assert(OTAI_REDIS_GET_OBJECT_TYPE_EXTENSIONS_FLAG(OTAI_REDIS_TEST_OID) == 0x1, "test object type extensions flag");
static_assert(OTAI_REDIS_GET_OBJECT_INDEX(OTAI_REDIS_TEST_OID) == 0x6789abcdef, "test object index");

using namespace otairedis;

VirtualObjectIdManager::VirtualObjectIdManager(
        _In_ uint32_t globalContext,
        _In_ std::shared_ptr<SwitchConfigContainer> scc,
        _In_ std::shared_ptr<OidIndexGenerator> oidIndexGenerator):
    m_globalContext(globalContext),
    m_container(scc),
    m_oidIndexGenerator(oidIndexGenerator)
{
    SWSS_LOG_ENTER();

    if (globalContext > OTAI_REDIS_GLOBAL_CONTEXT_MAX)
    {
        SWSS_LOG_THROW("specified globalContext(0x%x) > maximum global context 0x%llx",
                globalContext,
                OTAI_REDIS_GLOBAL_CONTEXT_MAX);
    }
}

otai_object_id_t VirtualObjectIdManager::otaiSwitchIdQuery(
        _In_ otai_object_id_t objectId) const
{
    SWSS_LOG_ENTER();

    if (objectId == OTAI_NULL_OBJECT_ID)
    {
        return OTAI_NULL_OBJECT_ID;
    }

    otai_object_type_t objectType = otaiObjectTypeQuery(objectId);

    if (objectType == OTAI_OBJECT_TYPE_NULL)
    {
        // TODO don't throw, those 2 functions should never throw
        // it doesn't matter whether oid is correct, that will be validated
        // in metadata
        SWSS_LOG_THROW("invalid object type of oid %s",
                otai_serialize_object_id(objectId).c_str());
    }

    if (objectType == OTAI_OBJECT_TYPE_LINECARD)
    {
        return objectId;
    }

    // NOTE: we could also check:
    // - if object id has correct global context
    // - if object id has existing switch index
    // but then this method can't be made static

    uint32_t switchIndex = (uint32_t)OTAI_REDIS_GET_LINECARD_INDEX(objectId);

    uint32_t globalContext = (uint32_t)OTAI_REDIS_GET_GLOBAL_CONTEXT(objectId);

    return constructObjectId(OTAI_OBJECT_TYPE_LINECARD, switchIndex, switchIndex, globalContext);
}

otai_object_type_t VirtualObjectIdManager::otaiObjectTypeQuery(
        _In_ otai_object_id_t objectId) const
{
    SWSS_LOG_ENTER();

    if (objectId == OTAI_NULL_OBJECT_ID)
    {
        return OTAI_OBJECT_TYPE_NULL;
    }

    otai_object_type_t objectType = 
         (otai_object_type_t)(OTAI_REDIS_GET_OBJECT_TYPE(objectId));

    if (otai_metadata_is_object_type_valid(objectType) == false)
    {
        SWSS_LOG_ERROR("invalid object id %s",
                otai_serialize_object_id(objectId).c_str());

        /*
         * We can't throw here, since it would give no meaningful message.
         * Throwing at one level up is better.
         */

        return OTAI_OBJECT_TYPE_NULL;
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

otai_object_id_t VirtualObjectIdManager::allocateNewObjectId(
        _In_ otai_object_type_t objectType,
        _In_ otai_object_id_t switchId)
{
    SWSS_LOG_ENTER();

    if (otai_metadata_is_object_type_valid(objectType) == false)
    {
        SWSS_LOG_THROW("invalid object type: %d", objectType);
    }

    if (objectType == OTAI_OBJECT_TYPE_LINECARD)
    {
        SWSS_LOG_THROW("this function can't be used to allocate switch id");
    }

    otai_object_type_t switchObjectType = otaiObjectTypeQuery(switchId);

    if (switchObjectType != OTAI_OBJECT_TYPE_LINECARD)
    {
        SWSS_LOG_THROW("object type of switch %s is %s, should be SWITCH",
                otai_serialize_object_id(switchId).c_str(),
                otai_serialize_object_type(switchObjectType).c_str());
    }

    uint32_t switchIndex = (uint32_t)OTAI_REDIS_GET_LINECARD_INDEX(switchId);

    uint64_t objectIndex = m_oidIndexGenerator->increment(); // get new object index

    const uint64_t indexMax = OTAI_REDIS_OBJECT_INDEX_MAX;

    if (objectIndex > OTAI_REDIS_OBJECT_INDEX_MAX)
    {
        SWSS_LOG_THROW("no more object indexes available, given: 0x%" PRIx64 " but limit is 0x%" PRIx64 " ",
                objectIndex,
                indexMax);
    }

    otai_object_id_t objectId = constructObjectId(objectType, switchIndex, objectIndex, m_globalContext);

    SWSS_LOG_DEBUG("created VID %s",
            otai_serialize_object_id(objectId).c_str());

    return objectId;
}

otai_object_id_t VirtualObjectIdManager::allocateNewSwitchObjectId(
        _In_ const std::string& hardwareInfo)
{
    SWSS_LOG_ENTER();

    auto config = m_container->getConfig(hardwareInfo);

    if (config == nullptr)
    {
        SWSS_LOG_ERROR("no switch config for hardware info: '%s'", hardwareInfo.c_str());

        return OTAI_NULL_OBJECT_ID;
    }

    uint32_t switchIndex = config->m_switchIndex;

    if (switchIndex > OTAI_REDIS_LINECARD_INDEX_MAX)
    {
        SWSS_LOG_THROW("switch index %u > %llu (max)", switchIndex, OTAI_REDIS_LINECARD_INDEX_MAX);
    }

    if (m_switchIndexes.find(switchIndex) != m_switchIndexes.end())
    {
        // this could happen, if we first create switch with INIT=true, and
        // then with INIT=false but we should have other way to not double call
        // allocate to obtain existing switch ID, like from switch container

        SWSS_LOG_WARN("switch index %u already allocated, double call to allocate!", switchIndex);
    }

    m_switchIndexes.insert(switchIndex);

    otai_object_id_t objectId = constructObjectId(OTAI_OBJECT_TYPE_LINECARD, switchIndex, switchIndex, m_globalContext);

    SWSS_LOG_NOTICE("created SWITCH VID %s for hwinfo: '%s'",
            otai_serialize_object_id(objectId).c_str(),
            hardwareInfo.c_str());

    return objectId;
}


void VirtualObjectIdManager::releaseObjectId(
        _In_ otai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (otaiObjectTypeQuery(objectId) == OTAI_OBJECT_TYPE_LINECARD)
    {
        releaseSwitchIndex((uint32_t)OTAI_REDIS_GET_LINECARD_INDEX(objectId));
    }
}

otai_object_id_t VirtualObjectIdManager::constructObjectId(
        _In_ otai_object_type_t objectType,
        _In_ uint32_t switchIndex,
        _In_ uint64_t objectIndex,
        _In_ uint32_t globalContext)
{
    SWSS_LOG_ENTER();

    if (otai_metadata_is_object_type_valid(objectType) == false)
    {
        SWSS_LOG_THROW("FATAL: invalid object type (0x%x), logic error, this is a bug!", objectType);
    }

    uint64_t extensionsFlag = 0;

    return (otai_object_id_t)(
            ((uint64_t)switchIndex << (OTAI_REDIS_OBJECT_TYPE_BITS_SIZE + OTAI_REDIS_GLOBAL_CONTEXT_BITS_SIZE + OTAI_REDIS_OBJECT_TYPE_EXTENSIONS_FLAG_BITS_SIZE + OTAI_REDIS_OBJECT_INDEX_BITS_SIZE)) |
            ((uint64_t)objectType << (OTAI_REDIS_GLOBAL_CONTEXT_BITS_SIZE + OTAI_REDIS_OBJECT_TYPE_EXTENSIONS_FLAG_BITS_SIZE + OTAI_REDIS_OBJECT_INDEX_BITS_SIZE)) |
            ((uint64_t)globalContext << (OTAI_REDIS_OBJECT_TYPE_EXTENSIONS_FLAG_BITS_SIZE + OTAI_REDIS_OBJECT_INDEX_BITS_SIZE)) |
            ((uint64_t)extensionsFlag << (OTAI_REDIS_OBJECT_INDEX_BITS_SIZE)) |
            objectIndex);
}

otai_object_id_t VirtualObjectIdManager::switchIdQuery(
        _In_ otai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (objectId == OTAI_NULL_OBJECT_ID)
    {
        return OTAI_NULL_OBJECT_ID;
    }

    otai_object_type_t objectType = objectTypeQuery(objectId);

    if (objectType == OTAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_ERROR("invalid object type of oid %s",
                otai_serialize_object_id(objectId).c_str());

        return OTAI_NULL_OBJECT_ID;
    }

    if (objectType == OTAI_OBJECT_TYPE_LINECARD)
    {
        return objectId;
    }

    uint32_t switchIndex = (uint32_t)OTAI_REDIS_GET_LINECARD_INDEX(objectId);
    uint32_t globalContext = (uint32_t)OTAI_REDIS_GET_GLOBAL_CONTEXT(objectId);

    return constructObjectId(OTAI_OBJECT_TYPE_LINECARD, switchIndex, switchIndex, globalContext);
}

otai_object_type_t VirtualObjectIdManager::objectTypeQuery(
        _In_ otai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (objectId == OTAI_NULL_OBJECT_ID)
    {
        return OTAI_OBJECT_TYPE_NULL;
    }

    otai_object_type_t objectType = 
         (otai_object_type_t)(OTAI_REDIS_GET_OBJECT_TYPE(objectId));

    if (!otai_metadata_is_object_type_valid(objectType))
    {
        SWSS_LOG_ERROR("invalid object id %s",
                otai_serialize_object_id(objectId).c_str());

        return OTAI_OBJECT_TYPE_NULL;
    }

    return objectType;
}

uint32_t VirtualObjectIdManager::getSwitchIndex(
        _In_ otai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    auto switchId = switchIdQuery(objectId);

    return (uint32_t)OTAI_REDIS_GET_LINECARD_INDEX(switchId);
}

uint32_t VirtualObjectIdManager::getGlobalContext(
        _In_ otai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    auto switchId = switchIdQuery(objectId);

    return (uint32_t)OTAI_REDIS_GET_GLOBAL_CONTEXT(switchId);
}

uint64_t VirtualObjectIdManager::getObjectIndex(
        _In_ otai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    return (uint32_t)OTAI_REDIS_GET_OBJECT_INDEX(objectId);
}

otai_object_id_t VirtualObjectIdManager::updateObjectIndex(
        _In_ otai_object_id_t objectId,
        _In_ uint64_t objectIndex)
{
    SWSS_LOG_ENTER();

    if (objectId == OTAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_THROW("can't update object index on NULL_OBJECT_ID");
    }

    if (objectIndex > OTAI_REDIS_OBJECT_INDEX_MAX)
    {
        SWSS_LOG_THROW("object index %lu over maximum %llu", objectIndex, OTAI_REDIS_OBJECT_INDEX_MAX);
    }

    otai_object_type_t objectType = objectTypeQuery(objectId);

    if (objectType == OTAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_THROW("invalid object type of oid %s",
                otai_serialize_object_id(objectId).c_str());
    }

    uint32_t switchIndex = (uint32_t)OTAI_REDIS_GET_LINECARD_INDEX(objectId);
    uint32_t globalContext = (uint32_t)OTAI_REDIS_GET_GLOBAL_CONTEXT(objectId);

    return constructObjectId(objectType, switchIndex, objectIndex, globalContext);
}
