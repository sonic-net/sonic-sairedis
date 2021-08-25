#pragma once

extern "C"{
#include "sai.h"
}

#include "AsicView.h"
#include "VendorSai.h"
#include "SaiSwitchInterface.h"
#include "VirtualOidTranslator.h"
#include "NotificationHandler.h"
#include "BreakConfig.h"

#include <set>

namespace syncd
{
    class ComparisonLogic
    {
        public:

            ComparisonLogic(
                _In_ std::shared_ptr<sairedis::SaiInterface> vendorSai,
                _In_ std::shared_ptr<SaiSwitchInterface> sw,
                _In_ std::shared_ptr<NotificationHandler> handler,
                _In_ std::set<sai_object_id_t> initViewRemovedVids,
                _In_ std::shared_ptr<AsicView> current,
                _In_ std::shared_ptr<AsicView> temp,
                _In_ std::shared_ptr<BreakConfig> breakConfig);

            virtual ~ComparisonLogic();;

        public:

            bool checkAsicVsDatabaseConsistency(
                    _In_ std::shared_ptr<VirtualOidTranslator> translator);


            void executeOperationsOnAsic();

            void compareViews();

        private:

            void matchOids(
                    _In_ AsicView& currentView,
                    _In_ AsicView& temporaryView);

            void populateExistingObjects(
                    _In_ AsicView& currentView,
                    _In_ AsicView& temporaryView);

            void createPreMatchMap(
                    _In_ const AsicView& cur,
                    _Inout_ AsicView& tmp);

            void applyViewTransition(
                    _In_ AsicView& current,
                    _In_ AsicView& temp);

            void transferNotProcessed(
                    _In_ AsicView& current,
                    _In_ AsicView& temp);

            void checkInternalObjects(
                    _In_ const AsicView& cv,
                    _In_ const AsicView& tv);

            void logViewObjectCount(
                    _In_ const AsicView& currentView,
                    _In_ const AsicView& temporaryView);

            void checkMap(
                    _In_ const AsicView& cur,
                    _In_ const AsicView& tmp) const;

        private:

            void checkMap(
                    _In_ const AsicView::ObjectIdMap& firstR2V,
                    _In_ const char* firstR2Vname,
                    _In_ const AsicView::ObjectIdMap& firstV2R,
                    _In_ const char * firstV2Rname,
                    _In_ const AsicView::ObjectIdMap& secondR2V,
                    _In_ const char* secondR2Vname,
                    _In_ const AsicView::ObjectIdMap& secondV2R,
                    _In_ const char *secondV2Rname) const;


            void checkMatchedPorts(
                    _In_ const AsicView& temporaryView);

            void procesObjectAttributesForViewTransition(
                    _In_ AsicView& currentView,
                    _In_ AsicView& temporaryView,
                    _In_ const std::shared_ptr<SaiObj>& temporaryObj);

            void bringNonRemovableObjectToDefaultState(
                    _In_ AsicView& currentView,
                    _In_ const std::shared_ptr<SaiObj>& currentObj);

            bool isNonRemovableObject(
                    _In_ const AsicView& currentView,
                    _In_ const AsicView& temporaryView,
                    _In_ const std::shared_ptr<const SaiObj>& currentObj);

            void removeExistingObjectFromCurrentView(
                    _In_ AsicView& currentView,
                    _In_ const AsicView& temporaryView,
                    _In_ const std::shared_ptr<SaiObj>& currentObj);

            sai_object_id_t translateTemporaryVidToCurrentVid(
                    _In_ const AsicView& currentView,
                    _In_ const AsicView& temporaryView,
                    _In_ sai_object_id_t tvid);

            std::shared_ptr<SaiAttr> translateTemporaryVidsToCurrentVids(
                    _In_ const AsicView& currentView,
                    _In_ const AsicView& temporaryView,
                    _In_ const std::shared_ptr<SaiObj>& currentObj,
                    _In_ const std::shared_ptr<SaiAttr>& inattr);

            void setAttributeOnCurrentObject(
                    _In_ AsicView& currentView,
                    _In_ const AsicView& temporaryView,
                    _In_ const std::shared_ptr<SaiObj>& currentObj,
                    _In_ const std::shared_ptr<SaiAttr>& inattr);

            void createNewObjectFromTemporaryObject(
                    _In_ AsicView& currentView,
                    _In_ const AsicView& temporaryView,
                    _In_ const std::shared_ptr<SaiObj>& temporaryObj);

            void updateObjectStatus(
                    _In_ AsicView& currentView,
                    _In_ AsicView& temporaryView,
                    _In_ const std::shared_ptr<SaiObj>& currentBestMatch,
                    _In_ const std::shared_ptr<SaiObj>& temporaryObj);

            bool performObjectSetTransition(
                    _In_ AsicView& currentView,
                    _In_ AsicView& temporaryView,
                    _In_ const std::shared_ptr<SaiObj> currentBestMatch,
                    _In_ const std::shared_ptr<SaiObj> temporaryObj,
                    _In_ bool performTransition);

            void breakBeforeMake(
                    _In_ AsicView &currentView,
                    _In_ AsicView &temporaryView,
                    _In_ const std::shared_ptr<SaiObj>& currentBestMatch,
                    _In_ const std::shared_ptr<SaiObj>& temporaryObj);

            void removeCurrentObjectDependencyTree(
                    _In_ AsicView &currentView,
                    _In_ AsicView &temporaryView,
                    _In_ const std::shared_ptr<SaiObj>& currentObj);

            void processObjectForViewTransition(
                    _In_ AsicView& currentView,
                    _In_ AsicView& temporaryView,
                    _Inout_ std::shared_ptr<SaiObj> temporaryObj);

            void checkSwitch(
                    _In_ const AsicView& currentView,
                    _In_ const AsicView& temporaryView);

            void bringDefaultTrapGroupToFinalState(
                    _In_ AsicView& currentView,
                    _In_ AsicView& temporaryView);

            void createPreMatchMapForObject(
                    _In_ const AsicView& cur,
                    _Inout_ AsicView& tmp,
                    _In_ std::shared_ptr<const SaiObj> cObj,
                    _In_ std::shared_ptr<const SaiObj> tObj,
                    _Inout_ std::set<std::string>& processed);


            sai_object_id_t asic_translate_vid_to_rid(
                    _In_ const AsicView& current,
                    _In_ const AsicView& temporary,
                    _In_ sai_object_id_t vid);

            void asic_translate_list_vid_to_rid(
                    _In_ const AsicView& current,
                    _In_ const AsicView& temporary,
                    _Inout_ sai_object_list_t& element);

            void asic_translate_vid_to_rid_list(
                    _In_ const AsicView& current,
                    _In_ const AsicView& temporary,
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t attr_count,
                    _Inout_ sai_attribute_t *attr_list);

            sai_status_t asic_handle_generic(
                    _In_ AsicView& current,
                    _In_ AsicView& temporary,
                    _In_ sai_object_meta_key_t& meta_key,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            void asic_translate_vid_to_rid_non_object_id(
                    _In_ const AsicView& current,
                    _In_ const AsicView& temporary,
                    _In_ sai_object_meta_key_t& meta_key);

            sai_status_t asic_handle_non_object_id(
                    _In_ const AsicView& current,
                    _In_ const AsicView& temporary,
                    _In_ sai_object_meta_key_t& meta_key,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t asic_process_event(
                    _In_ AsicView& current,
                    _In_ AsicView& temporary,
                    _In_ const swss::KeyOpFieldsValuesTuple& kco);

        private:


            /**
             * @brief Enable reference count logs.
             *
             * When set to true extra logging will be added for tracking
             * references.  This is useful for debugging, but for production
             * operations this will produce too much noise in logs, and we
             * still can replay scenario using recordings.
             */
            bool m_enableRefernceCountLogs;

            std::shared_ptr<sairedis::SaiInterface> m_vendorSai;

            std::shared_ptr<SaiSwitchInterface> m_switch;

            std::set<sai_object_id_t> m_initViewRemovedVids;

            std::shared_ptr<AsicView> m_current;

            std::shared_ptr<AsicView> m_temp;

            std::shared_ptr<NotificationHandler> m_handler;

            std::shared_ptr<BreakConfig> m_breakConfig;
    };
}
