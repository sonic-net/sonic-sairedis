#pragma once

#include <cstdint>

extern "C" {
#include "sai.h"
}
#include <unordered_map>
#include <vector>

namespace saivs
{
    /*
      There is circular dependency between SaiObjectDB and SwitchVpp. To remove it we need major surgery. For example,
      move SaiObjectDB to SwitchState. When we need to read an object, such as get_linked_object, get it from m_objectHash in SwitchState
    */
    class SwitchVpp;
    class SaiDBObject;

    typedef struct _SaiChildRelation
    {
        sai_object_type_t parent_type;
        // the attribute id in child object pointing to Parent object
        sai_attr_id_t child_link_attr;
        sai_attr_value_type_t child_link_attr_type;
    } SaiChildRelation;

    /*
      SaiObject is the base class for all objects in the SaiObjectDB.
      It represents a generic SAI object with a type, serialized ID, and associated attributes.
      Derived classes should implement the get_attr() function to retrieve specific attributes.
    */
    class SaiObject {
    public:
        SaiObject(SwitchVpp* switch_db, sai_object_type_t type, const std::string& id) : m_switch_db(switch_db), m_type(type), m_id(id){}

        virtual ~SaiObject() = default;

        virtual sai_status_t get_attr(_Inout_ sai_attribute_t &attr) const = 0;

        /**
         * @brief Retrieves the value of a specific attribute of the SAI object. If the attribute is not found, log an error and
         *        return SAI_STATUS_FAILURE.
         *
         * @param attr [out] A reference to the sai_attribute_t structure to store the attribute value.
         * @return sai_status_t The status of the attribute retrieval operation.
         */
        sai_status_t get_mandatory_attr(_Inout_ sai_attribute_t &attr) const;

        const std::string& get_id() const { return m_id; }

        sai_object_type_t get_type() const { return m_type; }

        std::shared_ptr<SaiDBObject> get_linked_object(_In_ sai_object_type_t linked_object_type, _In_ sai_attr_id_t link_attr_id) const;

        std::vector<std::shared_ptr<SaiDBObject>> get_linked_objects(_In_ sai_object_type_t linked_object_type, _In_ sai_attr_id_t link_attr_id) const;

        // Get the attribute name from the attribute ID
        const char* get_attr_name(_In_ sai_attr_id_t attr_id) const;
    protected:
        SwitchVpp* m_switch_db;
        sai_object_type_t m_type;
        std::string m_id;
    };

    /**
     * @brief The SaiCachedObject class represents a SAI object that has not been created in the SaiObjectDB. This is typically
     * used for objects that are being created through the SAI API.
     *
     * The cached object contains the type, serialized ID, and associated attributes.
     *
     * Derived classes should implement the get_attr() function to retrieve specific attributes.
     */
    class SaiCachedObject : public SaiObject {
    public:
        /**
         * @brief Constructs a SaiCachedObject with the specified parameters.
         *
         * @param switch_db A pointer to the SwitchVpp object.
         * @param type The type of the SAI object.
         * @param id The serialized ID of the SAI object.
         * @param attr_count The number of attributes associated with the SAI object.
         * @param attr_list An array of sai_attribute_t structures representing the attributes of the SAI object.
         */
        SaiCachedObject(SwitchVpp* switch_db, sai_object_type_t type, const std::string& id, uint32_t attr_count, const sai_attribute_t *attr_list) :
            SaiObject(switch_db, type, id), m_attr_count(attr_count), m_attr_list(attr_list) {}
        ~SaiCachedObject() = default;

        /**
         * @brief Retrieves the value of a specific attribute of the SAI object.
         *
         * This function should be implemented in derived classes to provide the specific attribute value.
         *
         * @param attr [out] A reference to the sai_attribute_t structure to store the attribute value.
         * @return sai_status_t The status of the attribute retrieval operation.
         */
        sai_status_t get_attr(_Inout_ sai_attribute_t &attr) const override;

    private:
        /**< The number of attributes associated with the SAI object. */
        uint32_t m_attr_count;
        /**< An array of sai_attribute_t structures representing the attributes of the SAI object. */
        const sai_attribute_t *m_attr_list;
    };

    /**
     * @brief The `SaiDBObject` class represents a SaiObject that is aready created through the SAI API.
     *
     * This class inherits from the `SaiObject` class and provides additional functionality for managing child objects.
     * It maintains a map of child objects based on their type and ID.
     */
    class SaiDBObject : public SaiObject {
    public:
        /**
         * @brief Constructs a `SaiDBObject` object.
         *
         * @param switch_db A pointer to the SwitchVpp object.
         * @param type The type of the SaiObject.
         * @param id The ID of the SaiObject.
         */
        SaiDBObject(SwitchVpp* switch_db, sai_object_type_t type, const std::string& id) : SaiObject(switch_db, type, id) {}

        /**
         * @brief Default destructor for the `SaiDBObject` class.
         */
        ~SaiDBObject() = default;

        /**
         * @brief Retrieves the attribute of the SaiObject.
         *
         * This function is overridden from the base class.
         *
         * @param attr The attribute to be retrieved.
         * @return The status of the operation.
         */
        sai_status_t get_attr(_Inout_ sai_attribute_t &attr) const override;

        /**
         * @brief Retrieves the child objects of the specified type.
         *
         * This function returns a pointer to the map of child objects of the specified type. The key of the map
         *  is the ID of the child object.
         *
         * @param child_type The type of the child objects.
         * @return A pointer to the map of child objects, or nullptr if no child objects of the specified type exist.
         */
        const std::unordered_map<std::string, std::shared_ptr<SaiObject>>* get_child_objs(sai_object_type_t child_type) const {
            SWSS_LOG_ENTER();
            auto it = m_child_map.find(child_type);
            if (it != m_child_map.end()) {
                return &(it->second);
            }
            return nullptr;
        }

        /**
         * @brief Adds a child object to the `SaiDBObject`.
         *
         * This function adds the specified child object to the map of child objects.
         *
         * @param entry The child object to be added.
         */
        void add_child(const std::shared_ptr<SaiObject> entry) {
            SWSS_LOG_ENTER();
            if (entry) {
                m_child_map[entry->get_type()][entry->get_id()] = entry;
            }
        }

        /**
         * @brief Removes a child object from the `SaiDBObject`.
         *
         * This function removes the child object with the specified type and ID from the map of child objects.
         *
         * @param child_type The type of the child object.
         * @param id The ID of the child object.
         */
        void remove_child(sai_object_type_t child_type, const std::string& id) {
            SWSS_LOG_ENTER();
            auto child_map_it = m_child_map.find(child_type);
            if (child_map_it != m_child_map.end()) {
                auto child_map_per_type_it = child_map_it->second.find(id);
                if (child_map_per_type_it != child_map_it->second.end()) {
                    child_map_it->second.erase(child_map_per_type_it);
                }
            }
        }

    private:
        /**
         * @brief A map of child objects based on their type and ID.
         * child-type -> child-oid -> child-object
         */
        std::unordered_map<sai_object_type_t, std::unordered_map<std::string, std::shared_ptr<SaiObject>>> m_child_map;
    };

    /**
     * SaiModDBObject is used to represent a SAI object that has been modified. It has the list of modified attributes of
     * the SAI object passed in through set request and backed by original SAI object in DB. get_attribute will first read
     * from the modified attribute list and if not found, it will read the object in database.
     */
    class SaiModDBObject : public SaiObject {
    public:
        SaiModDBObject(SwitchVpp* switch_db, sai_object_type_t type, const std::string& id,
                       uint32_t attr_count, const sai_attribute_t *attr_list);

        ~SaiModDBObject() = default;

        sai_status_t get_attr(_Inout_ sai_attribute_t &attr) const override;

        std::shared_ptr<SaiDBObject> get_db_obj() const {
            SWSS_LOG_ENTER();
            return m_sai_db_obj;
        }

        const std::unordered_map<std::string, std::shared_ptr<SaiObject>>* get_child_objs(sai_object_type_t child_type) const {
            SWSS_LOG_ENTER();
            if (m_sai_db_obj) {
                return m_sai_db_obj->get_child_objs(child_type);
            } else {
                return nullptr;
            }
        }
    private:
        /**< The number of modified attributes associated with the SAI object. */
        uint32_t m_attr_count;
        /**< An array of sai_attribute_t structures representing the modified attributes of the SAI object. */
        const sai_attribute_t *m_attr_list;
        std::shared_ptr<SaiDBObject> m_sai_db_obj;
    };

    /**
     * @brief The SaiObjectDB class represents a database for managed SAI objects. Only the SAI objects we are interested are
     * added to SaiObjectDB. See sai_child_relation_defs in SaiObjectDB.cpp for such objects.
     *
     * This class provides methods for adding, removing, and retrieving SAI objects from the database.
     * Each SAI object is associated with a unique ID and belongs to a specific switch.
     *
     * @note This class assumes that the switch database (`SwitchVpp`) is already initialized.
     */
    class SaiObjectDB {
    public:
        /**
         * @brief Constructs a SaiObjectDB object.
         *
         * @param switch_db A pointer to the switch database (`SwitchVpp`).
         */
        SaiObjectDB(SwitchVpp* switch_db) : m_switch_db(switch_db) {};

        /**
         * @brief Destructs the SaiObjectDB object.
         */
        ~SaiObjectDB() = default;

        /**
         * @brief Adds a new SAI object to the database or update an existing one if it already exists,
         *      which includes removing the child from the current parent and adding it to the new parent.
         *
         * @param object_type The type of the SAI object.
         * @param id The ID of the SAI object.
         * @param attr_count The number of attributes in the attribute list.
         * @param attr_list An array of attributes for the SAI object.
         * @param is_create A flag indicating whether the object is being created.
         * @return The status of the operation.
         */
        sai_status_t create_or_update(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::string& id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list,
                    _In_ bool is_create);

        /**
         * @brief Removes an existing SAI object from the database.
         *
         * @param object_type The type of the SAI object.
         * @param id The ID of the SAI object.
         * @return The status of the operation.
         */
        sai_status_t remove(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::string& id);

        /**
         * @brief Retrieves a shared pointer to an existing SAI object from the database.
         *
         * @param object_type The type of the SAI object.
         * @param id The ID of the SAI object.
         * @return A shared pointer to the SAI object, or nullptr if the object is not found.
         */
        std::shared_ptr<SaiDBObject> get(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::string& id);

    private:
        // Pointer to the switch database
        SwitchVpp* m_switch_db;
        /**
         * @brief A map of SAI parent objects based on their type and ID.
         *  parent-type -> parent-oid -> parent-object
         * a parent object has a map of child objects based on their type and ID.
         */
        std::unordered_map<sai_object_type_t, std::unordered_map<std::string, std::shared_ptr<SaiDBObject>>> m_sai_parent_objs;
        /**
         * @brief Removes a child object from its parent in the SaiObjectDB for the give child-parent relationship definition.
         *
         * This function removes a child object from its parent in the SaiObjectDB. It takes the object type, the ID of the child object,
         * and the child-parent relationship definition as input parameters.
         * The function retrieves the parent object IDs using the get_parent_oids() function and iterates over each parent ID.
         * For each parent ID, it checks if the parent object exists in the SaiObjectDB. If the parent object is not found, a warning
         * message is logged and the function returns SAI_STATUS_SUCCESS.
         * If the parent object is found, the function removes the child object from the parent object using the remove_child()
         * function and logs a debug message.
         *
         * @param object_type The type of the child object.
         * @param id The ID of the child object.
         * @param child_def The child-parent relationship definition.
         */
        void remove_child_from_parent(_In_ sai_object_type_t object_type, _In_ const std::string& id, _In_ const SaiChildRelation& child_def);
    };
}
