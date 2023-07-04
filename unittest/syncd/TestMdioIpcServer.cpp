#include "MdioIpcServer.h"
#include "MdioIpcClient.h"
#include "MdioIpcCommon.h"
#include "MockableSaiInterface.h"
#include "swss/logger.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <gtest/gtest.h>

#define MDIO_MISSING_DEV_ADDR 0x1F
#define MDIO_MISSING_REG_ADDR 0xFFFF

using namespace syncd;
using namespace std;

static std::map<uint64_t, uint32_t> mdioDevRegValMap;
static std::map<uint64_t, uint32_t> mdioDevCl22RegValMap;


static sai_status_t MockMdioRead(
    _In_ sai_object_id_t switchId,
    _In_ uint32_t device_addr,
    _In_ uint32_t start_reg_addr,
    _In_ uint32_t number_of_registers,
    _Out_ uint32_t *reg_val)
{
    SWSS_LOG_ENTER();
    uint64_t key = device_addr;
    key <<= 32;
    key |= start_reg_addr;
    auto it = mdioDevRegValMap.find(key);
    if (it == mdioDevRegValMap.end())
    {
        *reg_val = 0;
        return SAI_STATUS_FAILURE;
    }
    else
    {
        *reg_val = it->second;
    }
    return SAI_STATUS_SUCCESS;
}

static sai_status_t MockMdioWrite(
    _In_ sai_object_id_t switchId,
    _In_ uint32_t device_addr,
    _In_ uint32_t start_reg_addr,
    _In_ uint32_t number_of_registers,
    _In_ const uint32_t *reg_val)
{
    SWSS_LOG_ENTER();
    if (MDIO_MISSING_DEV_ADDR == device_addr)
    {
        return SAI_STATUS_FAILURE;
    }
    uint64_t key = device_addr;
    key <<= 32;
    key |= start_reg_addr;
    mdioDevRegValMap[key] = *reg_val;
    return SAI_STATUS_SUCCESS;
}

static sai_status_t MockMdioCl22Read(
    _In_ sai_object_id_t switchId,
    _In_ uint32_t device_addr,
    _In_ uint32_t start_reg_addr,
    _In_ uint32_t number_of_registers,
    _Out_ uint32_t *reg_val)
{
    SWSS_LOG_ENTER();
    uint64_t key = device_addr;
    key <<= 32;
    key |= start_reg_addr;
    auto it = mdioDevCl22RegValMap.find(key);
    if (it == mdioDevCl22RegValMap.end())
    {
        *reg_val = 0;
        return SAI_STATUS_FAILURE;
    }
    else
    {
        *reg_val = it->second;
    }
    return SAI_STATUS_SUCCESS;
}

static sai_status_t MockMdioCl22Write(
    _In_ sai_object_id_t switchId,
    _In_ uint32_t device_addr,
    _In_ uint32_t start_reg_addr,
    _In_ uint32_t number_of_registers,
    _In_ const uint32_t *reg_val)
{
    SWSS_LOG_ENTER();
    if (MDIO_MISSING_DEV_ADDR == device_addr)
    {
        return SAI_STATUS_FAILURE;
    }
    uint64_t key = device_addr;
    key <<= 32;
    key |= start_reg_addr;
    mdioDevCl22RegValMap[key] = *reg_val;
    return SAI_STATUS_SUCCESS;
}

TEST(MdioIpcServer, mdioAccess)
{
    SWSS_LOG_ENTER();
    std::shared_ptr<MockableSaiInterface> mdio_sai(new MockableSaiInterface());
    mdio_sai->mock_switchMdioRead = MockMdioRead;
    mdio_sai->mock_switchMdioWrite = MockMdioWrite;
    mdio_sai->mock_switchMdioCl22Read = MockMdioCl22Read;
    mdio_sai->mock_switchMdioCl22Write = MockMdioCl22Write;
    char path[64];
    strcpy(path, SYNCD_IPC_SOCK_SYNCD);
    if (open(path, O_DIRECTORY) < 0)
    {
        SWSS_LOG_NOTICE("Directory %s does not exist", SYNCD_IPC_SOCK_SYNCD);
        if (mkdir(SYNCD_IPC_SOCK_SYNCD, 0755) < 0)
        {
            SWSS_LOG_WARN("Can not create directory %s", SYNCD_IPC_SOCK_SYNCD);
        }
    }
    std::shared_ptr<MdioIpcServer> mdio_server(new MdioIpcServer(mdio_sai, 0));
    mdio_server->setIpcTestMode();
    mdio_server->setSwitchId(0x21000000000000);
    mdio_server->startMdioThread();
    sleep(1);

    uint32_t data;
    uint64_t key;
    sai_status_t rc;
    uint32_t data2[2];

    /* MDIO read */
    mdioDevRegValMap.clear();
    data = 0;
    key = 0x4;
    key <<= 32;
    key |= 0x1A;
    mdioDevRegValMap[key] = 0xC0DE;
    rc = mdio_read(0xF0F0F0F0F0F0F0F0, 0x4, 0x1A, 1, &data);
    SWSS_LOG_NOTICE("rc = %d, data = %x", rc, data);
    EXPECT_EQ(rc, SAI_STATUS_SUCCESS);
    EXPECT_EQ(data, 0xC0DE);

    rc = mdio_read(0xF0F0F0F0F0F0F0F0, MDIO_MISSING_DEV_ADDR, MDIO_MISSING_REG_ADDR, 1, &data);
    EXPECT_NE(rc, SAI_STATUS_SUCCESS);

    rc = mdio_read(0xF0F0F0F0F0F0F0F0, 0x4, 0x1A, 2, data2);
    EXPECT_NE(rc, SAI_STATUS_SUCCESS);

    /* MDIO write */
    mdioDevRegValMap.clear();
    data = 0xBEEF;
    rc = mdio_write(0xF0F0F0F0F0F0F0F0, 0x3, 0x1B, 1, &data);
    SWSS_LOG_NOTICE("rc = %d", rc);
    EXPECT_EQ(rc, SAI_STATUS_SUCCESS);
    key = 0x3;
    key <<= 32;
    key |= 0x1B;
    SWSS_LOG_NOTICE("reg = %x", mdioDevRegValMap[key]);
    EXPECT_EQ(mdioDevRegValMap[key], 0xBEEF);

    rc = mdio_write(0xF0F0F0F0F0F0F0F0, MDIO_MISSING_DEV_ADDR, MDIO_MISSING_REG_ADDR, 1, &data);
    EXPECT_NE(rc, SAI_STATUS_SUCCESS);

    rc = mdio_write(0xF0F0F0F0F0F0F0F0, 0x3, 0x1B, 2, data2);
    EXPECT_NE(rc, SAI_STATUS_SUCCESS);

    /* MDIO CL22 read */
    mdioDevCl22RegValMap.clear();
    data = 0x0;
    key = 0x2;
    key <<= 32;
    key |= 0x1C;
    mdioDevCl22RegValMap[key] = 0xFEED;
    rc = mdio_read_cl22(0xF0F0F0F0F0F0F0F0, 0x2, 0x1C, 1, &data);
    SWSS_LOG_NOTICE("rc = %d, data = %x", rc, data);
    EXPECT_EQ(rc, SAI_STATUS_SUCCESS);
    EXPECT_EQ(data, 0xFEED);

    rc = mdio_read_cl22(0xF0F0F0F0F0F0F0F0, MDIO_MISSING_DEV_ADDR, MDIO_MISSING_REG_ADDR, 1, &data);
    EXPECT_NE(rc, SAI_STATUS_SUCCESS);

    rc = mdio_read_cl22(0xF0F0F0F0F0F0F0F0, 0x2, 0x1C, 2, data2);
    EXPECT_NE(rc, SAI_STATUS_SUCCESS);

    /* MDIO CL22 write */
    mdioDevCl22RegValMap.clear();
    data = 0xCAFE;
    rc = mdio_write_cl22(0xF0F0F0F0F0F0F0F0, 0x1, 0x1D, 1, &data);
    SWSS_LOG_NOTICE("rc = %d", rc);
    EXPECT_EQ(rc, SAI_STATUS_SUCCESS);
    key = 0x1;
    key <<= 32;
    key |= 0x1D;
    SWSS_LOG_NOTICE("cl22 reg = %x", mdioDevCl22RegValMap[key]);
    EXPECT_EQ(mdioDevCl22RegValMap[key], 0xCAFE);

    rc = mdio_write_cl22(0xF0F0F0F0F0F0F0F0, MDIO_MISSING_DEV_ADDR, MDIO_MISSING_REG_ADDR, 1, &data);
    EXPECT_NE(rc, SAI_STATUS_SUCCESS);

    rc = mdio_write_cl22(0xF0F0F0F0F0F0F0F0, 0x1, 0x1D, 2, data2);
    EXPECT_NE(rc, SAI_STATUS_SUCCESS);

    /* Test ipc client timeout */
    sleep(MDIO_CLIENT_TIMEOUT+1);
    rc = mdio_read_cl22(0xF0F0F0F0F0F0F0F0, 0x1, 0x1D, 1, &data);
    EXPECT_EQ(rc, SAI_STATUS_SUCCESS);

    mdio_server->stopMdioThread();
    sleep(MDIO_CLIENT_TIMEOUT+1);
}

TEST(MdioIpcServer, mdioConnect)
{
    SWSS_LOG_ENTER();
    std::shared_ptr<MockableSaiInterface> mdio_sai(new MockableSaiInterface());
    mdio_sai->mock_switchMdioRead = MockMdioRead;
    mdio_sai->mock_switchMdioWrite = MockMdioWrite;
    mdio_sai->mock_switchMdioCl22Read = MockMdioCl22Read;
    mdio_sai->mock_switchMdioCl22Write = MockMdioCl22Write;
    char path[64];
    strcpy(path, SYNCD_IPC_SOCK_SYNCD);
    if (open(path, O_DIRECTORY) < 0)
    {
        SWSS_LOG_NOTICE("Directory %s does not exist", SYNCD_IPC_SOCK_SYNCD);
        if (mkdir(SYNCD_IPC_SOCK_SYNCD, 0755) < 0)
        {
            SWSS_LOG_WARN("Can not create directory %s", SYNCD_IPC_SOCK_SYNCD);
        }
    }
    std::shared_ptr<MdioIpcServer> mdio_server(new MdioIpcServer(mdio_sai, 0));
    mdio_server->setSwitchId(0x21000000000000);
    mdio_server->startMdioThread();
    sleep(1);

    uint32_t data;
    uint64_t key;
    sai_status_t rc;

    /* ipc connect fail without setIpcTestMode() on non broadcom platform */
    mdioDevRegValMap.clear();
    data = 0;
    key = 0x4;
    key <<= 32;
    key |= 0x1A;
    mdioDevRegValMap[key] = 0xC0DE;
    rc = mdio_read(0xF0F0F0F0F0F0F0F0, 0x4, 0x1A, 1, &data);
    EXPECT_NE(rc, SAI_STATUS_SUCCESS);

    mdio_server->stopMdioThread();
}
