#pragma once

extern "C" {
#include "saimetadata.h"
}

#include "swss/logger.h"

#include <string>

namespace saimeta
{
    class Globals
    {
        private:

            Globals() = delete;
            ~Globals() = delete;

        public:

            static std::string getAttrInfo(
                    _In_ const sai_attr_metadata_t& md);

            /**
             * @brief Get hardware info.
             *
             * Get hardware info from attribute list, typically passed to
             * create_switch api and convert it from s8list to std::string.
             * Object type is assumed to be SAI_OBJECT_TYPE_SWITCH.
             *
             * @return Hardware info converted to string.
             */
            static std::string getHardwareInfo(
                    _In_ uint32_t attrCount,
                    _In_ const sai_attribute_t *attrList);
    };
}

#define META_LOG_WARN(   md, format, ...)   SWSS_LOG_WARN   ("%s " format, saimeta::Globals::getAttrInfo(md).c_str(), ##__VA_ARGS__)
#define META_LOG_ERROR(  md, format, ...)   SWSS_LOG_ERROR  ("%s " format, saimeta::Globals::getAttrInfo(md).c_str(), ##__VA_ARGS__)
#define META_LOG_DEBUG(  md, format, ...)   SWSS_LOG_DEBUG  ("%s " format, saimeta::Globals::getAttrInfo(md).c_str(), ##__VA_ARGS__)
#define META_LOG_NOTICE( md, format, ...)   SWSS_LOG_NOTICE ("%s " format, saimeta::Globals::getAttrInfo(md).c_str(), ##__VA_ARGS__)
#define META_LOG_INFO(   md, format, ...)   SWSS_LOG_INFO   ("%s " format, saimeta::Globals::getAttrInfo(md).c_str(), ##__VA_ARGS__)
#define META_LOG_THROW(  md, format, ...)   SWSS_LOG_THROW  ("%s " format, saimeta::Globals::getAttrInfo(md).c_str(), ##__VA_ARGS__)
