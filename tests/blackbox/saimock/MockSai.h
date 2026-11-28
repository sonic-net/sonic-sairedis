#pragma once

extern "C" {
#include "sai.h"
#include "saiextensions.h"
}

#include "meta/SaiInterface.h"
#include "meta/MetaKeyHasher.h"
#include "meta/SaiAttrWrapper.h"

#include "swss/logger.h"

#include <atomic>
#include <cinttypes>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace saimock
{
    /**
     * @brief Kind of SAI call, the first half of a statistics key.
     */
    enum class Op : uint32_t
    {
        CREATE,
        REMOVE,
        SET,
        GET,
        BULK_CREATE,
        BULK_REMOVE,
        BULK_SET,
        BULK_GET,
        GET_STATS,
        GET_STATS_EXT,
        CLEAR_STATS,
        BULK_GET_STATS,
        BULK_CLEAR_STATS,
        OBJECT_TYPE_QUERY,
        SWITCH_ID_QUERY,
        QUERY,
        FLUSH_FDB,
        NOTIFY,
        ANY,
    };

    const char* opName(
            _In_ Op op);

    bool opFromName(
            _In_ const std::string& name,
            _Out_ Op& op);

    /**
     * @brief Counters of one (operation, object type) pair.
     *
     * Times are CLOCK_MONOTONIC nanoseconds, the clock every process on the
     * host shares, so they compare with the times other processes take.
     */
    struct ApiStat
    {
        uint64_t calls = 0;

        uint64_t objects = 0;

        uint64_t errors = 0;

        /** Calls answered not supported, as bulk on an SDK without it */
        uint64_t unsupported = 0;

        /** Time spent inside the mock, injected delay included */
        uint64_t sdkNs = 0;

        /** Injected delay alone */
        uint64_t delayNs = 0;

        /** Entry of the first call */
        uint64_t firstNs = 0;

        /** Return of the last call */
        uint64_t lastNs = 0;

        uint64_t maxObjects = 0;
    };

    /**
     * @brief Latency a call gets, to emulate the time a real SDK takes.
     */
    struct Delay
    {
        uint64_t perCallNs = 0;

        uint64_t perObjectNs = 0;

        /** Sleep instead of spinning, so the delay costs no CPU */
        bool sleep = false;
    };

    struct Config
    {
        std::string controlSocket = "/var/run/saimock.sock";

        uint32_t portCount = 32;

        uint32_t lanesPerPort = 4;

        uint32_t portSpeed = 100000;

        uint32_t queuesPerPort = 20;

        uint32_t cpuQueues = 32;

        uint32_t pgsPerPort = 8;

        bool defaultBridgePorts = true;

        bool defaultVlanMembers = true;

        bool portsUp = true;

        bool storeAttributes = true;

        bool bulkSupported = true;

        bool bulkStatsSupported = true;

        std::string srcMac = "00:11:22:33:44:55";

        std::string delays;
    };

    /**
     * @brief One attribute value the mock keeps.
     *
     * Values of primitive types are kept in place, others (lists) in a deep
     * copy.
     */
    struct StoredAttr
    {
        sai_attribute_t attr;

        std::shared_ptr<saimeta::SaiAttrWrapper> wrapper;

        const sai_attribute_t* get() const
        {
            SWSS_LOG_ENTER();

            return wrapper ? wrapper->getSaiAttr() : &attr;
        }
    };

    struct MockObject
    {
        sai_object_type_t objectType = SAI_OBJECT_TYPE_NULL;

        std::vector<StoredAttr> attrs;
    };

    /**
     * @brief The objects a switch has from its creation, and the
     * notification callbacks its user registered.
     */
    struct SwitchModel
    {
        uint32_t index = 0;

        sai_object_id_t switchRid = SAI_NULL_OBJECT_ID;

        sai_object_id_t cpuPort = SAI_NULL_OBJECT_ID;

        std::vector<sai_object_id_t> ports;

        std::vector<sai_object_id_t> bridgePorts;

        sai_object_id_t defaultVlan = SAI_NULL_OBJECT_ID;

        std::map<sai_object_id_t, std::vector<sai_object_id_t>> portChildren;

        std::map<sai_attr_id_t, sai_pointer_t> notifications;
    };

    /**
     * @brief Vendor independent mock of a SAI SDK, for benchmarking syncd.
     *
     * Every SAI API reaches this class through API tables that stub.pl
     * generates. It keeps the objects syncd creates, answers what syncd asks
     * of a switch (discovery, lanes, queues), and counts every call: how
     * many, how many objects, how long they took. A control socket serves
     * the counters, sets injected latency and triggers notifications.
     *
     * All SAI calls hold one mutex: syncd makes them one at a time anyway.
     */
    class MockSai:
        public sairedis::SaiInterface
    {
        public:

            MockSai();

            virtual ~MockSai();

        public:

            virtual sai_status_t apiInitialize(
                    _In_ uint64_t flags,
                    _In_ const sai_service_method_table_t *service_method_table) override;

            virtual sai_status_t apiUninitialize(void) override;

        public: // SAI interface overrides

            virtual sai_status_t create(
                    _In_ sai_object_type_t objectType,
                    _Out_ sai_object_id_t* objectId,
                    _In_ sai_object_id_t switchId,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list) override;

            virtual sai_status_t remove(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId) override;

            virtual sai_status_t set(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId,
                    _In_ const sai_attribute_t *attr) override;

            virtual sai_status_t get(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId,
                    _In_ uint32_t attr_count,
                    _Inout_ sai_attribute_t *attr_list) override;

        public: // QUAD ENTRY and BULK QUAD ENTRY

            SAIREDIS_DECLARE_EVERY_ENTRY(SAIREDIS_SAIINTERFACE_DECLARE_QUAD_ENTRY_OVERRIDE);
            SAIREDIS_DECLARE_EVERY_BULK_ENTRY(SAIREDIS_SAIINTERFACE_DECLARE_BULK_ENTRY_OVERRIDE);

        public: // bulk QUAD oid

            virtual sai_status_t bulkCreate(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t object_count,
                    _In_ const uint32_t *attr_count,
                    _In_ const sai_attribute_t **attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_object_id_t *object_id,
                    _Out_ sai_status_t *object_statuses) override;

            virtual sai_status_t bulkRemove(
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_id_t *object_id,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses) override;

            virtual sai_status_t bulkSet(
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_id_t *object_id,
                    _In_ const sai_attribute_t *attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses) override;

            virtual sai_status_t bulkGet(
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_id_t *object_id,
                    _In_ const uint32_t *attr_count,
                    _Inout_ sai_attribute_t **attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses) override;

        public: // stats API

            virtual sai_status_t getStats(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _Out_ uint64_t *counters) override;

            virtual sai_status_t queryStatsCapability(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_type_t object_type,
                    _Inout_ sai_stat_capability_list_t *stats_capability) override;

            virtual sai_status_t queryStatsStCapability(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_type_t object_type,
                    _Inout_ sai_stat_st_capability_list_t *stats_capability) override;

            virtual sai_status_t getStatsExt(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _In_ sai_stats_mode_t mode,
                    _Out_ uint64_t *counters) override;

            virtual sai_status_t clearStats(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids) override;

            virtual sai_status_t bulkGetStats(
                    _In_ sai_object_id_t switchId,
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_key_t *object_key,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _In_ sai_stats_mode_t mode,
                    _Inout_ sai_status_t *object_statuses,
                    _Out_ uint64_t *counters) override;

            virtual sai_status_t bulkClearStats(
                    _In_ sai_object_id_t switchId,
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_key_t *object_key,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _In_ sai_stats_mode_t mode,
                    _Inout_ sai_status_t *object_statuses) override;

        public: // non QUAD API

            virtual sai_status_t flushFdbEntries(
                    _In_ sai_object_id_t switchId,
                    _In_ uint32_t attrCount,
                    _In_ const sai_attribute_t *attrList) override;

        public: // SAI API

            virtual sai_status_t objectTypeGetAvailability(
                    _In_ sai_object_id_t switchId,
                    _In_ sai_object_type_t objectType,
                    _In_ uint32_t attrCount,
                    _In_ const sai_attribute_t *attrList,
                    _Out_ uint64_t *count) override;

            virtual sai_status_t queryAttributeCapability(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_type_t object_type,
                    _In_ sai_attr_id_t attr_id,
                    _Out_ sai_attr_capability_t *capability) override;

            virtual sai_status_t queryAttributeEnumValuesCapability(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_type_t object_type,
                    _In_ sai_attr_id_t attr_id,
                    _Inout_ sai_s32_list_t *enum_values_capability) override;

            virtual sai_object_type_t objectTypeQuery(
                    _In_ sai_object_id_t objectId) override;

            virtual sai_object_id_t switchIdQuery(
                    _In_ sai_object_id_t objectId) override;

            virtual sai_status_t logSet(
                    _In_ sai_api_t api,
                    _In_ sai_log_level_t log_level) override;

            virtual sai_status_t queryApiVersion(
                    _Out_ sai_api_version_t *version) override;

        public: // control

            /**
             * @brief Executes one control command, returns a JSON document.
             */
            std::string control(
                    _In_ const std::string& line);

        public: // entry helpers used by the generated entry methods

            sai_status_t entryCreate(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t entryRemove(
                    _In_ const sai_object_meta_key_t& metaKey);

            sai_status_t entrySet(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ const sai_attribute_t *attr);

            sai_status_t entryGet(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ uint32_t attr_count,
                    _Inout_ sai_attribute_t *attr_list);

            sai_status_t entryBulkCreate(
                    _In_ sai_object_type_t objectType,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_meta_key_t *metaKeys,
                    _In_ const uint32_t *attr_count,
                    _In_ const sai_attribute_t **attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses);

            sai_status_t entryBulkRemove(
                    _In_ sai_object_type_t objectType,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_meta_key_t *metaKeys,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses);

            sai_status_t entryBulkSet(
                    _In_ sai_object_type_t objectType,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_meta_key_t *metaKeys,
                    _In_ const sai_attribute_t *attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses);

            sai_status_t entryBulkGet(
                    _In_ sai_object_type_t objectType,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_meta_key_t *metaKeys,
                    _In_ const uint32_t *attr_count,
                    _Inout_ sai_attribute_t **attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses);

        private: // call accounting

            friend class CallScope;

            static uint64_t nowNs();

            uint64_t delayFor(
                    _In_ Op op,
                    _In_ sai_object_type_t objectType,
                    _In_ uint64_t objects,
                    _Out_ bool& sleep) const;

            void account(
                    _In_ Op op,
                    _In_ sai_object_type_t objectType,
                    _In_ uint64_t objects,
                    _In_ uint64_t errors,
                    _In_ uint64_t unsupported,
                    _In_ uint64_t startNs,
                    _In_ uint64_t delayNs,
                    _In_ uint64_t endNs);

        private: // objects

            static sai_object_id_t encodeOid(
                    _In_ sai_object_type_t objectType,
                    _In_ uint32_t switchIndex,
                    _In_ uint64_t index);

            static uint32_t switchIndexOf(
                    _In_ sai_object_id_t oid);

            sai_object_id_t allocateOid(
                    _In_ sai_object_type_t objectType,
                    _In_ uint32_t switchIndex);

            void storeAttr(
                    _Inout_ MockObject& object,
                    _In_ const sai_attribute_t& attr);

            void storeAttrs(
                    _Inout_ MockObject& object,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list,
                    _In_ bool force);

            sai_object_id_t addObject(
                    _In_ SwitchModel& model,
                    _In_ sai_object_type_t objectType,
                    _In_ const std::vector<sai_attribute_t>& attrs);

            void setOidList(
                    _In_ sai_object_id_t oid,
                    _In_ sai_attr_id_t attrId,
                    _In_ const std::vector<sai_object_id_t>& list);

            void setU32(
                    _In_ sai_object_id_t oid,
                    _In_ sai_attr_id_t attrId,
                    _In_ uint32_t value);

            void setOid(
                    _In_ sai_object_id_t oid,
                    _In_ sai_attr_id_t attrId,
                    _In_ sai_object_id_t value);

            sai_status_t getAttrs(
                    _In_ const MockObject& object,
                    _In_ sai_object_id_t oid,
                    _In_ uint32_t attr_count,
                    _Inout_ sai_attribute_t *attr_list);

            sai_status_t getAttr(
                    _In_ const MockObject& object,
                    _In_ sai_object_id_t oid,
                    _Inout_ sai_attribute_t& attr);

            bool dynamicAttr(
                    _In_ const MockObject& object,
                    _In_ sai_object_id_t oid,
                    _Inout_ sai_attribute_t& attr,
                    _Out_ sai_status_t& status);

            sai_status_t defaultAttr(
                    _In_ const MockObject& object,
                    _In_ sai_object_id_t oid,
                    _In_ const sai_attr_metadata_t& md,
                    _Inout_ sai_attribute_t& attr);

            SwitchModel* modelOf(
                    _In_ sai_object_id_t oid);

            void eraseOid(
                    _In_ sai_object_id_t oid);

        private: // single operations, called with the mutex held

            sai_status_t doCreate(
                    _In_ sai_object_type_t objectType,
                    _Out_ sai_object_id_t* objectId,
                    _In_ sai_object_id_t switchId,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t doRemove(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId);

            sai_status_t doSet(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId,
                    _In_ const sai_attribute_t *attr);

            sai_status_t doGet(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId,
                    _In_ uint32_t attr_count,
                    _Inout_ sai_attribute_t *attr_list);

            sai_status_t doCreateEntry(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t doRemoveEntry(
                    _In_ const sai_object_meta_key_t& metaKey);

            sai_status_t doSetEntry(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ const sai_attribute_t *attr);

            sai_status_t doGetEntry(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ uint32_t attr_count,
                    _Inout_ sai_attribute_t *attr_list);

            uint32_t statModes() const;

            void fillCounters(
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _Out_ uint64_t *counters);

        private: // switch model, MockSwitch.cpp

            sai_status_t createSwitch(
                    _Out_ sai_object_id_t* switchId,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            void createPortChildren(
                    _In_ SwitchModel& model,
                    _In_ sai_object_id_t portRid,
                    _In_ bool cpu);

            void removeSwitch(
                    _In_ uint32_t index);

        private: // configuration and control, MockControl.cpp

            void loadConfig(
                    _In_ const sai_service_method_table_t *smt);

            std::string setDelay(
                    _In_ const std::vector<std::string>& args);

            std::string statsJson(
                    _In_ bool reset);

            std::string objectsJson();

            std::string configJson();

            std::string notify(
                    _In_ const std::vector<std::string>& args);

            void fdbNotifier(
                    _In_ uint64_t count,
                    _In_ uint32_t batch,
                    _In_ uint64_t rate,
                    _In_ sai_fdb_event_t event,
                    _In_ uint64_t start);

            void portNotifier(
                    _In_ uint64_t count,
                    _In_ uint32_t batch,
                    _In_ uint64_t rate);

            void startControlServer();

            void stopControlServer();

            void controlServerThread();

            void serveClient(
                    _In_ int fd);

            void joinNotifiers();

        private:

            std::mutex m_mutex;

            bool m_apiInitialized;

            Config m_config;

            uint64_t m_nextIndex;

            uint64_t m_statsTick;

            std::unordered_map<sai_object_id_t, MockObject> m_oids;

            std::unordered_map<sai_object_meta_key_t, MockObject, saimeta::MetaKeyHasher, saimeta::MetaKeyHasher> m_entries;

            std::map<int32_t, int64_t> m_counts;

            std::map<uint32_t, SwitchModel> m_switches;

            std::map<std::pair<Op, int32_t>, ApiStat> m_stats;

            uint64_t m_statsSinceNs;

            std::map<std::pair<Op, int32_t>, Delay> m_delays;

            bool m_hasDelays;

            std::atomic<bool> m_runControl;

            int m_controlFd;

            int m_stopFd;

            std::shared_ptr<std::thread> m_controlThread;

            std::mutex m_notifierMutex;

            std::vector<std::shared_ptr<std::thread>> m_notifiers;

            std::atomic<uint64_t> m_notifiersRunning;
    };

    /**
     * @brief Times one SAI call and accounts it when it goes out of scope.
     */
    class CallScope
    {
        public:

            CallScope(
                    _In_ MockSai& sai,
                    _In_ Op op,
                    _In_ sai_object_type_t objectType,
                    _In_ uint64_t objects);

            ~CallScope();

            sai_status_t done(
                    _In_ sai_status_t status);

            sai_status_t done(
                    _In_ sai_status_t status,
                    _In_ uint32_t count,
                    _In_ const sai_status_t* statuses);

        private:

            MockSai& m_sai;

            Op m_op;

            sai_object_type_t m_objectType;

            uint64_t m_objects;

            uint64_t m_errors;

            uint64_t m_unsupported;

            uint64_t m_startNs;
    };
}
