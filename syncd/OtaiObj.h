#pragma once

extern "C" {
#include "otaimetadata.h"
}

#include "OtaiAttr.h"

#include <string>
#include <memory>
#include <unordered_map>

namespace syncd
{
    /**
     * @brief Represents OTAI object status during comparison
     */
    typedef enum _otai_object_status_t
    {
        /**
         * @brief Object was not processed at all
         *
         * This enum must be declared first.
         */
        OTAI_OBJECT_STATUS_NOT_PROCESSED = 0,

        /**
         * @brief Object was matched in previous view
         *
         * Previous VID was matched to temp VID since it's the same. Objects exists
         * in both previous and next view and have the save VID/RID values.
         *
         * However attributes of that object could be different and may be not
         * matched yet. This object still needs processing for attributes.
         *
         * Since only attributes can be updated then this object may not be removed
         * at all, it must be possible to update only attribute values.
         */
        OTAI_OBJECT_STATUS_MATCHED,

        /**
         * @brief Object was removed during child processing
         *
         * Only current view objects can be set to this status.
         */
        OTAI_OBJECT_STATUS_REMOVED,

        /**
         * @brief Object is in final stage
         *
         * This means object was matched/set/created and proper actions were
         * generated as diff data to be executed on ASIC.
         */
        OTAI_OBJECT_STATUS_FINAL,

    } otai_object_status_t;

    class ObjectStatus
    {
        private:

            ObjectStatus() = delete;
            ~ObjectStatus() = delete;

        public:

            static std::string otai_serialize_object_status(
                    _In_ otai_object_status_t os);
    };

    class OtaiObj
    {
        private:

            OtaiObj(const OtaiObj&) = delete;
            OtaiObj& operator=(const OtaiObj&) = delete;

        public:

            OtaiObj();

            virtual ~OtaiObj() = default;

        public:

            bool isOidObject() const;

            const std::unordered_map<otai_attr_id_t, std::shared_ptr<OtaiAttr>>& getAllAttributes() const;

            std::shared_ptr<const OtaiAttr> getOtaiAttr(
                    _In_ otai_attr_id_t id) const;

            std::shared_ptr<const OtaiAttr> tryGetOtaiAttr(
                    _In_ otai_attr_id_t id) const;

            /**
             * @brief Sets object status
             *
             * @param[in] objectStatus New object status
             */
            void setObjectStatus(
                    _In_ otai_object_status_t objectStatus);

            /**
             * @brief Gets current object status
             *
             * @return Current object status
             */
            otai_object_status_t getObjectStatus() const;

            /**
             * @brief Gets object type
             *
             * @return Object type
             */
            otai_object_type_t getObjectType() const;

            // TODO should be private, and we should have some friends from AsicView class

            void setAttr(
                    _In_ const std::shared_ptr<OtaiAttr> &attr);

            bool hasAttr(
                    _In_ otai_attr_id_t id) const;

            otai_object_id_t getVid() const;

            /*
             * NOTE: We need dependency tree if we want to remove objects which
             * have reference count not zero. Currently we just iterate on removed
             * objects as many times as their reference will get down to zero.
             */

        public: // TODO to private

            std::string m_str_object_type;
            std::string m_str_object_id;

            otai_object_meta_key_t m_meta_key;

            const otai_object_type_info_t *m_info;

            /**
             * @brief This will indicate whether object was created and it will
             * indicate that currently there is no RID for it.
             */
            bool m_createdObject;

        private:

            otai_object_status_t m_objectStatus;

            std::unordered_map<otai_attr_id_t, std::shared_ptr<OtaiAttr>> m_attrs;
    };
}
