#pragma once

extern "C" {
#include "otaimetadata.h"
}

#include "swss/logger.h"
#include "swss/table.h"

#include <string>

namespace otaimeta
{
    class Globals
    {
        private:

            Globals() = delete;
            ~Globals() = delete;

        public:

            static std::string getAttrInfo(
                    _In_ const otai_attr_metadata_t& md);

            /**
             * @brief Get hardware info.
             *
             * Get hardware info from attribute list, typically passed to
             * create_switch api and convert it from s8list to std::string.
             * Object type is assumed to be OTAI_OBJECT_TYPE_SWITCH.
             *
             * @return Hardware info converted to string.
             */
            static std::string getHardwareInfo(
                    _In_ uint32_t attrCount,
                    _In_ const otai_attribute_t *attrList);

            static std::string joinFieldValues(
                    _In_ const std::vector<swss::FieldValueTuple>& values);
    };
}

#define META_LOG_WARN(   md, format, ...)   SWSS_LOG_WARN   ("%s " format, otaimeta::Globals::getAttrInfo(md).c_str(), ##__VA_ARGS__)
#define META_LOG_ERROR(  md, format, ...)   SWSS_LOG_ERROR  ("%s " format, otaimeta::Globals::getAttrInfo(md).c_str(), ##__VA_ARGS__)
#define META_LOG_DEBUG(  md, format, ...)   SWSS_LOG_DEBUG  ("%s " format, otaimeta::Globals::getAttrInfo(md).c_str(), ##__VA_ARGS__)
#define META_LOG_NOTICE( md, format, ...)   SWSS_LOG_NOTICE ("%s " format, otaimeta::Globals::getAttrInfo(md).c_str(), ##__VA_ARGS__)
#define META_LOG_INFO(   md, format, ...)   SWSS_LOG_INFO   ("%s " format, otaimeta::Globals::getAttrInfo(md).c_str(), ##__VA_ARGS__)
#define META_LOG_THROW(  md, format, ...)   SWSS_LOG_THROW  ("%s " format, otaimeta::Globals::getAttrInfo(md).c_str(), ##__VA_ARGS__)
