
#define MUTEX() std::lock_guard<std::recursive_mutex> \
    lock(g_recursive_mutex)

// object id

#define VS_CREATE(OBJECT_TYPE,object_type)              \
    sai_status_t vs_create_ ## object_type(             \
            _Out_ sai_object_id_t *object_type ##_id,   \
            _In_ sai_object_id_t switch_id,             \
            _In_ uint32_t attr_count,                   \
            _In_ const sai_attribute_t *attr_list)      \
    {                                                   \
        MUTEX();                                        \
        SWSS_LOG_ENTER();                               \
        return meta_sai_create_oid(                     \
                SAI_OBJECT_TYPE_ ## OBJECT_TYPE,        \
                object_type ## _id,                     \
                switch_id,                              \
                attr_count,                             \
                attr_list,                              \
                &vs_generic_create);                    \
    }

#define VS_REMOVE(OBJECT_TYPE,object_type)              \
    sai_status_t vs_remove_ ## object_type(             \
            _In_ sai_object_id_t object_type ## _id)    \
    {                                                   \
        MUTEX();                                        \
        SWSS_LOG_ENTER();                               \
        return meta_sai_remove_oid(                     \
                SAI_OBJECT_TYPE_ ## OBJECT_TYPE,        \
                object_type ## _id,                     \
                &vs_generic_remove);                    \
    }

#define VS_SET(OBJECT_TYPE,object_type)                 \
    sai_status_t vs_set_ ## object_type ## _attribute(  \
            _In_ sai_object_id_t object_type ## _id,    \
            _In_ const sai_attribute_t *attr)           \
    {                                                   \
        MUTEX();                                        \
        SWSS_LOG_ENTER();                               \
        return meta_sai_set_oid(                        \
                SAI_OBJECT_TYPE_ ## OBJECT_TYPE,        \
                object_type ## _id,                     \
                attr,                                   \
                &vs_generic_set);                       \
    }

#define VS_GET(OBJECT_TYPE,object_type)                 \
    sai_status_t vs_get_ ## object_type ## _attribute(  \
            _In_ sai_object_id_t object_type ## _id,    \
            _In_ uint32_t attr_count,                   \
            _Inout_ sai_attribute_t *attr_list)         \
    {                                                   \
        MUTEX();                                        \
        SWSS_LOG_ENTER();                               \
        return meta_sai_get_oid(                        \
                SAI_OBJECT_TYPE_ ## OBJECT_TYPE,        \
                object_type ## _id,                     \
                attr_count,                             \
                attr_list,                              \
                &vs_generic_get);                       \
    }

#define VS_GENERIC_QUAD(OT,ot)  \
    VS_CREATE(OT,ot);           \
    VS_REMOVE(OT,ot);           \
    VS_SET(OT,ot);              \
    VS_GET(OT,ot);

// struct object id

#define VS_CREATE_ENTRY(OBJECT_TYPE,object_type)                \
    sai_status_t vs_create_ ## object_type(                     \
            _In_ const sai_ ## object_type ##_t *object_type,   \
            _In_ uint32_t attr_count,                           \
            _In_ const sai_attribute_t *attr_list)              \
    {                                                           \
        MUTEX();                                                \
        SWSS_LOG_ENTER();                                       \
        return meta_sai_create_ ## object_type(                 \
                object_type,                                    \
                attr_count,                                     \
                attr_list,                                      \
                &vs_generic_create_ ## object_type);            \
    }

#define VS_REMOVE_ENTRY(OBJECT_TYPE,object_type)                \
    sai_status_t vs_remove_ ## object_type(                     \
            _In_ const sai_ ## object_type ## _t *object_type)  \
    {                                                           \
        MUTEX();                                                \
        SWSS_LOG_ENTER();                                       \
        return meta_sai_remove_ ## object_type(                 \
                object_type,                                    \
                &vs_generic_remove_ ## object_type);            \
    }

#define VS_SET_ENTRY(OBJECT_TYPE,object_type)                   \
    sai_status_t vs_set_ ## object_type ## _attribute(          \
            _In_ const sai_ ## object_type ## _t *object_type,  \
            _In_ const sai_attribute_t *attr)                   \
    {                                                           \
        MUTEX();                                                \
        SWSS_LOG_ENTER();                                       \
        return meta_sai_set_ ## object_type(                    \
                object_type,                                    \
                attr,                                           \
                &vs_generic_set_ ## object_type);               \
    }

#define VS_GET_ENTRY(OBJECT_TYPE,object_type)                   \
    sai_status_t vs_get_ ## object_type ## _attribute(          \
            _In_ const sai_ ## object_type ## _t *object_type,  \
            _In_ uint32_t attr_count,                           \
            _Inout_ sai_attribute_t *attr_list)                 \
    {                                                           \
        MUTEX();                                                \
        SWSS_LOG_ENTER();                                       \
        return meta_sai_get_ ## object_type(                    \
                object_type,                                    \
                attr_count,                                     \
                attr_list,                                      \
                &vs_generic_get_ ## object_type);               \
    }

#define VS_GENERIC_QUAD_ENTRY(OT,ot)  \
    VS_CREATE_ENTRY(OT,ot);           \
    VS_REMOVE_ENTRY(OT,ot);           \
    VS_SET_ENTRY(OT,ot);              \
    VS_GET_ENTRY(OT,ot);

// common api

#define VS_GENERIC_QUAD_API(ot)     \
    vs_create_ ## ot,               \
    vs_remove_ ## ot,               \
    vs_set_ ## ot ##_attribute,     \
    vs_get_ ## ot ##_attribute,
