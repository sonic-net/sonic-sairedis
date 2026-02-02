#include "SaiObj.h"

#include "swss/logger.h"

using namespace syncd;

std::string ObjectStatus::sai_serialize_object_status(
        _In_ sai_object_status_t os)
{
    SWSS_LOG_ENTER();

    switch (os)
    {
        case SAI_OBJECT_STATUS_NOT_PROCESSED:
            return "not-processed";

        case SAI_OBJECT_STATUS_MATCHED:
             return "matched";

        case SAI_OBJECT_STATUS_REMOVED:
             return "removed";

        case SAI_OBJECT_STATUS_FINAL:
             return "final";

        default:
             SWSS_LOG_THROW("unknown object status: %d", os);
    }
}

SaiObj::SaiObj():
    m_createdObject(false),
    m_objectStatus(SAI_OBJECT_STATUS_NOT_PROCESSED)
{
    SWSS_LOG_ENTER();

    // empty
}

bool SaiObj::isOidObject() const
{
    SWSS_LOG_ENTER();

    return m_info->isobjectid;
}

const std::unordered_map<sai_attr_id_t, std::shared_ptr<SaiAttr>>& SaiObj::getAllAttributes() const
{
    SWSS_LOG_ENTER();

    return m_attrs;
}

std::shared_ptr<const SaiAttr> SaiObj::getSaiAttr(
        _In_ sai_attr_id_t id) const
{
    SWSS_LOG_ENTER();

    auto it = m_attrs.find(id);

    if (it == m_attrs.end())
    {
        for (const auto &ita: m_attrs)
        {
            const auto &a = ita.second;

            SWSS_LOG_ERROR("%s: %s", a->getStrAttrId().c_str(), a->getStrAttrValue().c_str());
        }

        SWSS_LOG_THROW("object %s has no attribute %d", m_str_object_id.c_str(), id);
    }

    return it->second;
}

std::shared_ptr<const SaiAttr> SaiObj::tryGetSaiAttr(
        _In_ sai_attr_id_t id) const
{
    SWSS_LOG_ENTER();

    auto it = m_attrs.find(id);

    if (it == m_attrs.end())
        return nullptr;

    return it->second;
}

void SaiObj::setObjectStatus(
        _In_ sai_object_status_t objectStatus)
{
    SWSS_LOG_ENTER();

    m_objectStatus = objectStatus;
}

sai_object_status_t SaiObj::getObjectStatus() const
{
    SWSS_LOG_ENTER();

    return m_objectStatus;
}

sai_object_type_t SaiObj::getObjectType() const
{
    SWSS_LOG_ENTER();

    return m_meta_key.objecttype;
}

void SaiObj::setAttr(
        _In_ const std::shared_ptr<SaiAttr> &attr)
{
    SWSS_LOG_ENTER();

    /*
     * Always create a deep copy to prevent double-free crashes during
     * warm restart. During applyView(), multiple AsicView objects exist
     * in a vector, and the same SaiAttr pointer can be stored across
     * different SaiObj instances in different AsicView objects. When the
     * vector is destroyed, each AsicView tries to free the same SaiAttr,
     * causing double-free (see bug #24372).
     *
     * Memory trade-off: This uses more memory but guarantees correctness.
     * Each SaiObj owns independent copies of its attributes.
     */

    auto attrCopy = std::make_shared<SaiAttr>(*attr);
    m_attrs[attr->getSaiAttr()->id] = attrCopy;
}

bool SaiObj::hasAttr(
        _In_ sai_attr_id_t id) const
{
    SWSS_LOG_ENTER();

    return m_attrs.find(id) != m_attrs.end();
}

sai_object_id_t SaiObj::getVid() const
{
    SWSS_LOG_ENTER();

    if (isOidObject())
    {
        return m_meta_key.objectkey.key.object_id;
    }

    SWSS_LOG_THROW("object %s it not object id type", m_str_object_id.c_str());
}
