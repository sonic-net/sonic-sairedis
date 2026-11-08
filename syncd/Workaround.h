#pragma once

extern "C" {
#include "saimetadata.h"
}

#include "AsicView.h"

#include <vector>

namespace syncd
{
    class Workaround
    {
        private:

            Workaround() = delete;
            ~Workaround() = delete;

        public:

            /**
             * @brief Determines whether attribute is "workaround" attribute for SET API.
             *
             * Some attributes are not supported on SET API on different platforms.
             * For example SAI_SWITCH_ATTR_SRC_MAC_ADDRESS.
             *
             * @param[in] objectType Object type.
             * @param[in] attrId Attribute Id.
             * @param[in] status Status from SET API.
             *
             * @return True if error from SET API can be ignored, false otherwise.
             */
            static bool isSetAttributeWorkaround(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_attr_id_t attrId,
                    _In_ sai_status_t status);

            /**
             * @brief Determines whether attribute is "workaround" attribute for SET API
             * during apply view only.
             *
             * Some workarounds should only be applied during apply view i.e.
             * warm-boot. For workaround that should apply at all times see
             * isSetAttributeWorkaround.
             *
             * @param[in] currentView Current ASIC view that the op will apply to.
             * @param[in] objectId Virtual Object ID.
             * @param[in] attrId Attribute Id.
             * @param[in] status Status from SET API.
             *
             * @return True if error from SET API can be ignored, false otherwise.
             */
            static bool isSetAttributeWorkaroundDuringApplyView(
                    _In_ const AsicView& currentView,
                    _In_ sai_object_id_t objectId,
                    _In_ sai_attr_id_t attrId,
                    _In_ sai_status_t status);

            /**
             * @brief Convert port status notification from older version.
             *
             * This method will convert port status notification from SAI
             * v1.14.0 to version v1.15.0, since from that version a new
             * structure field was added, and there is possibility that syncd
             * compiled against headers v1.15.0 will receive notification from
             * sai library v1.14.0 or older, and since that change is not
             * backward compatible, it can cause invalid memory read or garbage
             * memory read. To prevent that, we will convert structures.
             *
             * Similar thing can happen if older syncd will receive
             * notification from newer version of sai library.
             */
            static std::vector<sai_port_oper_status_notification_t> convertPortOperStatusNotification(
                    _In_ const uint32_t count,
                    _In_ const sai_port_oper_status_notification_t* data,
                    _In_ sai_api_version_t version);
        public:

            /**
             * @brief Port operational status notification as in SAI version v1.14.0
             *
             * This will imitate structure size needed for notification conversion.
             */
            typedef struct _sai_port_oper_status_notification_v1_14_0_t
            {
                sai_object_id_t port_id;

                sai_port_oper_status_t port_state;

            } sai_port_oper_status_notification_v1_14_0_t;
    };
}
