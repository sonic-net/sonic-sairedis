#pragma once

#include <functional>

#include "DummySaiInterface.h"

// Use gmock?
class MockableSaiInterface: public saimeta::DummySaiInterface
{
    public:

        MockableSaiInterface() {}

        virtual ~MockableSaiInterface() {}

    public:

        virtual sai_status_t initialize(
                _In_ uint64_t flags,
                _In_ const sai_service_method_table_t *service_method_table) override { return SAI_STATUS_SUCCESS; }

        virtual sai_status_t uninitialize(void) override { return SAI_STATUS_SUCCESS; }

    public: // SAI interface overrides

        virtual sai_status_t create(
                _In_ sai_object_type_t objectType,
                _Out_ sai_object_id_t* objectId,
                _In_ sai_object_id_t switchId,
                _In_ uint32_t attr_count,
                _In_ const sai_attribute_t *attr_list) override
        {
            if (mock_create)
            {
                return mock_create(objectType, objectId, switchId, attr_count, attr_list);
            }

            return SAI_STATUS_SUCCESS;
        }

        std::function<sai_status_t(sai_object_type_t, _Out_ sai_object_id_t*, sai_object_id_t, uint32_t, const sai_attribute_t*)> mock_create;

        virtual sai_status_t remove(
                _In_ sai_object_type_t objectType,
                _In_ sai_object_id_t objectId) override
        {
            if (mock_remove)
            {
                return mock_remove(objectType, objectId);
            }

            return SAI_STATUS_SUCCESS;
        }

        std::function<sai_status_t(sai_object_type_t, sai_object_id_t)> mock_remove;

        virtual sai_status_t set(
                _In_ sai_object_type_t objectType,
                _In_ sai_object_id_t objectId,
                _In_ const sai_attribute_t *attr) override
        {
            if (mock_set)
            {
                return mock_set(objectType, objectId, attr);
            }

            return SAI_STATUS_SUCCESS;
        }

        std::function<sai_status_t(sai_object_type_t, sai_object_id_t, const sai_attribute_t *)> mock_set;

        virtual sai_status_t get(
                _In_ sai_object_type_t objectType,
                _In_ sai_object_id_t objectId,
                _In_ uint32_t attr_count,
                _Inout_ sai_attribute_t *attr_list) override
        {
            if (mock_get)
            {
                return mock_get(objectType, objectId, attr_count, attr_list);
            }

            return SAI_STATUS_SUCCESS;
        }

        std::function<sai_status_t(sai_object_type_t, sai_object_id_t, uint32_t, sai_attribute_t *)> mock_get;

    public: // bulk QUAD oid

        virtual sai_status_t bulkCreate(
                _In_ sai_object_type_t object_type,
                _In_ sai_object_id_t switch_id,
                _In_ uint32_t object_count,
                _In_ const uint32_t *attr_count,
                _In_ const sai_attribute_t **attr_list,
                _In_ sai_bulk_op_error_mode_t mode,
                _Out_ sai_object_id_t *object_id,
                _Out_ sai_status_t *object_statuses) override
        {
            if (mock_bulkCreate)
            {
                return mock_bulkCreate(object_type, switch_id, object_count, attr_count, attr_list, mode, object_id, object_statuses);
            }

            return SAI_STATUS_SUCCESS;
        }

        std::function<sai_status_t(sai_object_type_t, sai_object_id_t, uint32_t, const uint32_t *, const sai_attribute_t **, sai_bulk_op_error_mode_t, sai_object_id_t *, sai_status_t*)> mock_bulkCreate;

        virtual sai_status_t bulkRemove(
                _In_ sai_object_type_t object_type,
                _In_ uint32_t object_count,
                _In_ const sai_object_id_t *object_id,
                _In_ sai_bulk_op_error_mode_t mode,
                _Out_ sai_status_t *object_statuses) override
        {
            if (mock_bulkRemove)
            {
                return mock_bulkRemove(object_type, object_count, object_id, mode, object_statuses);
            }

            return SAI_STATUS_SUCCESS;
        }

        std::function<sai_status_t(sai_object_type_t, uint32_t, const sai_object_id_t *, sai_bulk_op_error_mode_t, sai_status_t *)> mock_bulkRemove;

        virtual sai_status_t bulkSet(
                _In_ sai_object_type_t object_type,
                _In_ uint32_t object_count,
                _In_ const sai_object_id_t *object_id,
                _In_ const sai_attribute_t *attr_list,
                _In_ sai_bulk_op_error_mode_t mode,
                _Out_ sai_status_t *object_statuses) override
        {
            if (mock_bulkSet)
            {
                return mock_bulkSet(object_type, object_count, object_id, attr_list, mode, object_statuses);
            }

            return SAI_STATUS_SUCCESS;
        }

        std::function<sai_status_t(sai_object_type_t, uint32_t, const sai_object_id_t *, const sai_attribute_t *, sai_bulk_op_error_mode_t, sai_status_t *)> mock_bulkSet;

    public: // stats API

        virtual sai_status_t getStats(
                _In_ sai_object_type_t object_type,
                _In_ sai_object_id_t object_id,
                _In_ uint32_t number_of_counters,
                _In_ const sai_stat_id_t *counter_ids,
                _Out_ uint64_t *counters) override
        {
            if (mock_getStats)
            {
                return mock_getStats(object_type, object_id, number_of_counters, counter_ids, counters);
            }

            return SAI_STATUS_SUCCESS;
        }

        std::function<sai_status_t(sai_object_type_t, sai_object_id_t, uint32_t, const sai_stat_id_t *, uint64_t *)> mock_getStats;

        virtual sai_status_t queryStatsCapability(
                _In_ sai_object_id_t switch_id,
                _In_ sai_object_type_t object_type,
                _Inout_ sai_stat_capability_list_t *stats_capability) override
        {
            if (mock_queryStatsCapability)
            {
                return mock_queryStatsCapability(switch_id, object_type, stats_capability);
            }

            return SAI_STATUS_SUCCESS;
        }

        std::function<sai_status_t(sai_object_id_t, sai_object_type_t, sai_stat_capability_list_t *)> mock_queryStatsCapability;

        virtual sai_status_t getStatsExt(
                _In_ sai_object_type_t object_type,
                _In_ sai_object_id_t object_id,
                _In_ uint32_t number_of_counters,
                _In_ const sai_stat_id_t *counter_ids,
                _In_ sai_stats_mode_t mode,
                _Out_ uint64_t *counters) override
        {
            if (mock_getStatsExt)
            {
                return mock_getStatsExt(object_type, object_id, number_of_counters, counter_ids, mode, counters);
            }

            return SAI_STATUS_SUCCESS;
        }

        std::function<sai_status_t(sai_object_type_t, sai_object_id_t, uint32_t, const sai_stat_id_t *, sai_stats_mode_t, uint64_t *)> mock_getStatsExt;

        virtual sai_status_t clearStats(
                _In_ sai_object_type_t object_type,
                _In_ sai_object_id_t object_id,
                _In_ uint32_t number_of_counters,
                _In_ const sai_stat_id_t *counter_ids) override
        {
            if (mock_clearStats)
            {
                return mock_clearStats(object_type, object_id, number_of_counters, counter_ids);
            }

            return SAI_STATUS_SUCCESS;
        }

        std::function<sai_status_t(sai_object_type_t, sai_object_id_t, uint32_t, const sai_stat_id_t *)> mock_clearStats;


    public: // non QUAD API

        virtual sai_status_t flushFdbEntries(
                _In_ sai_object_id_t switchId,
                _In_ uint32_t attrCount,
                _In_ const sai_attribute_t *attrList) override
        {
            if (mock_flushFdbEntries)
            {
                return mock_flushFdbEntries(switchId, attrCount, attrList);
            }

            return SAI_STATUS_SUCCESS;
        }

        std::function<sai_status_t(sai_object_id_t, uint32_t, const sai_attribute_t *)> mock_flushFdbEntries;

    public: // SAI API

        virtual sai_status_t objectTypeGetAvailability(
                _In_ sai_object_id_t switchId,
                _In_ sai_object_type_t objectType,
                _In_ uint32_t attrCount,
                _In_ const sai_attribute_t *attrList,
                _Out_ uint64_t *count) override
        {
            if (mock_objectTypeGetAvailability)
            {
                return mock_objectTypeGetAvailability(switchId, objectType, attrCount, attrList, count);
            }

            return SAI_STATUS_SUCCESS;
        }

        std::function<sai_status_t(sai_object_id_t, sai_object_type_t, uint32_t, const sai_attribute_t *, uint64_t *)> mock_objectTypeGetAvailability;


        virtual sai_status_t queryAttributeCapability(
                _In_ sai_object_id_t switch_id,
                _In_ sai_object_type_t object_type,
                _In_ sai_attr_id_t attr_id,
                _Out_ sai_attr_capability_t *capability) override
        {
            if (mock_queryAttributeCapability)
            {
                return mock_queryAttributeCapability(switch_id, object_type, attr_id, capability);
            }

            return SAI_STATUS_SUCCESS;
        }

        std::function<sai_status_t(sai_object_id_t, sai_object_type_t, sai_attr_id_t, sai_attr_capability_t *)> mock_queryAttributeCapability;


        virtual sai_status_t queryAattributeEnumValuesCapability(
                _In_ sai_object_id_t switch_id,
                _In_ sai_object_type_t object_type,
                _In_ sai_attr_id_t attr_id,
                _Inout_ sai_s32_list_t *enum_values_capability) override
        {
            if (mock_queryAattributeEnumValuesCapability)
            {
                return mock_queryAattributeEnumValuesCapability(switch_id, object_type, attr_id, enum_values_capability);
            }

            return SAI_STATUS_SUCCESS;
        }

        std::function<sai_status_t(sai_object_id_t, sai_object_type_t, sai_attr_id_t, sai_s32_list_t *)> mock_queryAattributeEnumValuesCapability;

        virtual sai_object_type_t objectTypeQuery(
                _In_ sai_object_id_t objectId) override
        {
            if (mock_objectTypeQuery)
            {
                return mock_objectTypeQuery(objectId);
            }

            return SAI_OBJECT_TYPE_NULL;
        }

        std::function<sai_object_type_t(sai_object_id_t)> mock_objectTypeQuery;


        virtual sai_object_id_t switchIdQuery(
                _In_ sai_object_id_t objectId) override
        {
            if (mock_switchIdQuery)
            {
                return mock_switchIdQuery(objectId);
            }

            return 0;
        }

        std::function<sai_object_id_t(sai_object_id_t)> mock_switchIdQuery;

        virtual sai_status_t logSet(
                _In_ sai_api_t api,
                _In_ sai_log_level_t log_level) override
        {
            if (mock_logSet)
            {
                return mock_logSet(api, log_level);
            }

            return SAI_STATUS_SUCCESS;
        }

        std::function<sai_status_t(sai_api_t, sai_log_level_t)> mock_logSet;
};
