#pragma once

extern "C" {
#include "saimetadata.h"
}

#include "swss/logger.h"
#include "swss/table.h"

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

            static std::string joinFieldValues(
                    _In_ const std::vector<swss::FieldValueTuple>& values);

            /**
             * @brief Get log priority for a failed API call.
             *
             * An FDB entry create that returns SAI_STATUS_ITEM_ALREADY_EXISTS
             * is an expected race: the switch learned the MAC while the create
             * was in flight. The caller handles that status, so it is logged
             * at NOTICE. Every other failure is logged at ERROR. A double
             * create stops earlier, in the caller's meta, which logs it at
             * ERROR because the entry it holds came from an API create.
             *
             * @return Log priority for the failure.
             */
            static swss::Logger::Priority getApiFailureLogPriority(
                    _In_ sai_common_api_t api,
                    _In_ sai_object_type_t objectType,
                    _In_ sai_status_t status);
    };
}

#define META_LOG_WARN(   md, format, ...)   SWSS_LOG_WARN   ("%s " format, saimeta::Globals::getAttrInfo(md).c_str(), ##__VA_ARGS__)
#define META_LOG_ERROR(  md, format, ...)   SWSS_LOG_ERROR  ("%s " format, saimeta::Globals::getAttrInfo(md).c_str(), ##__VA_ARGS__)
#define META_LOG_DEBUG(  md, format, ...)   SWSS_LOG_DEBUG  ("%s " format, saimeta::Globals::getAttrInfo(md).c_str(), ##__VA_ARGS__)
#define META_LOG_NOTICE( md, format, ...)   SWSS_LOG_NOTICE ("%s " format, saimeta::Globals::getAttrInfo(md).c_str(), ##__VA_ARGS__)
#define META_LOG_INFO(   md, format, ...)   SWSS_LOG_INFO   ("%s " format, saimeta::Globals::getAttrInfo(md).c_str(), ##__VA_ARGS__)
#define META_LOG_THROW(  md, format, ...)   SWSS_LOG_THROW  ("%s " format, saimeta::Globals::getAttrInfo(md).c_str(), ##__VA_ARGS__)

#define META_LOG_PRIO(prio, format, ...)   swss::Logger::getInstance().write(prio, ":- %s: " format, __FUNCTION__, ##__VA_ARGS__)
