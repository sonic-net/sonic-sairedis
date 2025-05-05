#include "NotificationTamTelTypeConfigChange.h"
#include "Meta.h"
#include "MetaTestSaiInterface.h"

#include "sairediscommon.h"
#include "sai_serialize.h"

#include <gtest/gtest.h>

using namespace sairedis;
using namespace saimeta;

TEST(NotificationTamTelTypeConfigChange, processMetadata)
{
    auto sai = std::make_shared<MetaTestSaiInterface>();
    auto meta = std::make_shared<Meta>(sai);

    sai_object_id_t switch_id;
    sai_object_id_t tam_id;

    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    sai->create(SAI_OBJECT_TYPE_SWITCH, &switch_id, SAI_NULL_OBJECT_ID, 1, &attr);
    sai->create(SAI_OBJECT_TYPE_TAM_TEL_TYPE, &tam_id, switch_id, 0, nullptr);

    std::stringstream ss;
    ss << "oid:0x" << std::hex << tam_id;

    NotificationTamTelTypeConfigChange n(ss.str());
    n.processMetadata(meta);
}
