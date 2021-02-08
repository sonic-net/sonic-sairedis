#pragma once

extern "C" {
#include "sai.h"
#include "saimetadata.h"
}

#include "swss/json.hpp"
#include "swss/table.h"

#include "syncd/AsicView.h"

#include <map>
#include <set>

namespace saiasiccmp
{
    class View
    {
        using json = nlohmann::json;

        public:

            // TODO support multiple switches

            View(
                    _In_ const std::string& filename);

        public:

                void translateViewVids(
                        _In_ uint64_t otherMaxObjectIndex);

        private:

                void loadVidRidMaps(
                        _In_ const json& j);

                void loadAsicView(
                        _In_ const json& j);

                void loadColdVids(
                        _In_ const json& j);

                void loadHidden(
                        _In_ const json& j);

                void translateVidRidMaps();

                sai_object_id_t translateOldVidToNewVid(
                        _In_ sai_object_id_t oldVid) const;

                void translateMetaKeyVids(
                        _Inout_ sai_object_meta_key_t& mk) const;

                void translateAttrVids(
                        _In_ const sai_attr_metadata_t* meta,
                        _Inout_ sai_attribute_t& attr);

                void translateAsicView();

                void translateColdVids();

        public:

                uint64_t m_maxObjectIndex;
                uint64_t m_otherMaxObjectIndex;

                bool m_translateVids;

                swss::TableDump m_dump;

                sai_object_id_t m_switchVid;
                sai_object_id_t m_switchRid;

                std::map<sai_object_id_t, sai_object_id_t> m_vid2rid;
                std::map<sai_object_id_t, sai_object_id_t> m_rid2vid;

                std::map<sai_object_id_t, sai_object_id_t> m_oldVid2NewVid;

                std::map<sai_object_type_t, std::set<sai_object_id_t>> m_oidTypeMap;
                std::map<sai_object_type_t, std::set<std::string>> m_objTypeStrMap;

                std::shared_ptr<syncd::AsicView> m_asicView;

                std::map<sai_object_id_t, sai_object_type_t> m_coldVids;
                std::map<std::string, sai_object_id_t> m_hidden;
    };
}
