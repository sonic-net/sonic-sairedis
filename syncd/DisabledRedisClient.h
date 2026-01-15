#pragma once

#include "BaseRedisClient.h"

namespace syncd
{
    class DisabledRedisClient: public BaseRedisClient
    {
        public:

            DisabledRedisClient() = default;

            virtual ~DisabledRedisClient() = default;

        public:
            virtual bool isRedisEnabled() const override;

            virtual void clearLaneMap(
                    _In_ sai_object_id_t switchVid) const override;

            virtual std::unordered_map<sai_uint32_t, sai_object_id_t> getLaneMap(
                    _In_ sai_object_id_t switchVid) const override;

            virtual void saveLaneMap(
                    _In_ sai_object_id_t switchVid,
                    _In_ const std::unordered_map<sai_uint32_t, sai_object_id_t>& map) const override;

            virtual std::unordered_map<sai_object_id_t, sai_object_id_t> getVidToRidMap(
                    _In_ sai_object_id_t switchVid) const override;

            virtual std::unordered_map<sai_object_id_t, sai_object_id_t> getRidToVidMap(
                    _In_ sai_object_id_t switchVid) const override;

            virtual std::unordered_map<sai_object_id_t, sai_object_id_t> getVidToRidMap() const override;

            virtual std::unordered_map<sai_object_id_t, sai_object_id_t> getRidToVidMap() const override;

            virtual void setDummyAsicStateObject(
                    _In_ sai_object_id_t objectVid) override;

            virtual void setDummyAsicStateObjects(
                    _In_ size_t count,
                    _In_ const sai_object_id_t* objectVids) override;

            virtual void saveColdBootDiscoveredVids(
                    _In_ sai_object_id_t switchVid,
                    _In_ const std::set<sai_object_id_t>& coldVids) override;

            virtual std::shared_ptr<std::string> getSwitchHiddenAttribute(
                    _In_ sai_object_id_t switchVid,
                    _In_ const std::string& attrIdName) override;

            virtual void saveSwitchHiddenAttribute(
                    _In_ sai_object_id_t switchVid,
                    _In_ const std::string& attrIdName,
                    _In_ sai_object_id_t objectRid) override;

            virtual std::set<sai_object_id_t> getColdVids(
                    _In_ sai_object_id_t switchVid) override;

            virtual void setPortLanes(
                    _In_ sai_object_id_t switchVid,
                    _In_ sai_object_id_t portRid,
                    _In_ const std::vector<uint32_t>& lanes) override;

            virtual size_t getAsicObjectsSize(
                    _In_ sai_object_id_t switchVid) const override;

            virtual int removePortFromLanesMap(
                    _In_ sai_object_id_t switchVid,
                    _In_ sai_object_id_t portRid) const override;

            virtual void removeAsicObject(
                    _In_ sai_object_id_t objectVid) const override;

            virtual void removeAsicObject(
                    _In_ const sai_object_meta_key_t& metaKey) override;

            virtual void removeTempAsicObject(
                    _In_ const sai_object_meta_key_t& metaKey) override;

            virtual void removeAsicObjects(
                    _In_ const std::vector<std::string>& keys) override;

            virtual void removeTempAsicObjects(
                    _In_ const std::vector<std::string>& keys) override;

            virtual void createAsicObject(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ const std::vector<swss::FieldValueTuple>& attrs) override;

            virtual void createTempAsicObject(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ const std::vector<swss::FieldValueTuple>& attrs) override;

            virtual void createAsicObjects(
                    _In_ const std::unordered_map<std::string, std::vector<swss::FieldValueTuple>>& multiHash) override;

            virtual void createTempAsicObjects(
                    _In_ const std::unordered_map<std::string, std::vector<swss::FieldValueTuple>>& multiHash) override;

            virtual void setVidAndRidMap(
                    _In_ const std::unordered_map<sai_object_id_t, sai_object_id_t>& map) override;

            virtual std::vector<std::string> getAsicStateKeys() const override;

            virtual std::vector<std::string> getAsicStateSwitchesKeys() const override;

            virtual void removeColdVid(
                    _In_ sai_object_id_t vid) override;

            virtual std::unordered_map<std::string, std::string> getAttributesFromAsicKey(
                    _In_ const std::string& key) const override;

            virtual bool hasNoHiddenKeysDefined() const override;

            virtual void removeVidAndRid(
                    _In_ sai_object_id_t vid,
                    _In_ sai_object_id_t rid) override;

            virtual void insertVidAndRid(
                    _In_ sai_object_id_t vid,
                    _In_ sai_object_id_t rid) override;

            virtual void insertVidsAndRids(
                    _In_ size_t count,
                    _In_ const sai_object_id_t* vids,
                    _In_ const sai_object_id_t* rids) override;

            virtual sai_object_id_t getVidForRid(
                    _In_ sai_object_id_t rid) override;

            virtual sai_object_id_t getRidForVid(
                    _In_ sai_object_id_t vid) override;

            virtual void getVidsForRids(
                    _In_ size_t count,
                    _In_ const sai_object_id_t* rids,
                    _Out_ sai_object_id_t* vids) override;

            virtual void removeAsicStateTable() override;

            virtual void removeTempAsicStateTable() override;

            virtual std::map<sai_object_id_t, swss::TableDump> getAsicView() override;

            virtual std::map<sai_object_id_t, swss::TableDump> getTempAsicView() override;

            virtual void setAsicObject(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ const std::string& attr,
                    _In_ const std::string& value) override;

            virtual void setTempAsicObject(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ const std::string& attr,
                    _In_ const std::string& value) override;

            virtual void processFlushEvent(
                    _In_ sai_object_id_t switchVid,
                    _In_ sai_object_id_t portVid,
                    _In_ sai_object_id_t bvId,
                    _In_ sai_fdb_flush_entry_type_t type) override;
    };
}

