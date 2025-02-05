#include "AttrVersionChecker.h"

#include "swss/logger.h"

using namespace syncd;

AttrVersionChecker::AttrVersionChecker():
    m_enabled(false),
    m_saiApiVersion(SAI_VERSION(0,0,0))
{
    SWSS_LOG_ENTER();

    // empty
}

void AttrVersionChecker::enable(
        _In_ bool enable)
{
    SWSS_LOG_ENTER();

    m_enabled = enable;
}

void AttrVersionChecker::setSaiApiVersion(
        _In_ sai_api_version_t version)
{
    SWSS_LOG_ENTER();

    m_saiApiVersion = version;
}

void AttrVersionChecker::reset()
{
    SWSS_LOG_ENTER();

    m_visitedAttributes.clear();
}

bool AttrVersionChecker::isSufficientVersion(
        _In_ const sai_attr_metadata_t *md)
{
    SWSS_LOG_ENTER();

    if (md == nullptr)
    {
        SWSS_LOG_ERROR("md is NULL");

        return false;
    }

    if (!m_enabled)
    {
        return true;
    }

    if (SAI_METADATA_HAVE_ATTR_VERSION == 0)
    {
        // metadata does not contain attr versions, no check will be preformed
        return true;
    }

    // check attr version if metadata have version defined

    if (m_saiApiVersion > md->apiversion)
    {
        // ok, SAI version is bigger than attribute release version

        return true;
    }

    if (m_saiApiVersion < md->apiversion)
    {
        // skip, SAI version is not sufficient

        if (m_visitedAttributes.find(md->attridname) == m_visitedAttributes.end())
        {
            m_visitedAttributes.insert(md->attridname);

            // log only once

            SWSS_LOG_WARN("SAI version %lu, not sufficient to discover %s", m_saiApiVersion, md->attridname);
        }

        return false;
    }

    // m_saiApiVersion == md->apiversion

    if (md->nextrelease == false)
    {
        // ok, SAI version is equal to attribute version
        return true;
    }

    // next release == true

    if (m_visitedAttributes.find(md->attridname) == m_visitedAttributes.end())
    {
        m_visitedAttributes.insert(md->attridname);

        // warn only once

        SWSS_LOG_WARN("%s is ment for next release after %lu, will not discover", md->attridname, m_saiApiVersion);
    }

    return false;
}
