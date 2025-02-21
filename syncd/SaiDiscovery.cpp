#include "SaiDiscovery.h"

#include "swss/logger.h"

#include "meta/sai_serialize.h"

using namespace syncd;

/**
 * @def SAI_DISCOVERY_LIST_MAX_ELEMENTS
 *
 * Defines maximum elements that can be obtained from the OID list when
 * performing list attribute query (discovery) on the switch.
 *
 * This value will be used to allocate memory on the stack for obtaining object
 * list, and should be big enough to obtain list for all ports on the switch
 * and vlan members.
 */
#define SAI_DISCOVERY_LIST_MAX_ELEMENTS 1024

SaiDiscovery::SaiDiscovery(
        _In_ std::shared_ptr<sairedis::SaiInterface> sai):
    m_sai(sai)
{
    SWSS_LOG_ENTER();

    // empty
}

SaiDiscovery::~SaiDiscovery()
{
    SWSS_LOG_ENTER();

    // empty
}

void SaiDiscovery::discover(
        _In_ sai_object_id_t rid,
        _Inout_ std::set<sai_object_id_t> &discovered)
{
    SWSS_LOG_ENTER();

    /*
     * NOTE: This method is only good after switch init since we are making
     * assumptions that there are no ACL after initialization.
     *
     * NOTE: Input set could be a map of sets, this way we will also have
     * dependency on each oid.
     */

    if (rid == SAI_NULL_OBJECT_ID)
    {
        return;
    }

    if (discovered.find(rid) != discovered.end())
    {
        return;
    }

    sai_object_type_t ot = m_sai->objectTypeQuery(rid);

    if (ot == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_THROW("objectTypeQuery: rid %s returned NULL object type",
                sai_serialize_object_id(rid).c_str());
    }

    SWSS_LOG_DEBUG("processing %s: %s",
            sai_serialize_object_id(rid).c_str(),
            sai_serialize_object_type(ot).c_str());

    /*
     * We will ignore STP ports by now, since when removing bridge port, then
     * associated stp port is automatically removed, and we don't use STP in
     * out solution.  This causing inconsistency with redis ASIC view vs
     * actual ASIC asic state.
     *
     * TODO: This needs to be solved by sending discovered state to sairedis
     * metadata db for reference count.
     *
     * XXX: workaround
     */

    if (ot != SAI_OBJECT_TYPE_STP_PORT)
    {
        discovered.insert(rid);
    }

#ifdef SKIP_SAI_PORT_DISCOVERY
    if (ot == SAI_OBJECT_TYPE_PORT)
    {
        return;
    }
#endif

    const sai_object_type_info_t *info = sai_metadata_get_object_type_info(ot);

    /*
     * We will query only oid object types
     * then we don't need meta key, but we need to add to metadata
     * pointers to only generic functions.
     */

    sai_object_meta_key_t mk = { .objecttype = ot, .objectkey = { .key = { .object_id = rid } } };

    for (int idx = 0; info->attrmetadata[idx] != NULL; ++idx)
    {
        const sai_attr_metadata_t *md = info->attrmetadata[idx];

        /*
         * Note that we don't care about ACL object id's since
         * we assume that there are no ACLs on switch after init.
         */

        sai_attribute_t attr;

        attr.id = md->attrid;

        if (md->attrvaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_ID)
        {
            if (md->defaultvaluetype == SAI_DEFAULT_VALUE_TYPE_CONST)
            {
                /*
                 * This means that default value for this object is
                 * SAI_NULL_OBJECT_ID, since this is discovery after
                 * create, we don't need to query this attribute.
                 */

                //continue;
            }

            if (md->objecttype == SAI_OBJECT_TYPE_STP &&
                    md->attrid == SAI_STP_ATTR_BRIDGE_ID)
            {
                // XXX workaround (for mlnx)
                SWSS_LOG_WARN("skipping since it causes crash: %s", md->attridname);
                continue;
            }

            if (md->objecttype == SAI_OBJECT_TYPE_BRIDGE_PORT)
            {
                if (md->attrid == SAI_BRIDGE_PORT_ATTR_TUNNEL_ID ||
                        md->attrid == SAI_BRIDGE_PORT_ATTR_RIF_ID)
                {
                    /*
                     * We know that bridge port is bound on PORT, no need
                     * to query those attributes.
                     */

                    continue;
                }
            }

            SWSS_LOG_DEBUG("getting %s for %s", md->attridname,
                    sai_serialize_object_id(rid).c_str());

            sai_status_t status = m_sai->get(mk.objecttype, mk.objectkey.key.object_id, 1, &attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                /*
                 * We failed to get value, maybe it's not supported ?
                 */

                SWSS_LOG_INFO("%s: %s on %s",
                        md->attridname,
                        sai_serialize_status(status).c_str(),
                        sai_serialize_object_id(rid).c_str());

                continue;
            }

            m_defaultOidMap[rid][attr.id] = attr.value.oid;

            if (!md->allownullobjectid && attr.value.oid == SAI_NULL_OBJECT_ID)
            {
                // SWSS_LOG_WARN("got null on %s, but not allowed", md->attridname);
            }

            if (attr.value.oid != SAI_NULL_OBJECT_ID)
            {
                ot = m_sai->objectTypeQuery(attr.value.oid);

                if (ot == SAI_OBJECT_TYPE_NULL)
                {
                    SWSS_LOG_THROW("when query %s (on %s RID %s) got value %s objectTypeQuery returned NULL object type",
                            md->attridname,
                            sai_serialize_object_type(md->objecttype).c_str(),
                            sai_serialize_object_id(rid).c_str(),
                            sai_serialize_object_id(attr.value.oid).c_str());
                }
            }

            discover(attr.value.oid, discovered); // recursion
        }
        else if (md->attrvaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_LIST)
        {
            if (md->defaultvaluetype == SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST)
            {
                /*
                 * This means that default value for this object is
                 * empty list, since this is discovery after
                 * create, we don't need to query this attribute.
                 */

                //continue;
            }

            SWSS_LOG_DEBUG("getting %s for %s", md->attridname,
                    sai_serialize_object_id(rid).c_str());

            sai_object_id_t local[SAI_DISCOVERY_LIST_MAX_ELEMENTS];

            attr.value.objlist.count = SAI_DISCOVERY_LIST_MAX_ELEMENTS;
            attr.value.objlist.list = local;

            sai_status_t status = m_sai->get(mk.objecttype, mk.objectkey.key.object_id, 1, &attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                /*
                 * We failed to get value, maybe it's not supported ?
                 */

                SWSS_LOG_INFO("%s: %s on %s",
                        md->attridname,
                        sai_serialize_status(status).c_str(),
                        sai_serialize_object_id(rid).c_str());

                continue;
            }

            SWSS_LOG_DEBUG("list count %s %u", md->attridname, attr.value.objlist.count);

            for (uint32_t i = 0; i < attr.value.objlist.count; ++i)
            {
                sai_object_id_t oid = attr.value.objlist.list[i];

                ot = m_sai->objectTypeQuery(oid);

                if (ot == SAI_OBJECT_TYPE_NULL)
                {
                    SWSS_LOG_THROW("when query %s (on %s RID %s) got value %s objectTypeQuery returned NULL object type",
                            md->attridname,
                            sai_serialize_object_type(md->objecttype).c_str(),
                            sai_serialize_object_id(rid).c_str(),
                            sai_serialize_object_id(oid).c_str());
                }

                discover(oid, discovered); // recursion
            }
        }
    }
}

std::set<sai_object_id_t> SaiDiscovery::discover(
        _In_ sai_object_id_t startRid)
{
    SWSS_LOG_ENTER();

    /*
     * Preform discovery on the switch to obtain ASIC view of
     * objects that are created internally.
     */

    m_defaultOidMap.clear();

    std::set<sai_object_id_t> discovered_rids;

    {
        SWSS_LOG_TIMER("discover");

        auto levels = getApiLogLevel();

        setApiLogLevel(SAI_LOG_LEVEL_CRITICAL);

        discover(startRid, discovered_rids);

        setApiLogLevel(levels);
    }

    SWSS_LOG_NOTICE("discovered objects count: %zu", discovered_rids.size());

    std::map<sai_object_type_t, int> map;

    for (sai_object_id_t rid: discovered_rids)
    {
        /*
         * We don't need to check for null since saiDiscovery already checked
         * that.
         */

        map[m_sai->objectTypeQuery(rid)]++;
    }

    for (const auto &p: map)
    {
        SWSS_LOG_NOTICE("%s: %d", sai_serialize_object_type(p.first).c_str(), p.second);
    }

    return discovered_rids;
}

const SaiDiscovery::DefaultOidMap& SaiDiscovery::getDefaultOidMap() const
{
    SWSS_LOG_ENTER();

    return m_defaultOidMap;
}

void SaiDiscovery::setApiLogLevel(
        _In_ sai_log_level_t logLevel)
{
    SWSS_LOG_ENTER();

    // We start from 1 since 0 is SAI_API_UNSPECIFIED.

    for (uint32_t idx = 1; idx < sai_metadata_enum_sai_api_t.valuescount; ++idx)
    {
        sai_status_t status = m_sai->logSet((sai_api_t)sai_metadata_enum_sai_api_t.values[idx], logLevel);

        if (status == SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_INFO("setting SAI loglevel %s on %s",
                    sai_serialize_log_level(logLevel).c_str(),
                    sai_serialize_api((sai_api_t)sai_metadata_enum_sai_api_t.values[idx]).c_str());
        }
        else
        {
            SWSS_LOG_INFO("set loglevel failed: %s", sai_serialize_status(status).c_str());
        }
    }
}

void SaiDiscovery::setApiLogLevel(
        _In_ const std::map<sai_api_t, sai_log_level_t>& levels)
{
    SWSS_LOG_ENTER();

    // We start from 1 since 0 is SAI_API_UNSPECIFIED.

    for (uint32_t idx = 1; idx < sai_metadata_enum_sai_api_t.valuescount; ++idx)
    {
        auto it = levels.find((sai_api_t)sai_metadata_enum_sai_api_t.values[idx]);

        sai_log_level_t logLevel = (it == levels.end()) ? SAI_LOG_LEVEL_NOTICE : it->second;

        sai_status_t status = m_sai->logSet((sai_api_t)sai_metadata_enum_sai_api_t.values[idx], logLevel);

        if (status == SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_INFO("setting SAI loglevel %s on %s",
                    sai_serialize_log_level(logLevel).c_str(),
                    sai_serialize_api((sai_api_t)sai_metadata_enum_sai_api_t.values[idx]).c_str());
        }
        else
        {
            SWSS_LOG_INFO("set loglevel failed: %s", sai_serialize_status(status).c_str());
        }
    }
}

std::map<sai_api_t, sai_log_level_t> SaiDiscovery::getApiLogLevel()
{
    SWSS_LOG_ENTER();

    std::map<sai_api_t, sai_log_level_t> levels;

    // We start from 1 since 0 is SAI_API_UNSPECIFIED.

    for (uint32_t idx = 1; idx < sai_metadata_enum_sai_api_t.valuescount; ++idx)
    {
        levels[(sai_api_t)sai_metadata_enum_sai_api_t.values[idx]] = m_sai->logGet((sai_api_t)sai_metadata_enum_sai_api_t.values[idx]);
    }

    return levels;
}
