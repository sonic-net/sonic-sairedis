#ifndef __SYNCD_H__
#define __SYNCD_H__

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <thread>
#include <set>

#include <unistd.h>
#include <execinfo.h>
#include <signal.h>
#include <getopt.h>

#ifdef SAITHRIFT
#include <utility>
#include <algorithm>
#include <switch_sai_rpc_server.h>
#endif // SAITHRIFT

#include "string.h"
extern "C" {
#include "sai.h"
}

#include "meta/sai_serialize.h"
#include "meta/saiattributelist.h"
#include "swss/redisclient.h"
#include "swss/dbconnector.h"
#include "swss/producertable.h"
#include "swss/consumertable.h"
#include "swss/consumerstatetable.h"
#include "swss/notificationconsumer.h"
#include "swss/notificationproducer.h"
#include "swss/selectableevent.h"
#include "swss/select.h"
#include "swss/logger.h"
#include "swss/table.h"

#include "syncd_saiswitch.h"

#define UNREFERENCED_PARAMETER(X)

#define DEFAULT_VLAN_NUMBER         1

#define VIDTORID                    "VIDTORID"
#define RIDTOVID                    "RIDTOVID"
#define VIDCOUNTER                  "VIDCOUNTER"
#define LANES                       "LANES"
#define HIDDEN                      "HIDDEN"

#define SAI_COLD_BOOT               0
#define SAI_WARM_BOOT               1
#define SAI_FAST_BOOT               2

#ifdef SAITHRIFT
#define SWITCH_SAI_THRIFT_RPC_SERVER_PORT 9092
#endif // SAITHRIFT

extern std::mutex g_mutex;

extern std::map<sai_object_id_t, std::shared_ptr<SaiSwitch>> switches;

void startDiagShell();

void hardReinit();

void performWarmRestart();

sai_object_id_t translate_vid_to_rid(_In_ sai_object_id_t vid);

void redisClearVidToRidMap();
void redisClearRidToVidMap();

sai_object_type_t getObjectTypeFromVid(
        _In_ sai_object_id_t sai_object_id);

extern std::shared_ptr<swss::NotificationProducer>  notifications;
extern std::shared_ptr<swss::RedisClient>   g_redisClient;

sai_object_id_t redis_create_virtual_object_id(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type);

sai_object_type_t redis_sai_object_type_query(
        _In_ sai_object_id_t object_id);

sai_object_id_t redis_sai_switch_id_query(
        _In_ sai_object_id_t oid);

sai_object_id_t translate_rid_to_vid(
        _In_ sai_object_id_t rid,
        _In_ sai_object_id_t switch_vid);

void translate_rid_to_vid_list(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t switch_vid,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list);

sai_status_t syncdApplyView();
void check_notifications_pointers(
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list);

bool is_set_attribute_workaround(
        _In_ sai_object_type_t objecttype,
        _In_ sai_attr_id_t attrid,
        _In_ sai_status_t status);

void startNotificationsProcessingThread();
void stopNotificationsProcessingThread();

sai_status_t processBulkEvent(
        _In_ sai_common_api_t api,
        _In_ const swss::KeyOpFieldsValuesTuple &kco);

void set_sai_api_log_min_prio(
        _In_ const std::string &prio);

#endif // __SYNCD_H__
