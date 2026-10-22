#include "SaiConfig.h"

#include "swss/dbconnector.h"
#include "swss/table.h"
#include "swss/redisreply.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

#include <cinttypes>
#include <cstdio>
#include <unistd.h>
#include <fstream>
#include <stdexcept>

using namespace syncd;

// This binary is built without access control, so private members
// of SaiConfig are reachable from here. Needs a redis with the SONiC database_config, the same
// requirement as the other unittest directories.

class SaiConfigTest : public ::testing::Test
{
    protected:

        void SetUp() override
        {
            SWSS_LOG_ENTER();

            m_db = std::make_shared<swss::DBConnector>("ASIC_DB", 0);
            flush();

            m_cfg = std::make_unique<SaiConfig>();

            m_switch = oidStr(m_cfg->m_saiConfigManager->getSwitchId());

            // What syncd publishes once it is up; requireSyncdReady() keys on it.
            m_db->hset("VIDTORID", m_switch, "oid:0x1");

            m_port = seed("SAI_OBJECT_TYPE_PORT", "oid:0x1000000000001", "SAI_PORT_ATTR_MTU", "9100");
            m_vr = seed("SAI_OBJECT_TYPE_VIRTUAL_ROUTER", "oid:0x3000000000002", "SAI_VIRTUAL_ROUTER_ATTR_ADMIN_V4_STATE", "true");
            m_rif = seed("SAI_OBJECT_TYPE_ROUTER_INTERFACE", "oid:0x6000000000003", "SAI_ROUTER_INTERFACE_ATTR_TYPE", "SAI_ROUTER_INTERFACE_TYPE_PORT");
            m_nh = seed("SAI_OBJECT_TYPE_NEXT_HOP", "oid:0x4000000000004", "SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP");
            m_mirror = seed("SAI_OBJECT_TYPE_MIRROR_SESSION", "oid:0xe000000000005", "SAI_MIRROR_SESSION_ATTR_TYPE", "SAI_MIRROR_SESSION_TYPE_LOCAL");
        }

        void TearDown() override
        {
            SWSS_LOG_ENTER();

            flush();
            clearConfigDb();
        }

        void flush()
        {
            SWSS_LOG_ENTER();

            swss::RedisCommand cmd;
            cmd.format("FLUSHDB");
            swss::RedisReply reply(m_db.get(), cmd, REDIS_REPLY_STATUS);
        }

        static std::string oidStr(sai_object_id_t oid)
        {
            SWSS_LOG_ENTER();

            char buf[32];
            snprintf(buf, sizeof(buf), "oid:0x%" PRIx64, oid);
            return buf;
        }

        std::string seed(const std::string &type, const std::string &oid, const std::string &attr, const std::string &value)
        {
            SWSS_LOG_ENTER();

            m_db->hset("ASIC_STATE:" + type + ":" + oid, attr, value);
            return oid;
        }

        std::string routeKey(const std::string &dest, const std::string &vr)
        {
            SWSS_LOG_ENTER();

            return "{\"dest\":\"" + dest + "\",\"switch_id\":\"" + m_switch + "\",\"vr\":\"" + vr + "\"}";
        }

        void validate(const std::string &type, const std::string &id,
                      const std::vector<std::pair<std::string, std::string>> &attrs,
                      const std::string &op)
        {
            SWSS_LOG_ENTER();

            m_cfg->validateRequest(type, id, attrs, op);
        }

        // Write a JSON file and run it through processJsonFile() in --no-wait
        // mode: requests land on the redis queue and nothing waits for syncd.
        bool runJson(const std::string &json)
        {
            SWSS_LOG_ENTER();

            std::string path = "/tmp/saiconfig-test-" + std::to_string(getpid()) + ".json";
            std::ofstream f(path);
            f << json;
            f.close();

            m_cfg->m_noWait = true;

            bool ok = m_cfg->processJsonFile(path);

            remove(path.c_str());

            return ok;
        }

        long queueLength()
        {
            SWSS_LOG_ENTER();

            swss::RedisCommand cmd;
            cmd.format("LLEN ASIC_STATE_KEY_VALUE_OP_QUEUE");
            swss::RedisReply reply(m_db.get(), cmd, REDIS_REPLY_INTEGER);
            return reply.getContext()->integer;
        }

        void setConfigDb(const std::string &table, const std::string &key, const std::string &field, const std::string &value)
        {
            SWSS_LOG_ENTER();

            swss::DBConnector cfg("CONFIG_DB", 0);
            swss::Table t(&cfg, table);
            t.hset(key, field, value);
        }

        void clearConfigDb()
        {
            SWSS_LOG_ENTER();

            swss::DBConnector cfg("CONFIG_DB", 0);
            swss::RedisCommand cmd;
            cmd.format("FLUSHDB");
            swss::RedisReply reply(&cfg, cmd, REDIS_REPLY_STATUS);
        }

        std::shared_ptr<swss::DBConnector> m_db;
        std::unique_ptr<SaiConfig> m_cfg;
        std::string m_switch, m_port, m_vr, m_rif, m_nh, m_mirror;
};


TEST_F(SaiConfigTest, objectTypeMapping)
{
    EXPECT_EQ(m_cfg->getSaiObjectType("acl_table"), "SAI_OBJECT_TYPE_ACL_TABLE");
    EXPECT_EQ(m_cfg->getSaiObjectType("Virtual_Router"), "SAI_OBJECT_TYPE_VIRTUAL_ROUTER");

    EXPECT_EQ(m_cfg->m_saiConfigManager->generateSaiObjectId("not_a_sai_object"), "");
    EXPECT_EQ(m_cfg->m_saiConfigManager->generateSaiObjectId("switch"), m_switch);

    auto vr = m_cfg->m_saiConfigManager->generateSaiObjectId("virtual_router");
    EXPECT_EQ(m_cfg->m_saiConfigManager->getSaiObjectTypeFromId(vr), "SAI_OBJECT_TYPE_VIRTUAL_ROUTER");
}

TEST_F(SaiConfigTest, oidAndVariableSyntax)
{
    EXPECT_TRUE(m_cfg->isOidFormat("oid:0x1000000000001"));
    EXPECT_FALSE(m_cfg->isOidFormat("0x1000000000001"));
    EXPECT_FALSE(m_cfg->isOidFormat("oid:0x"));
    EXPECT_FALSE(m_cfg->isOidFormat("$port"));

    EXPECT_TRUE(m_cfg->isVariableReference("$port"));
    EXPECT_FALSE(m_cfg->isVariableReference("port"));

    EXPECT_EQ(m_cfg->resolveOidValue("oid:0x1000000000001"), "oid:0x1000000000001");
    EXPECT_EQ(m_cfg->resolveOidValue("1000000000001"), "");
    EXPECT_EQ(m_cfg->resolveOidValue(""), "");
}

TEST_F(SaiConfigTest, resolveVariables)
{
    m_cfg->m_objectIdMap["vr"] = "oid:0xa";
    m_cfg->m_objectIdMap["vr10"] = "oid:0xb";

    EXPECT_EQ(m_cfg->resolveVariables("plain"), "plain");
    EXPECT_EQ(m_cfg->resolveVariables("$vr"), "oid:0xa");
    EXPECT_EQ(m_cfg->resolveVariables("$vr10"), "oid:0xb");        // longest name wins, no prefix clash
    EXPECT_EQ(m_cfg->resolveVariables("2:$vr,$vr10"), "2:oid:0xa,oid:0xb");

    EXPECT_THROW(m_cfg->resolveVariables("$missing"), std::runtime_error);
    EXPECT_THROW(m_cfg->resolveVariables("$"), std::runtime_error);
}

TEST_F(SaiConfigTest, compositeKey)
{
    m_cfg->m_objectIdMap["v"] = m_vr;

    nlohmann::json data = {{"dest", "10.0.0.0/24"}, {"vr", "$v"}, {"SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION", "SAI_PACKET_ACTION_DROP"}};

    // keys come out sorted, switch_id injected, variable resolved
    EXPECT_EQ(m_cfg->buildCompositeKeyJson("route_entry", data, "create"), routeKey("10.0.0.0/24", m_vr));

    nlohmann::json noVr = {{"dest", "10.0.0.0/24"}};
    EXPECT_EQ(m_cfg->buildCompositeKeyJson("route_entry", noVr, "create"), "");

    EXPECT_EQ(m_cfg->buildCompositeKeyJson("port", data, "create"), "");

    EXPECT_TRUE(m_cfg->m_saiConfigManager->isCompositeKeyEntry("fdb_entry"));
    EXPECT_FALSE(m_cfg->m_saiConfigManager->isCompositeKeyEntry("port"));
}


TEST_F(SaiConfigTest, validAttributesPass)
{
    EXPECT_NO_THROW(validate("SAI_OBJECT_TYPE_PORT", m_port, {{"SAI_PORT_ATTR_MTU", "9100"}}, "set"));
    EXPECT_NO_THROW(validate("SAI_OBJECT_TYPE_PORT", m_port, {{"SAI_PORT_ATTR_MTU", ""}}, "get"));
    EXPECT_NO_THROW(validate("SAI_OBJECT_TYPE_PORT", m_port, {}, "remove"));

    // null OID is a legal value
    EXPECT_NO_THROW(validate("SAI_OBJECT_TYPE_PORT", m_port, {{"SAI_PORT_ATTR_INGRESS_ACL", "oid:0x0"}}, "set"));

    // empty list, and a list whose one member exists
    EXPECT_NO_THROW(validate("SAI_OBJECT_TYPE_PORT", m_port, {{"SAI_PORT_ATTR_EGRESS_MIRROR_SESSION", "0:null"}}, "set"));
    EXPECT_NO_THROW(validate("SAI_OBJECT_TYPE_PORT", m_port, {{"SAI_PORT_ATTR_EGRESS_MIRROR_SESSION", "1:" + m_mirror}}, "set"));

    // composite create with every OID present
    EXPECT_NO_THROW(validate("SAI_OBJECT_TYPE_ROUTE_ENTRY", routeKey("10.0.0.0/24", m_vr),
                             {{"SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION", "SAI_PACKET_ACTION_FORWARD"},
                              {"SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID", m_nh}}, "create"));
}

TEST_F(SaiConfigTest, attributeNameChecks)
{
    EXPECT_THROW(validate("SAI_OBJECT_TYPE_PORT", m_port, {{"SAI_PORT_ATTR_DOES_NOT_EXIST", "1"}}, "set"), std::invalid_argument);
    EXPECT_THROW(validate("SAI_OBJECT_TYPE_PORT", m_port, {{"SAI_VLAN_ATTR_VLAN_ID", "5"}}, "set"), std::invalid_argument);
    EXPECT_THROW(validate("SAI_OBJECT_TYPE_PORT", m_port, {{"SAI_VLAN_ATTR_VLAN_ID", ""}}, "get"), std::invalid_argument);
}

TEST_F(SaiConfigTest, targetMustExist)
{
    EXPECT_THROW(validate("SAI_OBJECT_TYPE_PORT", "oid:0x10000000ffff", {{"SAI_PORT_ATTR_MTU", "9100"}}, "set"), std::invalid_argument);
    EXPECT_THROW(validate("SAI_OBJECT_TYPE_VIRTUAL_ROUTER", "oid:0x300000000ffff", {}, "remove"), std::invalid_argument);
    EXPECT_THROW(validate("SAI_OBJECT_TYPE_VIRTUAL_ROUTER", "oid:0x300000000ffff", {}, "get"), std::invalid_argument);

    // right OID, wrong object type in the key
    EXPECT_THROW(validate("SAI_OBJECT_TYPE_PORT", m_vr, {{"SAI_PORT_ATTR_MTU", ""}}, "get"), std::invalid_argument);

    EXPECT_THROW(validate("SAI_OBJECT_TYPE_ROUTE_ENTRY", routeKey("10.9.9.0/24", m_vr), {}, "remove"), std::invalid_argument);
}

TEST_F(SaiConfigTest, entryCreateMustNotExist)
{
    auto drop = std::vector<std::pair<std::string, std::string>>{{"SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION", "SAI_PACKET_ACTION_DROP"}};

    EXPECT_NO_THROW(validate("SAI_OBJECT_TYPE_ROUTE_ENTRY", routeKey("10.8.8.0/24", m_vr), drop, "create"));

    m_db->hset("ASIC_STATE:SAI_OBJECT_TYPE_ROUTE_ENTRY:" + routeKey("10.8.8.0/24", m_vr), "SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION", "SAI_PACKET_ACTION_DROP");

    EXPECT_THROW(validate("SAI_OBJECT_TYPE_ROUTE_ENTRY", routeKey("10.8.8.0/24", m_vr), drop, "create"), std::invalid_argument);

    // an OID object create never looks for its own key
    EXPECT_NO_THROW(validate("SAI_OBJECT_TYPE_VIRTUAL_ROUTER", m_cfg->m_saiConfigManager->generateSaiObjectId("virtual_router"), {}, "create"));
}

TEST_F(SaiConfigTest, referencedOidsMustExist)
{
    // single OID attribute
    EXPECT_THROW(validate("SAI_OBJECT_TYPE_ROUTE_ENTRY", routeKey("10.0.0.0/24", m_vr),
                          {{"SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID", "oid:0x400000000ffff"}}, "create"), std::invalid_argument);

    // member of an object list
    EXPECT_THROW(validate("SAI_OBJECT_TYPE_PORT", m_port,
                          {{"SAI_PORT_ATTR_EGRESS_MIRROR_SESSION", "1:oid:0x1500000000ffff"}}, "set"), std::invalid_argument);

    // composite key field
    EXPECT_THROW(validate("SAI_OBJECT_TYPE_ROUTE_ENTRY", routeKey("10.0.0.0/24", "oid:0x300000000ffff"),
                          {{"SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION", "SAI_PACKET_ACTION_DROP"}}, "create"), std::invalid_argument);
}

TEST_F(SaiConfigTest, malformedKeysAndValues)
{
    auto drop = std::vector<std::pair<std::string, std::string>>{{"SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION", "SAI_PACKET_ACTION_DROP"}};

    EXPECT_THROW(validate("SAI_OBJECT_TYPE_ROUTE_ENTRY", routeKey("10.0.0/24", m_vr), drop, "create"), std::invalid_argument);
    EXPECT_THROW(validate("SAI_OBJECT_TYPE_ROUTE_ENTRY", routeKey("300.1.1.0/24", m_vr), drop, "create"), std::invalid_argument);
    EXPECT_THROW(validate("SAI_OBJECT_TYPE_ROUTE_ENTRY", routeKey("2001:db8::/129", m_vr), drop, "create"), std::invalid_argument);

    // enum typo, bad bool, list count mismatch, bad MAC, bare oid
    EXPECT_THROW(validate("SAI_OBJECT_TYPE_ROUTE_ENTRY", routeKey("10.0.0.0/24", m_vr),
                          {{"SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION", "SAI_PACKET_ACTION_DORP"}}, "create"), std::invalid_argument);
    EXPECT_THROW(validate("SAI_OBJECT_TYPE_PORT", m_port, {{"SAI_PORT_ATTR_ADMIN_STATE", "yes"}}, "set"), std::invalid_argument);
    EXPECT_THROW(validate("SAI_OBJECT_TYPE_ACL_TABLE", "oid:0x7000000000009",
                          {{"SAI_ACL_TABLE_ATTR_ACL_BIND_POINT_TYPE_LIST", "2:SAI_ACL_BIND_POINT_TYPE_PORT"}}, "create"), std::invalid_argument);
    EXPECT_THROW(validate("SAI_OBJECT_TYPE_NEIGHBOR_ENTRY",
                          "{\"ip\":\"10.0.0.1\",\"rif\":\"" + m_rif + "\",\"switch_id\":\"" + m_switch + "\"}",
                          {{"SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS", "00:AA:BB"}}, "create"), std::invalid_argument);
    EXPECT_THROW(validate("SAI_OBJECT_TYPE_ROUTE_ENTRY", routeKey("10.0.0.0/24", m_vr),
                          {{"SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID", "0x4000000000004"}}, "create"), std::invalid_argument);

    // values are not parsed for get; names still are
    EXPECT_NO_THROW(validate("SAI_OBJECT_TYPE_PORT", m_port, {{"SAI_PORT_ATTR_ADMIN_STATE", ""}}, "get"));
}

TEST_F(SaiConfigTest, syncdReadiness)
{
    EXPECT_NO_THROW(m_cfg->requireSyncdReady());

    m_db->hdel("VIDTORID", m_switch);

    EXPECT_THROW(m_cfg->requireSyncdReady(), std::runtime_error);

    // a whole file is refused, nothing is queued
    EXPECT_FALSE(runJson("[ { \"virtual_router\": { }, \"OP\": \"create\" } ]"));
    EXPECT_EQ(queueLength(), 0);
}


TEST_F(SaiConfigTest, fileShapeErrors)
{
    EXPECT_FALSE(runJson("{ \"virtual_router\": { }, \"OP\": \"create\" }"));           // not an array
    EXPECT_FALSE(runJson("[ { \"virtual_router\": { } } ]"));                           // no OP
    EXPECT_FALSE(runJson("[ { \"OP\": \"frobnicate\", \"virtual_router\": { } } ]"));  // unknown OP
    EXPECT_FALSE(runJson("[ { \"virtual_router\": { }, \"port\": { }, \"OP\": \"create\" } ]")); // two objects
    EXPECT_FALSE(runJson("[ { \"virtual_router\": { }, \"OP\": \"create\", \"OID_VALUE\": \"oid:0x1\" } ]"));
    EXPECT_FALSE(runJson("[ { \"not_a_sai_object\": { }, \"OP\": \"create\" } ]"));
    EXPECT_FALSE(runJson("[ { \"port\": { \"SAI_PORT_ATTR_MTU\": \"1\" }, \"OP\": \"set\" } ]")); // set without OID_VALUE
    EXPECT_FALSE(m_cfg->processJsonFile("/tmp/does-not-exist.json"));

    EXPECT_TRUE(runJson("[]"));
    EXPECT_EQ(queueLength(), 0);
}

TEST_F(SaiConfigTest, oidNameRules)
{
    // only on create
    EXPECT_FALSE(runJson("[ { \"port\": { \"SAI_PORT_ATTR_MTU\": \"\" }, \"OP\": \"get\", \"OID_VALUE\": \"" + m_port + "\", \"OID_NAME\": \"p\" } ]"));

    // not on entry types
    EXPECT_FALSE(runJson("[ { \"route_entry\": { \"dest\": \"10.0.0.0/24\", \"vr\": \"" + m_vr + "\" }, \"OP\": \"create\", \"OID_NAME\": \"r\" } ]"));

    // forward reference
    EXPECT_FALSE(runJson("[ { \"OP\": \"get\", \"OID_VALUE\": \"$later\" }, { \"virtual_router\": { }, \"OP\": \"create\", \"OID_NAME\": \"later\" } ]"));

    EXPECT_EQ(queueLength(), 0);

    // a live name cannot be rebound, and the first create is the only thing queued
    EXPECT_FALSE(runJson("[ { \"virtual_router\": { }, \"OP\": \"create\", \"OID_NAME\": \"x\" },"
                         "  { \"virtual_router\": { }, \"OP\": \"create\", \"OID_NAME\": \"x\" } ]"));
    EXPECT_EQ(queueLength(), 3); // key, value, op of the first create
    EXPECT_EQ(m_cfg->m_objectIdMap.count("x"), 1u);
}

TEST_F(SaiConfigTest, nameReleasedOnRemove)
{
    EXPECT_TRUE(runJson("[ { \"virtual_router\": { \"SAI_VIRTUAL_ROUTER_ATTR_ADMIN_V4_STATE\": \"true\" }, \"OP\": \"create\", \"OID_NAME\": \"v\" } ]"));

    auto oid = m_cfg->m_objectIdMap.at("v");

    // remove needs the object in ASIC_DB; stand in for syncd
    seed("SAI_OBJECT_TYPE_VIRTUAL_ROUTER", oid, "SAI_VIRTUAL_ROUTER_ATTR_ADMIN_V4_STATE", "true");

    EXPECT_TRUE(runJson("[ { \"OP\": \"remove\", \"OID_VALUE\": \"$v\" } ]"));
    EXPECT_EQ(m_cfg->m_objectIdMap.count("v"), 0u);

    // and the name is free again
    EXPECT_TRUE(runJson("[ { \"virtual_router\": { }, \"OP\": \"create\", \"OID_NAME\": \"v\" } ]"));
    EXPECT_NE(m_cfg->m_objectIdMap.at("v"), oid);
}

TEST_F(SaiConfigTest, variablesSpanFilesInOneProcess)
{
    EXPECT_TRUE(runJson("[ { \"virtual_router\": { }, \"OP\": \"create\", \"OID_NAME\": \"shared\" } ]"));

    auto oid = m_cfg->m_objectIdMap.at("shared");
    seed("SAI_OBJECT_TYPE_VIRTUAL_ROUTER", oid, "SAI_VIRTUAL_ROUTER_ATTR_ADMIN_V4_STATE", "true");

    EXPECT_TRUE(runJson("[ { \"OP\": \"get\", \"OID_VALUE\": \"$shared\" } ]"));
}

TEST_F(SaiConfigTest, redisQueueFormat)
{
    EXPECT_TRUE(runJson("[ { \"route_entry\": { \"vr\": \"" + m_vr + "\", \"dest\": \"10.1.0.0/16\","
                        "  \"SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION\": \"SAI_PACKET_ACTION_DROP\" }, \"OP\": \"create\" } ]"));

    swss::RedisCommand cmd;
    cmd.format("LRANGE ASIC_STATE_KEY_VALUE_OP_QUEUE 0 -1");
    swss::RedisReply reply(m_db.get(), cmd, REDIS_REPLY_ARRAY);

    ASSERT_EQ(reply.getContext()->elements, 3u);

    // one LPUSH of key, value, op leaves them head-first: op, value, key.
    // Key fields come out sorted regardless of input order, switch_id injected.
    EXPECT_EQ(std::string(reply.getContext()->element[0]->str), "Screate");
    EXPECT_EQ(std::string(reply.getContext()->element[1]->str), "[\"SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION\",\"SAI_PACKET_ACTION_DROP\"]");
    EXPECT_EQ(std::string(reply.getContext()->element[2]->str), "SAI_OBJECT_TYPE_ROUTE_ENTRY:" + routeKey("10.1.0.0/16", m_vr));
}

TEST_F(SaiConfigTest, transportDecision)
{
    using sairedis::CONTEXT_CONFIG_ZMQ_EMPTY;
    using sairedis::CONTEXT_CONFIG_ZMQ_ENABLED;
    using sairedis::CONTEXT_CONFIG_ZMQ_DISABLED;

    // -z always wins
    EXPECT_TRUE(SaiConfig::transportIsZmq("zmq_sync", CONTEXT_CONFIG_ZMQ_DISABLED, false));
    EXPECT_FALSE(SaiConfig::transportIsZmq("redis_sync", CONTEXT_CONFIG_ZMQ_ENABLED, true));

    // then the context config, when it says anything
    EXPECT_TRUE(SaiConfig::transportIsZmq("", CONTEXT_CONFIG_ZMQ_ENABLED, false));
    EXPECT_FALSE(SaiConfig::transportIsZmq("", CONTEXT_CONFIG_ZMQ_DISABLED, true));

    // then CONFIG_DB, which is what puts -z zmq_sync on syncd's own command line
    EXPECT_TRUE(SaiConfig::transportIsZmq("", CONTEXT_CONFIG_ZMQ_EMPTY, true));
    EXPECT_FALSE(SaiConfig::transportIsZmq("", CONTEXT_CONFIG_ZMQ_EMPTY, false));
}

TEST_F(SaiConfigTest, configDbSwitches)
{
    clearConfigDb();
    EXPECT_FALSE(m_cfg->configDbWantsZmq());

    setConfigDb("SYSTEM_DEFAULTS", "swss_zmq", "status", "disabled");
    EXPECT_FALSE(m_cfg->configDbWantsZmq());
    setConfigDb("SYSTEM_DEFAULTS", "swss_zmq", "status", "enabled");
    EXPECT_TRUE(m_cfg->configDbWantsZmq());

    // a DPU is always on ZMQ, whatever the other two say
    clearConfigDb();
    setConfigDb("DEVICE_METADATA", "localhost", "switch_type", "switch");
    EXPECT_FALSE(m_cfg->configDbWantsZmq());
    setConfigDb("DEVICE_METADATA", "localhost", "switch_type", "dpu");
    setConfigDb("SYSTEM_DEFAULTS", "swss_zmq", "status", "disabled");
    EXPECT_TRUE(m_cfg->configDbWantsZmq());
}

TEST_F(SaiConfigTest, transportFromContextFile)
{
    // a context file with endpoints only, as shipped on hardware: no zmq_enable
    std::string path = "/tmp/saiconfig-test-ctx-" + std::to_string(getpid()) + ".json";
    std::ofstream f(path);
    f << "{\"CONTEXTS\":[{\"guid\":0,\"name\":\"syncd\",\"dbAsic\":\"ASIC_DB\",\"dbCounters\":\"COUNTERS_DB\","
         "\"dbFlex\":\"FLEX_COUNTER_DB\",\"dbState\":\"STATE_DB\",\"zmq_endpoint\":\"ipc:///tmp/saiconfig-test-endpoint\","
         "\"zmq_ntf_endpoint\":\"ipc:///tmp/saiconfig-test-notify\",\"switches\":[{\"index\":0,\"hwinfo\":\"\"}]}]}";
    f.close();

    m_cfg->m_contextConfigFile = path;

    clearConfigDb();
    EXPECT_FALSE(m_cfg->initTransport());   // nothing says zmq
    EXPECT_EQ(m_cfg->m_channel, nullptr);

    setConfigDb("SYSTEM_DEFAULTS", "swss_zmq", "status", "enabled");
    EXPECT_TRUE(m_cfg->initTransport());    // CONFIG_DB does, endpoint comes from the file
    EXPECT_NE(m_cfg->m_channel, nullptr);

    m_cfg->m_forcedMode = "redis_sync";
    EXPECT_FALSE(m_cfg->initTransport());   // operator override
    EXPECT_EQ(m_cfg->m_channel, nullptr);

    // no file at all: default context, decision still from CONFIG_DB
    m_cfg->m_forcedMode = "";
    m_cfg->m_contextConfigFile = "/tmp/does-not-exist-" + std::to_string(getpid()) + ".json";
    EXPECT_TRUE(m_cfg->initTransport());

    remove(path.c_str());
}

TEST_F(SaiConfigTest, commandLineModeAndContext)
{
    const char *bad[] = {"saiconfig", "-z", "bogus"};
    EXPECT_THROW(m_cfg->handleCmdLine(3, const_cast<char **>(bad)), std::invalid_argument);

    const char *dangling[] = {"saiconfig", "-x"};
    EXPECT_THROW(m_cfg->handleCmdLine(2, const_cast<char **>(dangling)), std::invalid_argument);

    const char *ok[] = {"saiconfig", "-z", "redis_sync", "-x", "/tmp/none.json"};
    EXPECT_NO_THROW(m_cfg->handleCmdLine(5, const_cast<char **>(ok)));
    EXPECT_EQ(m_cfg->m_forcedMode, "redis_sync");
    EXPECT_EQ(m_cfg->m_contextConfigFile, "/tmp/none.json");

    const char *longForm[] = {"saiconfig", "--mode", "zmq_sync", "--context-config", "/tmp/other.json"};
    EXPECT_NO_THROW(m_cfg->handleCmdLine(5, const_cast<char **>(longForm)));
    EXPECT_EQ(m_cfg->m_forcedMode, "zmq_sync");
    EXPECT_EQ(m_cfg->m_contextConfigFile, "/tmp/other.json");
}
