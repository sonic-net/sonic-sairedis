#pragma once

extern "C" {
#include "saimetadata.h"
}

#include "SaiObj.h"
#include "SaiAttr.h"
#include "AsicOperation.h"

#include "swss/table.h"

namespace syncd
{
    /**
     * @brief Class represents ASIC view
     */
    class AsicView
    {
        public:

            typedef std::unordered_map<sai_object_id_t, sai_object_id_t> ObjectIdMap;
            typedef std::map<std::string, std::shared_ptr<SaiObj>> StrObjectIdToSaiObjectHash;
            typedef std::map<sai_object_id_t, std::shared_ptr<SaiObj>> ObjectIdToSaiObjectHash;

        private:

            /*
             * Copy constructor and assignment operator are marked as private to
             * avoid copy of data of entire view. This is not needed for now.
             */

            AsicView(const AsicView&) = delete;
            AsicView& operator=(const SaiAttr&) = delete;

        public:

            AsicView();

            AsicView(
                    _In_ const swss::TableDump &dump);

            virtual ~AsicView();

        public:

            /**
             * @brief Populates ASIC view from REDIS table dump
             *
             * @param[in] dump Redis table dump
             *
             * NOTE: Could be static method that returns AsicView object.
             */
            void fromDump(
                    _In_ const swss::TableDump &dump);

            void checkObjectsStatus() const;

            sai_object_id_t getSwitchVid() const;

        private:

            /**
             * @brief Release existing VID links (references) based on given attribute.
             *
             * @param[in] attr Attribute which will be used to obtain oids.
             */
            void releaseExisgingLinks(
                    _In_ const std::shared_ptr<const SaiAttr> &attr);

            /**
             * @brief Release existing VID links (references) based on given object.
             *
             * All OID attributes will be scanned and released.
             *
             * @param[in] obj Object which will be used to obtain attributes and oids
             */
            void releaseExisgingLinks(
                    _In_ const std::shared_ptr<const SaiObj> &obj);

            /**
             * @brief Release VID reference.
             *
             * If SET operation was performed on attribute, and attribute was OID
             * attribute, then we need to release previous reference to that VID,
             * and bind new reference to next OID if present.
             *
             * @param[in] vid Virtual ID to be released.
             */
            void releaseVidReference(
                    _In_ sai_object_id_t vid);

            /**
             * @brief Bind new links (references) based on attribute
             *
             * @param[in] attr Attribute to obtain oids to bind references
             */
            void bindNewLinks(
                    _In_ const std::shared_ptr<const SaiAttr> &attr);

            /**
             * @brief Bind existing VID links (references) based on given object.
             *
             * All OID attributes will be scanned and bound.
             *
             * @param[in] obj Object which will be used to obtain attributes and oids
             */
            void bindNewLinks(
                    _In_ const std::shared_ptr<const SaiObj> &obj);

            /**
             * @brief Bind new VID reference
             *
             * If attribute is OID attribute then we need to increase reference
             * count on that VID to mark it that it is in use.
             *
             * @param[in] vid Virtual ID reference to be bind
             */
            void bindNewVidReference(
                    _In_ sai_object_id_t vid);

        public:

            /**
             * @brief Gets VID reference count.
             *
             * @param vid Virtual ID to obtain reference count.
             *
             * @return Reference count or -1 if VID was not found.
             */
            int getVidReferenceCount(
                    _In_ sai_object_id_t vid) const;

            /**
             * @brief Insert new VID reference.
             *
             * Inserts new reference to be tracked. This also make sure that
             * reference doesn't exist yet, as a sanity check if same reference
             * would be inserted twice.
             *
             * @param[in] vid Virtual ID reference to be inserted.
             */
            void insertNewVidReference(
                    _In_ sai_object_id_t vid);

        public:

            /**
             * @brief Gets objects by object type.
             *
             * @param object_type Object type to be used as filter.
             *
             * @return List of objects with requested object type.
             * Order on list is random.
             */
            std::vector<std::shared_ptr<SaiObj>> getObjectsByObjectType(
                    _In_ sai_object_type_t object_type) const;

            /**
             * @brief Gets not processed objects by object type.
             *
             * Call to this method can be expensive, since every time we iterate
             * entire list. This list can contain even 10k elements if view will be
             * very large.
             *
             * @param object_type Object type to be used as filter.
             *
             * @return List of objects with requested object type and marked
             * as not processed. Order on list is random.
             */
            std::vector<std::shared_ptr<SaiObj>> getNotProcessedObjectsByObjectType(
                    _In_ sai_object_type_t object_type) const;

            /**
             * @brief Gets all not processed objects
             *
             * @return List of all not processed objects. Order on list is random.
             */
            std::vector<std::shared_ptr<SaiObj>> getAllNotProcessedObjects() const;

            /**
             * @brief Create dummy existing object
             *
             * Function creates dummy object, which is used to indicate that
             * this OID object exist in current view. This is used for existing
             * objects, like CPU port, default trap group.
             *
             * @param[in] rid Real ID
             * @param[in] vid Virtual ID
             */
            std::shared_ptr<SaiObj> createDummyExistingObject(
                    _In_ sai_object_id_t rid,
                    _In_ sai_object_id_t vid);

            /**
             * @brief Generate ASIC set operation on current existing object.
             *
             * NOTE: In long run, this is serialize, and then we call deserialize
             * to execute them on actual ASIC, maybe this is not necessary
             * and could be optimized later.
             *
             * TODO: Set on object id should do release of links (currently done
             * outside) and modify dependency tree.
             *
             * @param currentObj Current object.
             * @param attr Attribute to be set on current object.
             */
            void asicSetAttribute(
                    _In_ const std::shared_ptr<SaiObj> &currentObj,
                    _In_ const std::shared_ptr<SaiAttr> &attr);

            /**
             * @brief Generate ASIC create operation for current object.
             *
             * NOTE: In long run, this is serialize, and then we call
             * deserialize to execute them on actual asic, maybe this is not
             * necessary and could be optimized later.
             *
             * TODO: Create on object id attributes should bind references to
             * used VIDs of of links (currently done outside) and modify
             * dependency tree.
             *
             * @param currentObject Current object to be created.
             */
            void asicCreateObject(
                    _In_ const std::shared_ptr<SaiObj> &currentObj);

            /**
             * @brief Generate ASIC remove operation for current existing object.
             *
             * @param currentObj Current existing object to be removed.
             */
            void asicRemoveObject(
                    _In_ const std::shared_ptr<SaiObj> &currentObj);

            std::vector<AsicOperation> asicGetWithOptimizedRemoveOperations() const;

            std::vector<AsicOperation> asicGetOperations() const;

            size_t asicGetOperationsCount() const;

            bool hasRid(
                    _In_ sai_object_id_t rid) const;

            bool hasVid(
                    _In_ sai_object_id_t vid) const;

            void dumpRef(const std::string & asicName);

            void dumpVidToAsicOperatioId() const;

        private:

            void populateAttributes(
                    _In_ std::shared_ptr<SaiObj> &obj,
                    _In_ const swss::TableMap &map);

            /**
             * @brief Update non object id VID reference count by specified value.
             *
             * Method will iterate via all OID struct members in non object id and
             * update reference count by specified value.
             *
             * @param currentObj Current object to be processed.
             * @param value Value by which reference will be updated. Can be negative.
             */
            void updateNonObjectIdVidReferenceCountByValue(
                    _In_ const std::shared_ptr<SaiObj> &currentObj,
                    _In_ int value);

        public:

            // TODO convert to something like nonObjectIdMap

            StrObjectIdToSaiObjectHash m_soFdbs;
            StrObjectIdToSaiObjectHash m_soNeighbors;
            StrObjectIdToSaiObjectHash m_soRoutes;
            StrObjectIdToSaiObjectHash m_soNatEntries;
            StrObjectIdToSaiObjectHash m_soInsegs;
            StrObjectIdToSaiObjectHash m_soOids;
            StrObjectIdToSaiObjectHash m_soAll;

            std::unordered_map<std::string,std::vector<std::string>> m_routesByPrefix;

            ObjectIdToSaiObjectHash m_oOids;

            std::unordered_map<sai_object_id_t, sai_object_id_t> m_preMatchMap;

            /*
             * On temp view this needs to be used for actual NEW rids created and
             * then reused with rid mapping to create new rid/vid map.
             */

            ObjectIdMap m_ridToVid;
            ObjectIdMap m_vidToRid;
            ObjectIdMap m_removedVidToRid;

            std::map<sai_object_type_t, std::unordered_map<std::string,std::string>> m_nonObjectIdMap;

            sai_object_id_t m_defaultTrapGroupRid;

        private:

            bool m_enableRefernceCountLogs;

            /**
             * @brief Virtual ID reference map.
             *
             * VID is key, reference count is value.
             */
            std::map<sai_object_id_t, int> m_vidReference;

            /**
             * @brief Asic operation ID.
             *
             * Since asic resources are limited like number of routes or buffer
             * pools, so if we don't match object, at first we created new object
             * and then remove previous one. This scenario may not be possible in
             * case of limited resources. Advantage here is that this approach is
             * making sure that asic data plane disruption will be minimal. But we
             * need to switch to remove object first and then create new one. This
             * will make sure that we will be able to remove first and then create,
             * but in this case we can have some asic data plane disruption.
             *
             * This asic operation id will be used to figure out what operation
             * reduced object reference to zero, so we could move remove operation
             * right after this operation instead of the executing all remove
             * actions after all set/create.
             *
             */
            int m_asicOperationId;

            /**
             * @brief VID to asic operation id map.
             *
             * Map where key is VID that points to last asic operation id that
             * decreased reference on that VID to zero. This mean that if object
             * with that VID will be removed, we can move remove operation right
             * after asic operation id pointed by this VID.
             */
            std::map<sai_object_id_t, int> m_vidToAsicOperationId;

            /**
             * @brief ASIC operation list.
             *
             * List contains ASIC operation generated in the same way as SAIREDIS.
             *
             * KeyOpFieldsValuesTuple is shared to prevent expensive copy when
             * adding to vector.
             */
            std::vector<AsicOperation> m_asicOperations;

            std::vector<AsicOperation> m_asicRemoveOperationsNonObjectId;

            std::map<sai_object_type_t, StrObjectIdToSaiObjectHash> m_sotAll;
    };
}
