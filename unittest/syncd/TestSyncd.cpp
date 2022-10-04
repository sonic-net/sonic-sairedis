#include "Syncd.h"
#include "VendorSai.h"
#include "CommandLineOptions.h"
#include "CommandLineOptionsParser.h"
#include "SaiAttr.h"

#include "swss/table.h"

#include "meta/sai_serialize.h"

#include <gtest/gtest.h>

using namespace syncd;
using namespace saimeta;

TEST(Syncd, snoopGetResponse)
{
    auto commandLineOptions = CommandLineOptionsParser::parseCommandLine(0, NULL);

    auto vendorSai = std::make_shared<VendorSai>();

    auto syncd = std::make_shared<Syncd>(vendorSai, commandLineOptions, false);

    sai_attribute_t attr;

    attr.id = SAI_PORT_ATTR_SPEED;
    attr.value.u32 = 25000;

    syncd->snoopGetResponse(
            SAI_OBJECT_TYPE_PORT,
            sai_serialize_object_id(0x1000000000007L),
            1,
            &attr);

    swss::DBConnector db("ASIC_DB", 0, true);

    swss::Table t(&db, "ASIC_STATE");

    sai_object_meta_key_t metaKey;

    metaKey.objecttype = SAI_OBJECT_TYPE_PORT;
    metaKey.objectkey.key.object_id = 0x1000000000007L;

    std::string key = sai_serialize_object_meta_key(metaKey);

    std::string value;

    bool r = t.hget(key, "SAI_PORT_ATTR_SPEED", value);

    EXPECT_FALSE(r);
}
