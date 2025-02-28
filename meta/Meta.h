#pragma once

#include "SaiInterface.h"

#include "SaiAttrWrapper.h"
#include "SaiObjectCollection.h"
#include "PortRelatedSet.h"
#include "AttrKeyMap.h"
#include "OidRefCounter.h"

#include "swss/table.h"

#include <vector>
#include <memory>
#include <set>

#define DEFAULT_VLAN_NUMBER 1
#define MINIMUM_VLAN_NUMBER 1
#define MAXIMUM_VLAN_NUMBER 4094

namespace saimeta
{
    class Meta:
        public sairedis::SaiInterface
    {
        public:

            using sairedis::SaiInterface::set; // name hiding

            Meta(
                    _In_ std::shared_ptr<SaiInterface> impl);

            virtual ~Meta() = default;

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

        public:

            void meta_init_db();

            bool isEmpty() const;

            void dump() const;

        public: // notifications

            void meta_sai_on_fdb_event(
                    _In_ uint32_t count,
                    _In_ const sai_fdb_event_notification_data_t *data);

            void meta_sai_on_nat_event(
                    _In_ uint32_t count,
                    _In_ const sai_nat_event_notification_data_t *data);

            void meta_sai_on_switch_state_change(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_switch_oper_status_t switch_oper_status);

            void meta_sai_on_switch_asic_sdk_health_event(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_switch_asic_sdk_health_severity_t severity,
                    _In_ sai_timespec_t timestamp,
                    _In_ sai_switch_asic_sdk_health_category_t category,
                    _In_ sai_switch_health_data_t data,
                    _In_ const sai_u8_list_t description);

            void meta_sai_on_switch_shutdown_request(
                    _In_ sai_object_id_t switch_id);

            void meta_sai_on_port_state_change(
                    _In_ uint32_t count,
                    _In_ const sai_port_oper_status_notification_t *data);

            void meta_sai_on_queue_pfc_deadlock_notification(
                    _In_ uint32_t count,
                    _In_ const sai_queue_deadlock_notification_data_t *data);

            void meta_sai_on_bfd_session_state_change(
                    _In_ uint32_t count,
                    _In_ const sai_bfd_session_state_notification_t *data);

            void meta_sai_on_port_host_tx_ready_change(
                    _In_ sai_object_id_t port_id,
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_port_host_tx_ready_status_t host_tx_ready_status);

            void meta_sai_on_twamp_session_event(
                    _In_ uint32_t count,
                    _In_ const sai_twamp_session_event_notification_data_t *data);

        private: // notifications helpers

            void meta_sai_on_fdb_flush_event_consolidated(
                    _In_ const sai_fdb_event_notification_data_t& data);

            void meta_fdb_event_snoop_oid(
                    _In_ sai_object_id_t oid);

            void meta_sai_on_fdb_event_single(
                    _In_ const sai_fdb_event_notification_data_t& data);

            void meta_sai_on_nat_event_single(
                    _In_ const sai_nat_event_notification_data_t& data);

            void meta_sai_on_port_state_change_single(
                    _In_ const sai_port_oper_status_notification_t& data);

            void meta_sai_on_queue_pfc_deadlock_notification_single(
                    _In_ const sai_queue_deadlock_notification_data_t& data);

            void meta_sai_on_bfd_session_state_change_single(
                    _In_ const sai_bfd_session_state_notification_t& data);

            void meta_sai_on_twamp_session_event_single(
                    _In_ const sai_twamp_session_event_notification_data_t& data);

        private: // validation helpers

            sai_status_t meta_generic_validation_objlist(
                    _In_ const sai_attr_metadata_t& md,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t count,
                    _In_ const sai_object_id_t* list);

            sai_status_t meta_genetic_validation_list(
                    _In_ const sai_attr_metadata_t& md,
                    _In_ uint32_t count,
                    _In_ const void* list);

            sai_status_t meta_generic_validate_non_object_on_create(
                    _In_ const sai_object_meta_key_t& meta_key,
                    _In_ sai_object_id_t switch_id);

            sai_object_id_t meta_extract_switch_id(
                    _In_ const sai_object_meta_key_t& meta_key,
                    _In_ sai_object_id_t switch_id);

            std::shared_ptr<SaiAttrWrapper> get_object_previous_attr(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ const sai_attr_metadata_t& md);

            std::vector<const sai_attr_metadata_t*> get_attributes_metadata(
                    _In_ sai_object_type_t objecttype);

            void meta_generic_validation_post_get_objlist(
                    _In_ const sai_object_meta_key_t& meta_key,
                    _In_ const sai_attr_metadata_t& md,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t count,
                    _In_ const sai_object_id_t* list);

        public:

            static bool is_ipv6_mask_valid(
                    _In_ const uint8_t* mask);

        private: // unit tests helpers

            bool meta_unittests_get_and_erase_set_readonly_flag(
                    _In_ const sai_attr_metadata_t& md);

        public:

            /**
             * @brief Enable unittest globally.
             *
             * @param[in] enable If set to true unittests are enabled.
             */
            void meta_unittests_enable(
                    _In_ bool enable);

            /**
             * @brief Indicates whether unittests are enabled;
             */
            bool meta_unittests_enabled();


            /**
             * @brief Allow to perform SET operation on READ_ONLY attribute only once.
             *
             * This function relaxes metadata checking on SET operation, it allows to
             * perform SET api on READ_ONLY attribute only once on specific object type and
             * specific attribute.
             *
             * Once means that SET operation is only relaxed for the very next SET call on
             * that specific object type and attribute id.
             *
             * Function is explicitly named ONCE, since it will force test developer to not
             * forget that SET check is relaxed, and not forget for future unittests.
             *
             * Function is provided for more flexible testing using virtual switch.  Since
             * some of the read only attributes maybe very complex to simulate (for example
             * resources used by actual asic when adding next hop or next hop group), then
             * it's easier to write such unittest:
             *
             * TestCase:
             * 1. meta_unittests_allow_readonly_set_once(x,y);
             * 2. object_x_api->set_attribute(object_id, attr, foo); // attr.id == y
             * 3. object_x_api->get_attribute(object_id, 1, attr); // attr.id == y
             * 4. check if get result is equal to set result.
             *
             * On real ASIC, even after allowing SET on read only attribute, actual SET
             * should fail.
             *
             * It can be dangerous to set any readonly attribute to different values since
             * internal metadata logic maybe using that value and in some cases metadata
             * database may get out of sync and cause unexpected results in api calls up to
             * application crash.
             *
             * This function is not thread safe.
             *
             * @param[in] object_type Object type on which SET will be possible.
             * @param[in] attr_id Attribute ID on which SET will be possible.
             *
             * @return #SAI_STATUS_SUCCESS on success Failure status code on error
             */
            sai_status_t meta_unittests_allow_readonly_set_once(
                    _In_ sai_object_type_t object_type,
                    _In_ int32_t attr_id);

        public: // unittests method helpers

            int32_t getObjectReferenceCount(
                    _In_ sai_object_id_t oid) const;

            bool objectExists(
                    _In_ const sai_object_meta_key_t& mk) const;

        private: // port helpers

            sai_status_t meta_port_remove_validation(
                    _In_ const sai_object_meta_key_t& meta_key);

            bool meta_is_object_in_default_state(
                    _In_ sai_object_id_t oid);

            void post_port_remove(
                    _In_ const sai_object_meta_key_t& meta_key);

            void meta_post_port_get(
                    _In_ const sai_object_meta_key_t& meta_key,
                    _In_ sai_object_id_t switch_id,
                    _In_ const uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            void meta_add_port_to_related_map(
                    _In_ sai_object_id_t port_id,
                    _In_ const sai_object_list_t& list);

        public: // validation post QUAD

            void meta_generic_validation_post_create(
                    _In_ const sai_object_meta_key_t& meta_key,
                    _In_ sai_object_id_t switch_id,
                    _In_ const uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            void meta_generic_validation_post_remove(
                    _In_ const sai_object_meta_key_t& meta_key);

            void meta_generic_validation_post_set(
                    _In_ const sai_object_meta_key_t& meta_key,
                    _In_ const sai_attribute_t *attr);

            void meta_generic_validation_post_get(
                    _In_ const sai_object_meta_key_t& meta_key,
                    _In_ sai_object_id_t switch_id,
                    _In_ const uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

        private: // validation QUAD

            sai_status_t meta_generic_validation_create(
                    _In_ const sai_object_meta_key_t& meta_key,
                    _In_ sai_object_id_t switch_id,
                    _In_ const uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t meta_generic_validation_remove(
                    _In_ const sai_object_meta_key_t& meta_key);

            sai_status_t meta_generic_validation_set(
                    _In_ const sai_object_meta_key_t& meta_key,
                    _In_ const sai_attribute_t *attr);

            sai_status_t meta_generic_validation_get(
                    _In_ const sai_object_meta_key_t& meta_key,
                    _In_ const uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

        protected: // stats

            sai_status_t meta_validate_stats(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _Out_ uint64_t *counters,
                    _In_ sai_stats_mode_t mode);

        private: // validate OID

            sai_status_t meta_sai_validate_oid(
                    _In_ sai_object_type_t object_type,
                    _In_ const sai_object_id_t* object_id,
                    _In_ sai_object_id_t switch_id,
                    _In_ bool create);

        private: // validate ENTRY

            sai_status_t meta_sai_validate_fdb_entry(
                    _In_ const sai_fdb_entry_t* fdb_entry,
                    _In_ bool create,
                    _In_ bool get = false);

            sai_status_t meta_sai_validate_mcast_fdb_entry(
                    _In_ const sai_mcast_fdb_entry_t* mcast_fdb_entry,
                    _In_ bool create,
                    _In_ bool get = false);

            sai_status_t meta_sai_validate_neighbor_entry(
                    _In_ const sai_neighbor_entry_t* neighbor_entry,
                    _In_ bool create,
                    _In_ bool get = false);

            sai_status_t meta_sai_validate_route_entry(
                    _In_ const sai_route_entry_t* route_entry,
                    _In_ bool create,
                    _In_ bool get = false);

            sai_status_t meta_sai_validate_l2mc_entry(
                    _In_ const sai_l2mc_entry_t* l2mc_entry,
                    _In_ bool create,
                    _In_ bool get = false);

            sai_status_t meta_sai_validate_ipmc_entry(
                    _In_ const sai_ipmc_entry_t* ipmc_entry,
                    _In_ bool create,
                    _In_ bool get = false);

            sai_status_t meta_sai_validate_nat_entry(
                    _In_ const sai_nat_entry_t* nat_entry,
                    _In_ bool create,
                    _In_ bool get = false);

            sai_status_t meta_sai_validate_inseg_entry(
                    _In_ const sai_inseg_entry_t* inseg_entry,
                    _In_ bool create,
                    _In_ bool get = false);

            sai_status_t meta_sai_validate_my_sid_entry(
                    _In_ const sai_my_sid_entry_t* my_sid_entry,
                    _In_ bool create,
                    _In_ bool get = false);

             sai_status_t meta_sai_validate_direction_lookup_entry(
                    _In_ const sai_direction_lookup_entry_t* direction_lookup_entry,
                    _In_ bool create,
                    _In_ bool get = false);

             sai_status_t meta_sai_validate_eni_ether_address_map_entry(
                    _In_ const sai_eni_ether_address_map_entry_t* eni_ether_address_map_entry,
                    _In_ bool create,
                    _In_ bool get = false);

             sai_status_t meta_sai_validate_vip_entry(
                    _In_ const sai_vip_entry_t* vip_entry,
                    _In_ bool create,
                    _In_ bool get = false);

             sai_status_t meta_sai_validate_inbound_routing_entry(
                    _In_ const sai_inbound_routing_entry_t* inbound_routing_entry,
                    _In_ bool create,
                    _In_ bool get = false);

             sai_status_t meta_sai_validate_pa_validation_entry(
                    _In_ const sai_pa_validation_entry_t* pa_validation_entry,
                    _In_ bool create,
                    _In_ bool get = false);

             sai_status_t meta_sai_validate_outbound_routing_entry(
                    _In_ const sai_outbound_routing_entry_t* outbound_routing_entry,
                    _In_ bool create,
                    _In_ bool get = false);

             sai_status_t meta_sai_validate_outbound_ca_to_pa_entry(
                    _In_ const sai_outbound_ca_to_pa_entry_t* outbound_ca_to_pa_entry,
                    _In_ bool create,
                    _In_ bool get = false);

             sai_status_t meta_sai_validate_flow_entry(
                     _In_ const sai_flow_entry_t* flow_entry,
                     _In_ bool create,
                     _In_ bool get = false);

             sai_status_t meta_sai_validate_meter_bucket_entry(
                     _In_ const sai_meter_bucket_entry_t* meter_bucket_entry,
                     _In_ bool create,
                     _In_ bool get = false);

        public:

            /*
             * Those functions will be used to recreate virtual switch local metadata state
             * after WARM BOOT.
             */

            void meta_warm_boot_notify();

            void populate(
                    _In_ const swss::TableDump& dump);

        private:

            void clean_after_switch_remove(
                    _In_ sai_object_id_t switchId);

        private:

            std::shared_ptr<sairedis::SaiInterface> m_implementation;

        private: // database objects


            /**
             * @brief Port related objects set.
             *
             * Key in map is port OID, and value is set of related objects ids
             * like queues, ipgs and scheduler groups.
             *
             * This map will help to identify objects to be automatically removed
             * when port will be removed.
             */
            PortRelatedSet m_portRelatedSet;

            /*
             * Non object ids don't need reference count since they are leafs and can be
             * removed at any time.
             */

            OidRefCounter m_oids;

            SaiObjectCollection m_saiObjectCollection;

            AttrKeyMap m_attrKeys;

        private: // unittests

            std::set<std::string> m_meta_unittests_set_readonly_set;

            bool m_unittestsEnabled;

        private: // warm boot

            bool m_warmBoot;
    };
}
