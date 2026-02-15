#pragma once

#include "SaiSwitch.h"
#include "VirtualOidTranslator.h"
#include "BaseRedisClient.h"
#include "NotificationHandler.h"

#include "meta/SaiInterface.h"
#include "meta/SaiAttributeList.h"

#include <string>
#include <unordered_map>
#include <map>
#include <vector>
#include <memory>

namespace syncd
{
    class SingleReiniter
    {
        public:

            typedef std::unordered_map<std::string, std::string> StringHash;
            typedef std::unordered_map<sai_object_id_t, sai_object_id_t> ObjectIdMap;

        public:

            SingleReiniter(
                    _In_ std::shared_ptr<BaseRedisClient> client,
                    _In_ std::shared_ptr<VirtualOidTranslator> translator,
                    _In_ std::shared_ptr<sairedis::SaiInterface> sai,
                    _In_ std::shared_ptr<NotificationHandler> handler,
                    _In_ const ObjectIdMap& vidToRidMap,
                    _In_ const ObjectIdMap& ridToVidMap,
                    _In_ const std::vector<std::string>& asicKeys);

            virtual ~SingleReiniter();

        public:

            std::shared_ptr<SaiSwitch> hardReinit();

            void postRemoveActions();

            ObjectIdMap getTranslatedVid2Rid() const;

            std::shared_ptr<SaiSwitch> getSwitch() const;

        private:

            void prepareAsicState();

            void checkAllIds();

            void processSwitches();

            void processFdbs();

            void processNeighbors();

            void processOids();

            void processRoutes(
                    _In_ bool defaultOnly);

            void processNatEntries();

            void processInsegs();

            sai_object_id_t processSingleVid(
                    _In_ sai_object_id_t vid);

            std::shared_ptr<saimeta::SaiAttributeList> redisGetAttributesFromAsicKey(
                    _In_ const std::string &key);

            void processAttributesForOids(
                    _In_ sai_object_type_t objectType,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            void processStructNonObjectIds(
                    _In_ sai_object_meta_key_t &meta_key);

            void listFailedAttributes(
                    _In_ sai_object_type_t objectType,
                    _In_ uint32_t attrCount,
                    _In_ const sai_attribute_t* attrList);

            void trapGroupWorkaround(
                    _In_ sai_object_id_t vid,
                    _Inout_ sai_object_id_t& rid,
                    _In_ bool& createObject,
                    _In_ uint32_t attrCount,
                    _In_ const sai_attribute_t* attrList);

        public:

            static sai_object_type_t getObjectTypeFromAsicKey(
                    _In_ const std::string &key);

            static std::string getObjectIdFromAsicKey(
                    _In_ const std::string &key);

        private:

            std::shared_ptr<sairedis::SaiInterface> m_vendorSai;

            ObjectIdMap m_translatedV2R;
            ObjectIdMap m_translatedR2V;

            ObjectIdMap m_vidToRidMap;
            ObjectIdMap m_ridToVidMap;

            StringHash m_oids;
            StringHash m_switches;
            StringHash m_fdbs;
            StringHash m_routes;
            StringHash m_neighbors;
            StringHash m_nats;
            StringHash m_insegs;

            std::vector<std::string> m_asicKeys;

            std::unordered_map<std::string, std::shared_ptr<saimeta::SaiAttributeList>> m_attributesLists;

            std::map<sai_object_type_t, std::tuple<int,double>> m_perf_create;
            std::map<sai_object_type_t, std::tuple<int,double>> m_perf_set;

            sai_object_id_t m_switch_rid;
            sai_object_id_t m_switch_vid;

            std::shared_ptr<SaiSwitch> m_sw;

            std::shared_ptr<VirtualOidTranslator> m_translator;

            std::shared_ptr<BaseRedisClient> m_client;

            std::shared_ptr<NotificationHandler> m_handler;
    };
}
