/**
 * @file TestGearboxMdioAccess.cpp
 *
 * Unit tests for GearboxMdioAccess, which rewrites the MDIO register access
 * pointers of a gearbox PHY's switch-create attribute list to symbols it
 * dlopen()s from the library the PHY is configured with.
 *
 * The APPL_DB-backed tests require a running redis; they are skipped when
 * redis is not reachable, mirroring how the other syncd unit tests depend on
 * it.
 */

#include "GearboxMdioAccess.h"
#include "swss/dbconnector.h"
#include "swss/logger.h"
#include "swss/table.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace syncd;

#ifndef MDIO_TEST_LIB_PATH
#define MDIO_TEST_LIB_PATH "libMdioTest.so"
#endif

#ifndef MDIO_TEST_LIB_MISSING_SYMBOLS_PATH
#define MDIO_TEST_LIB_MISSING_SYMBOLS_PATH "libMdioTestMissingSymbols.so"
#endif

#ifndef MDIO_TEST_LIB_MARKER
#define MDIO_TEST_LIB_MARKER "mdiotestlib.loaded"
#endif

#define GEARBOX_TABLE "_GEARBOX_TABLE"

static size_t markerCount()
{
    SWSS_LOG_ENTER();

    std::ifstream f(MDIO_TEST_LIB_MARKER);

    size_t count = 0;
    std::string line;

    while (std::getline(f, line))
    {
        count++;
    }

    return count;
}

class GearboxMdioAccessTest : public ::testing::Test
{
    protected:

        void SetUp() override
        {
            std::remove(MDIO_TEST_LIB_MARKER);

            try
            {
                m_db = std::make_shared<swss::DBConnector>("APPL_DB", 0);
            }
            catch (const std::exception&)
            {
                GTEST_SKIP() << "redis is not running";
            }

            m_table = std::make_shared<swss::Table>(m_db.get(), GEARBOX_TABLE);

            clearGearboxTable();
        }

        void TearDown() override
        {
            if (m_table)
            {
                clearGearboxTable();
            }

            std::remove(MDIO_TEST_LIB_MARKER);
        }

        void clearGearboxTable()
        {
            SWSS_LOG_ENTER();

            std::vector<std::string> keys;

            m_table->getKeys(keys);

            for (auto& key : keys)
            {
                m_table->del(key);
            }
        }

        void publishPhy(
                _In_ const std::string& key,
                _In_ const std::string& hwinfo,
                _In_ const std::string& library)
        {
            SWSS_LOG_ENTER();

            std::vector<swss::FieldValueTuple> values;

            if (!hwinfo.empty())
            {
                values.emplace_back("hwinfo", hwinfo);
            }

            if (!library.empty())
            {
                values.emplace_back("phy_access_lib_name", library);
            }

            m_table->set(key, values);
        }

        /**
         * @brief Attribute list builder keeping the s8list buffer alive.
         */
        struct AttrList
        {
            std::vector<sai_attribute_t> attrs;

            std::vector<std::vector<int8_t>> buffers;

            void addHardwareInfo(_In_ const std::string& hwinfo)
            {
                SWSS_LOG_ENTER();

                sai_attribute_t attr = {};

                attr.id = SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO;

                buffers.emplace_back(hwinfo.begin(), hwinfo.end());
                attr.value.s8list.list = buffers.back().data();
                attr.value.s8list.count = static_cast<uint32_t>(hwinfo.size());

                attrs.push_back(attr);
            }

            void addMdioPointers()
            {
                SWSS_LOG_ENTER();

                sai_attribute_t read = {};
                read.id = SAI_SWITCH_ATTR_REGISTER_READ;
                read.value.ptr = (void*)0xdeadbeef;

                sai_attribute_t write = {};
                write.id = SAI_SWITCH_ATTR_REGISTER_WRITE;
                write.value.ptr = (void*)0xcafebabe;

                attrs.push_back(read);
                attrs.push_back(write);
            }

            sai_attribute_t* find(_In_ sai_attr_id_t id)
            {
                SWSS_LOG_ENTER();

                for (auto& attr : attrs)
                {
                    if (attr.id == id)
                    {
                        return &attr;
                    }
                }

                return NULL;
            }
        };

    protected:

        std::shared_ptr<swss::DBConnector> m_db;

        std::shared_ptr<swss::Table> m_table;
};

TEST_F(GearboxMdioAccessTest, noMdioAttributesLeavesListAlone)
{
    AttrList attrs;

    attrs.addHardwareInfo("UNITTEST-PHY-0");

    GearboxMdioAccess access;

    /* Early return: nothing to rewrite, must not touch the database. */
    access.updateMdioPointers(
            static_cast<uint32_t>(attrs.attrs.size()),
            attrs.attrs.data());

    EXPECT_EQ(attrs.attrs.size(), 1U);
    EXPECT_EQ(attrs.find(SAI_SWITCH_ATTR_REGISTER_READ), nullptr);
}

TEST_F(GearboxMdioAccessTest, missingHardwareInfoClearsPointers)
{
    AttrList attrs;

    attrs.addMdioPointers();

    GearboxMdioAccess access;

    access.updateMdioPointers(
            static_cast<uint32_t>(attrs.attrs.size()),
            attrs.attrs.data());

    EXPECT_EQ(attrs.find(SAI_SWITCH_ATTR_REGISTER_READ)->value.ptr, nullptr);
    EXPECT_EQ(attrs.find(SAI_SWITCH_ATTR_REGISTER_WRITE)->value.ptr, nullptr);
}

TEST_F(GearboxMdioAccessTest, unknownHwinfoClearsPointers)
{
    AttrList attrs;

    attrs.addHardwareInfo("UNKNOWN-PHY");
    attrs.addMdioPointers();

    GearboxMdioAccess access;

    access.updateMdioPointers(
            static_cast<uint32_t>(attrs.attrs.size()),
            attrs.attrs.data());

    EXPECT_EQ(attrs.find(SAI_SWITCH_ATTR_REGISTER_READ)->value.ptr, nullptr);
    EXPECT_EQ(attrs.find(SAI_SWITCH_ATTR_REGISTER_WRITE)->value.ptr, nullptr);
}

TEST_F(GearboxMdioAccessTest, nullHardwareInfoListClearsPointers)
{
    AttrList attrs;

    sai_attribute_t attr = {};
    attr.id = SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO;
    attr.value.s8list.list = NULL;
    attrs.attrs.push_back(attr);

    attrs.addMdioPointers();

    GearboxMdioAccess access;

    access.updateMdioPointers(
            static_cast<uint32_t>(attrs.attrs.size()),
            attrs.attrs.data());

    EXPECT_EQ(attrs.find(SAI_SWITCH_ATTR_REGISTER_READ)->value.ptr, nullptr);
    EXPECT_EQ(attrs.find(SAI_SWITCH_ATTR_REGISTER_WRITE)->value.ptr, nullptr);
}

TEST_F(GearboxMdioAccessTest, nonPhyEntriesAreIgnored)
{
    /* "phy:<id>:ports:<n>" and friends must not be treated as PHY entries. */
    publishPhy("phy:0:ports:1", "UNITTEST-PHY-0", MDIO_TEST_LIB_PATH);
    publishPhy("gearbox:0", "UNITTEST-PHY-0", MDIO_TEST_LIB_PATH);

    AttrList attrs;

    attrs.addHardwareInfo("UNITTEST-PHY-0");
    attrs.addMdioPointers();

    GearboxMdioAccess access;

    access.updateMdioPointers(
            static_cast<uint32_t>(attrs.attrs.size()),
            attrs.attrs.data());

    EXPECT_EQ(attrs.find(SAI_SWITCH_ATTR_REGISTER_READ)->value.ptr, nullptr);
    EXPECT_EQ(attrs.find(SAI_SWITCH_ATTR_REGISTER_WRITE)->value.ptr, nullptr);
}

TEST_F(GearboxMdioAccessTest, entryMissingFieldsIsIgnored)
{
    /* No phy_access_lib_name: the PHY is skipped while scanning the table. */
    publishPhy("phy:0", "UNITTEST-PHY-0", "");

    AttrList attrs;

    attrs.addHardwareInfo("UNITTEST-PHY-0");
    attrs.addMdioPointers();

    GearboxMdioAccess access;

    access.updateMdioPointers(
            static_cast<uint32_t>(attrs.attrs.size()),
            attrs.attrs.data());

    EXPECT_EQ(attrs.find(SAI_SWITCH_ATTR_REGISTER_READ)->value.ptr, nullptr);
    EXPECT_EQ(attrs.find(SAI_SWITCH_ATTR_REGISTER_WRITE)->value.ptr, nullptr);
}

TEST_F(GearboxMdioAccessTest, libraryLoadFailureClearsPointers)
{
    publishPhy("phy:0", "UNITTEST-PHY-0", "/nonexistent/libmdio.so");

    AttrList attrs;

    attrs.addHardwareInfo("UNITTEST-PHY-0");
    attrs.addMdioPointers();

    GearboxMdioAccess access;

    access.updateMdioPointers(
            static_cast<uint32_t>(attrs.attrs.size()),
            attrs.attrs.data());

    EXPECT_EQ(attrs.find(SAI_SWITCH_ATTR_REGISTER_READ)->value.ptr, nullptr);
    EXPECT_EQ(attrs.find(SAI_SWITCH_ATTR_REGISTER_WRITE)->value.ptr, nullptr);
}

TEST_F(GearboxMdioAccessTest, libraryMissingSymbolsClearsPointers)
{
    publishPhy("phy:0", "UNITTEST-PHY-0", MDIO_TEST_LIB_MISSING_SYMBOLS_PATH);

    AttrList attrs;

    attrs.addHardwareInfo("UNITTEST-PHY-0");
    attrs.addMdioPointers();

    GearboxMdioAccess access;

    access.updateMdioPointers(
            static_cast<uint32_t>(attrs.attrs.size()),
            attrs.attrs.data());

    EXPECT_EQ(attrs.find(SAI_SWITCH_ATTR_REGISTER_READ)->value.ptr, nullptr);
    EXPECT_EQ(attrs.find(SAI_SWITCH_ATTR_REGISTER_WRITE)->value.ptr, nullptr);
}

TEST_F(GearboxMdioAccessTest, resolvesSymbolsAndPointersAreCallable)
{
    publishPhy("phy:0", "UNITTEST-PHY-0", MDIO_TEST_LIB_PATH);

    AttrList attrs;

    attrs.addHardwareInfo("UNITTEST-PHY-0");
    attrs.addMdioPointers();

    GearboxMdioAccess access;

    access.updateMdioPointers(
            static_cast<uint32_t>(attrs.attrs.size()),
            attrs.attrs.data());

    auto read = reinterpret_cast<sai_switch_register_read_fn>(
            attrs.find(SAI_SWITCH_ATTR_REGISTER_READ)->value.ptr);
    auto write = reinterpret_cast<sai_switch_register_write_fn>(
            attrs.find(SAI_SWITCH_ATTR_REGISTER_WRITE)->value.ptr);

    ASSERT_NE(read, nullptr);
    ASSERT_NE(write, nullptr);

    uint32_t reg_val = 0;

    EXPECT_EQ(read(0, 0x1, 0x0, 1, &reg_val), SAI_STATUS_SUCCESS);
    EXPECT_EQ(reg_val, 0xA5A5A5A5U);

    EXPECT_EQ(write(0, 0x1, 0x0, 1, &reg_val), SAI_STATUS_SUCCESS);

    EXPECT_EQ(markerCount(), 1U);
}

TEST_F(GearboxMdioAccessTest, symbolsAreCachedPerLibrary)
{
    publishPhy("phy:0", "UNITTEST-PHY-0", MDIO_TEST_LIB_PATH);
    publishPhy("phy:1", "UNITTEST-PHY-1", MDIO_TEST_LIB_PATH);

    GearboxMdioAccess access;

    AttrList attrs0;

    attrs0.addHardwareInfo("UNITTEST-PHY-0");
    attrs0.addMdioPointers();

    access.updateMdioPointers(
            static_cast<uint32_t>(attrs0.attrs.size()),
            attrs0.attrs.data());

    AttrList attrs1;

    attrs1.addHardwareInfo("UNITTEST-PHY-1");
    attrs1.addMdioPointers();

    access.updateMdioPointers(
            static_cast<uint32_t>(attrs1.attrs.size()),
            attrs1.attrs.data());

    /* Both PHYs share the cached symbols; the library was loaded once. */
    EXPECT_EQ(
            attrs0.find(SAI_SWITCH_ATTR_REGISTER_READ)->value.ptr,
            attrs1.find(SAI_SWITCH_ATTR_REGISTER_READ)->value.ptr);
    EXPECT_EQ(
            attrs0.find(SAI_SWITCH_ATTR_REGISTER_WRITE)->value.ptr,
            attrs1.find(SAI_SWITCH_ATTR_REGISTER_WRITE)->value.ptr);
    EXPECT_EQ(markerCount(), 1U);
}
