#pragma once

#include "SaiObj.h"
#include "AsicView.h"
#include "SaiSwitch.h"

#include <memory>

namespace syncd
{
    class BestCandidateFinder
    {
        private:

            typedef struct _sai_object_compare_info_t
            {
                size_t equal_attributes;

                std::shared_ptr<SaiObj> obj;

            } sai_object_compare_info_t;

        public:

            BestCandidateFinder(
                    _In_ const AsicView &currentView,
                    _In_ const AsicView &temporaryView,
                    _In_ std::shared_ptr<const SaiSwitch> sw);


            virtual ~BestCandidateFinder() = default;

        public:

            std::shared_ptr<SaiObj> findCurrentBestMatch(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj);

            std::shared_ptr<SaiObj> findSimilarBestMatch(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj);

        private:

            std::shared_ptr<SaiObj> findCurrentBestMatchForGenericObject(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj);

            std::shared_ptr<SaiObj> findCurrentBestMatchForLag(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
                    _In_ const std::vector<sai_object_compare_info_t> &candidateObjects);

            std::shared_ptr<SaiObj> findCurrentBestMatchForNextHopGroup(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
                    _In_ const std::vector<sai_object_compare_info_t> &candidateObjects);

            std::shared_ptr<SaiObj> findCurrentBestMatchForAclCounter(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
                    _In_ const std::vector<sai_object_compare_info_t> &candidateObjects);

            std::shared_ptr<SaiObj> findCurrentBestMatchForAclTableGroup(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
                    _In_ const std::vector<sai_object_compare_info_t> &candidateObjects);

            std::shared_ptr<SaiObj> findCurrentBestMatchForAclTable(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
                    _In_ const std::vector<sai_object_compare_info_t> &candidateObjects);

            std::shared_ptr<SaiObj> findCurrentBestMatchForRouterInterface(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
                    _In_ const std::vector<sai_object_compare_info_t> &candidateObjects);

            std::shared_ptr<SaiObj> findCurrentBestMatchForPolicer(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
                    _In_ const std::vector<sai_object_compare_info_t> &candidateObjects);

            std::shared_ptr<SaiObj> findCurrentBestMatchForHostifTrapGroup(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
                    _In_ const std::vector<sai_object_compare_info_t> &candidateObjects);

            std::shared_ptr<SaiObj> findCurrentBestMatchForBufferPool(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
                    _In_ const std::vector<sai_object_compare_info_t> &candidateObjects);

            std::shared_ptr<SaiObj> findCurrentBestMatchForBufferProfile(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
                    _In_ const std::vector<sai_object_compare_info_t> &candidateObjects);

            std::shared_ptr<SaiObj> findCurrentBestMatchForTunnelMap(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
                    _In_ const std::vector<sai_object_compare_info_t> &candidateObjects);

            std::shared_ptr<SaiObj> findCurrentBestMatchForWred(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
                    _In_ const std::vector<sai_object_compare_info_t> &candidateObjects);

            std::shared_ptr<SaiObj> findCurrentBestMatchForGenericObjectUsingPreMatchMap(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
                    _In_ const std::vector<sai_object_compare_info_t> &candidateObjects);

            std::shared_ptr<SaiObj> findCurrentBestMatchForGenericObjectUsingLabel(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
                    _In_ const std::vector<sai_object_compare_info_t> &candidateObjects);
                
            std::shared_ptr<SaiObj> findCurrentBestMatchForGenericObjectUsingLabel(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
                    _In_ const std::vector<sai_object_compare_info_t> &candidateObjects,
                    _In_ sai_attr_id_t attrId);

            std::shared_ptr<SaiObj> findCurrentBestMatchForGenericObjectUsingGraph(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
                    _In_ const std::vector<sai_object_compare_info_t> &candidateObjects);

            std::shared_ptr<SaiObj> findCurrentBestMatchForGenericObjectUsingHeuristic(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
                    _In_ const std::vector<sai_object_compare_info_t> &candidateObjects);

        private:

            std::shared_ptr<SaiObj> findCurrentBestMatchForNeighborEntry(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj);

            std::shared_ptr<SaiObj> findCurrentBestMatchForRouteEntry(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj);

            std::shared_ptr<SaiObj> findCurrentBestMatchForFdbEntry(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj);

            std::shared_ptr<SaiObj> findCurrentBestMatchForSwitch(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj);

            std::shared_ptr<SaiObj> findCurrentBestMatchForNatEntry(
                    _In_ const std::shared_ptr<const SaiObj> &temporaryObj);

        private:

            bool exchangeTemporaryVidToCurrentVid(
                    _Inout_ sai_object_meta_key_t &meta_key);

        private:

            static bool compareByEqualAttributes(
                    _In_ const sai_object_compare_info_t &a,
                    _In_ const sai_object_compare_info_t &b);

            static std::shared_ptr<SaiObj> selectRandomCandidate(
                    _In_ const std::vector<sai_object_compare_info_t> &candidateObjects);

            static int findAllChildsInDependencyTreeCount(
                    _In_ const AsicView &view,
                    _In_ const std::shared_ptr<const SaiObj> &obj);

            static std::vector<std::shared_ptr<const SaiObj>> findUsageCount(
                    _In_ const AsicView &view,
                    _In_ const std::shared_ptr<const SaiObj> &obj,
                    _In_ sai_object_type_t object_type,
                    _In_ sai_attr_id_t attr_id);

            static bool hasEqualQosMapList(
                    _In_ const std::shared_ptr<const SaiAttr> &current,
                    _In_ const std::shared_ptr<const SaiAttr> &temporary);

            static bool hasEqualObjectList(
                    _In_ const AsicView &currentView,
                    _In_ const AsicView &temporaryView,
                    _In_ uint32_t current_count,
                    _In_ const sai_object_id_t *current_list,
                    _In_ uint32_t temporary_count,
                    _In_ const sai_object_id_t *temporary_list);

        public:

            static bool hasEqualAttribute(
                    _In_ const AsicView &currentView,
                    _In_ const AsicView &temporaryView,
                    _In_ const std::shared_ptr<const SaiObj> &current,
                    _In_ const std::shared_ptr<const SaiObj> &temporary,
                    _In_ sai_attr_id_t id);

            static std::shared_ptr<SaiAttr> getSaiAttrFromDefaultValue(
                    _In_ const AsicView &currentView,
                    _In_ std::shared_ptr<const SaiSwitch> sw,
                    _In_ const sai_attr_metadata_t &meta);

            static bool hasEqualQosMapList(
                    _In_ const sai_qos_map_list_t& c,
                    _In_ const sai_qos_map_list_t& t);
        private:

            const AsicView& m_currentView;

            const AsicView& m_temporaryView;

            std::shared_ptr<const SaiSwitch> m_switch;

            std::shared_ptr<const SaiObj> m_temporaryObj;
            
            std::vector<sai_object_compare_info_t> m_candidateObjects;
    };
}
