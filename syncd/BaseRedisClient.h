#pragma once

extern "C" {
#include "saimetadata.h"
}

#include "swss/table.h"

#include <string>
#include <unordered_map>
#include <set>
#include <memory>
#include <vector>

namespace syncd
{
    class BaseRedisClient
    {
        public:

            BaseRedisClient() = default;

            virtual ~BaseRedisClient() = default;

        public:
            virtual bool isRedisEnabled() const = 0;

            virtual void clearLaneMap(
                    _In_ sai_object_id_t switchVid) const = 0;

            virtual std::unordered_map<sai_uint32_t, sai_object_id_t> getLaneMap(
                    _In_ sai_object_id_t switchVid) const = 0;

            virtual void saveLaneMap(
                    _In_ sai_object_id_t switchVid,
                    _In_ const std::unordered_map<sai_uint32_t, sai_object_id_t>& map) const = 0;

            virtual std::unordered_map<sai_object_id_t, sai_object_id_t> getVidToRidMap(
                    _In_ sai_object_id_t switchVid) const = 0;

            virtual std::unordered_map<sai_object_id_t, sai_object_id_t> getRidToVidMap(
                    _In_ sai_object_id_t switchVid) const = 0;

            virtual std::unordered_map<sai_object_id_t, sai_object_id_t> getVidToRidMap() const = 0;

            virtual std::unordered_map<sai_object_id_t, sai_object_id_t> getRidToVidMap() const = 0;

            virtual void setDummyAsicStateObject(
                    _In_ sai_object_id_t objectVid) = 0;

            virtual void setDummyAsicStateObjects(
                    _In_ size_t count,
                    _In_ const sai_object_id_t* objectVids) = 0;

            virtual void saveColdBootDiscoveredVids(
                    _In_ sai_object_id_t switchVid,
                    _In_ const std::set<sai_object_id_t>& coldVids) = 0;

            virtual std::shared_ptr<std::string> getSwitchHiddenAttribute(
                    _In_ sai_object_id_t switchVid,
                    _In_ const std::string& attrIdName) = 0;

            virtual void saveSwitchHiddenAttribute(
                    _In_ sai_object_id_t switchVid,
                    _In_ const std::string& attrIdName,
                    _In_ sai_object_id_t objectRid) = 0;

            virtual std::set<sai_object_id_t> getColdVids(
                    _In_ sai_object_id_t switchVid) = 0;

            virtual void setPortLanes(
                    _In_ sai_object_id_t switchVid,
                    _In_ sai_object_id_t portRid,
                    _In_ const std::vector<uint32_t>& lanes) = 0;

            virtual size_t getAsicObjectsSize(
                    _In_ sai_object_id_t switchVid) const = 0;

            virtual int removePortFromLanesMap(
                    _In_ sai_object_id_t switchVid,
                    _In_ sai_object_id_t portRid) const = 0;

            virtual void removeAsicObject(
                    _In_ sai_object_id_t objectVid) const = 0;

            virtual void removeAsicObject(
                    _In_ const sai_object_meta_key_t& metaKey) = 0;

            virtual void removeTempAsicObject(
                    _In_ const sai_object_meta_key_t& metaKey) = 0;

            virtual void removeAsicObjects(
                    _In_ const std::vector<std::string>& keys) = 0;

            virtual void removeTempAsicObjects(
                    _In_ const std::vector<std::string>& keys) = 0;

            virtual void createAsicObject(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ const std::vector<swss::FieldValueTuple>& attrs) = 0;

            virtual void createTempAsicObject(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ const std::vector<swss::FieldValueTuple>& attrs) = 0;

            virtual void createAsicObjects(
                    _In_ const std::unordered_map<std::string, std::vector<swss::FieldValueTuple>>& multiHash) = 0;

            virtual void createTempAsicObjects(
                    _In_ const std::unordered_map<std::string, std::vector<swss::FieldValueTuple>>& multiHash) = 0;

            virtual void setVidAndRidMap(
                    _In_ const std::unordered_map<sai_object_id_t, sai_object_id_t>& map) = 0;

            virtual std::vector<std::string> getAsicStateKeys() const = 0;

            virtual std::vector<std::string> getAsicStateSwitchesKeys() const = 0;

            virtual void removeColdVid(
                    _In_ sai_object_id_t vid) = 0;

            virtual std::unordered_map<std::string, std::string> getAttributesFromAsicKey(
                    _In_ const std::string& key) const = 0;

            virtual bool hasNoHiddenKeysDefined() const = 0;

            virtual void removeVidAndRid(
                    _In_ sai_object_id_t vid,
                    _In_ sai_object_id_t rid) = 0;

            virtual void insertVidAndRid(
                    _In_ sai_object_id_t vid,
                    _In_ sai_object_id_t rid) = 0;

            virtual void insertVidsAndRids(
                    _In_ size_t count,
                    _In_ const sai_object_id_t* vids,
                    _In_ const sai_object_id_t* rids) = 0;

            virtual sai_object_id_t getVidForRid(
                    _In_ sai_object_id_t rid) = 0;

            virtual sai_object_id_t getRidForVid(
                    _In_ sai_object_id_t vid) = 0;

            virtual void getVidsForRids(
                    _In_ size_t count,
                    _In_ const sai_object_id_t* rids,
                    _Out_ sai_object_id_t* vids) = 0;

            virtual void removeAsicStateTable() = 0;

            virtual void removeTempAsicStateTable() = 0;

            virtual std::map<sai_object_id_t, swss::TableDump> getAsicView() = 0;

            virtual std::map<sai_object_id_t, swss::TableDump> getTempAsicView() = 0;

            virtual void setAsicObject(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ const std::string& attr,
                    _In_ const std::string& value) = 0;

            virtual void setTempAsicObject(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ const std::string& attr,
                    _In_ const std::string& value) = 0;

            virtual void processFlushEvent(
                    _In_ sai_object_id_t switchVid,
                    _In_ sai_object_id_t portVid,
                    _In_ sai_object_id_t bvId,
                    _In_ sai_fdb_flush_entry_type_t type) = 0;
    };
}

