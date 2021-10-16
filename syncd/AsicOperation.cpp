#include "AsicOperation.h"

#include "swss/logger.h"

using namespace syncd;

AsicOperation::AsicOperation(
        _In_ int id,
        _In_ sai_object_id_t vid,
        _In_ bool remove,
        _In_ std::shared_ptr<swss::KeyOpFieldsValuesTuple>& operation):
    m_opId(id),
    m_vid(vid),
    m_isRemove(remove),
    m_op(operation)
{
    SWSS_LOG_ENTER();

    // empty
}
