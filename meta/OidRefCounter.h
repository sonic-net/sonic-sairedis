#pragma once

extern "C" {
#include "sai.h"
}

#include <unordered_map>
#include <vector>

namespace saimeta
{
    class OidRefCounter
    {
        public:

            OidRefCounter() = default;

            virtual ~OidRefCounter() = default;

        public:

            /**
             * @brief Clears entire hash.
             */
            void clear();

            /**
             * @brief Check if object reference exists.
             *
             * Return true if reference exists, even if zero.
             */
            bool objectReferenceExists(
                    _In_ sai_object_id_t oid) const;

            /**
             * @brief Increment reference count on object.
             *
             * Throws if object was not previously inserted.
             */
            void objectReferenceIncrement(
                    _In_ sai_object_id_t oid);

            void objectReferenceIncrement(
                    _In_ const sai_object_list_t& list);

            /**
             * @brief Decrement reference count on object.
             *
             * Throws if after decrement reference count becomes negative or
             * object was not previously inserted.
             */
            void objectReferenceDecrement(
                    _In_ sai_object_id_t oid);

            void objectReferenceDecrement(
                    _In_ const sai_object_list_t& list);

            /**
             * @brief Insert object reference.
             *
             * Throws if object reference already exists.
             */
            void objectReferenceInsert(
                    _In_ sai_object_id_t oid);

            /**
             * @brief Remove object reference.
             *
             * Throws if object don't exists or object is in use.
             */
            void objectReferenceRemove(
                    _In_ sai_object_id_t oid);

            /**
             * @brief Clear object reference.
             *
             * Throws if object don't exists. Removes reference even if there
             * is count on it.
             */
            void objectReferenceClear(
                    _In_ sai_object_id_t oid);

            /**
             * @brief Get reference count on given object.
             *
             * Throws if object don't exists.
             */
            int32_t getObjectReferenceCount(
                    _In_ sai_object_id_t oid) const;

            /**
             * @brief Check if object reference is in use.
             *
             * Throws if object don't exists.
             */
            bool isObjectInUse(
                    _In_ sai_object_id_t oid) const;

            /**
             * @brief Get copy of entire reference hash.
             */
            std::unordered_map<sai_object_id_t, int32_t> getAllReferences() const;

            std::vector<sai_object_id_t> getAllOids() const;

        private:

            /**
             * @brief Object id to reference count hash.
             *
             * Object may exist in the hash, and have reference count 0, which
             * means is not not used anywhere and can be safely removed.
             */
            std::unordered_map<sai_object_id_t, int32_t> m_hash;
    };
}
