
#define MUTEX() std::lock_guard<std::mutex> lock(g_apimutex)

// object id

#define REDIS_CREATE(OBJECT_TYPE,object_type)                       \
    sai_status_t redis_create_ ## object_type(                      \
            _Out_ sai_object_id_t *object_type ##_id,               \
            _In_ sai_object_id_t switch_id,                         \
            _In_ uint32_t attr_count,                               \
            _In_ const sai_attribute_t *attr_list)                  \
    {                                                               \
        MUTEX();                                                    \
        SWSS_LOG_ENTER();                                           \
        return meta_sai_create_oid(                                 \
                (sai_object_type_t)SAI_OBJECT_TYPE_ ## OBJECT_TYPE, \
                object_type ## _id,                                 \
                switch_id,                                          \
                attr_count,                                         \
                attr_list,                                          \
                &redis_generic_create);                             \
    }

#define REDIS_REMOVE(OBJECT_TYPE,object_type)                       \
    sai_status_t redis_remove_ ## object_type(                      \
            _In_ sai_object_id_t object_type ## _id)                \
    {                                                               \
        MUTEX();                                                    \
        SWSS_LOG_ENTER();                                           \
        return meta_sai_remove_oid(                                 \
                (sai_object_type_t)SAI_OBJECT_TYPE_ ## OBJECT_TYPE, \
                object_type ## _id,                                 \
                &redis_generic_remove);                             \
    }

#define REDIS_SET(OBJECT_TYPE,object_type)                          \
    sai_status_t redis_set_ ##object_type ## _attribute(            \
            _In_ sai_object_id_t object_type ## _id,                \
            _In_ const sai_attribute_t *attr)                       \
    {                                                               \
        MUTEX();                                                    \
        SWSS_LOG_ENTER();                                           \
        return meta_sai_set_oid(                                    \
                (sai_object_type_t)SAI_OBJECT_TYPE_ ## OBJECT_TYPE, \
                object_type ## _id,                                 \
                attr,                                               \
                &redis_generic_set);                                \
    }

#define REDIS_GET(OBJECT_TYPE,object_type)                          \
    sai_status_t redis_get_ ##object_type ## _attribute(            \
            _In_ sai_object_id_t object_type ## _id,                \
            _In_ uint32_t attr_count,                               \
            _Inout_ sai_attribute_t *attr_list)                     \
    {                                                               \
        MUTEX();                                                    \
        SWSS_LOG_ENTER();                                           \
        return meta_sai_get_oid(                                    \
                (sai_object_type_t)SAI_OBJECT_TYPE_ ## OBJECT_TYPE, \
                object_type ## _id,                                 \
                attr_count,                                         \
                attr_list,                                          \
                &redis_generic_get);                                \
    }

#define REDIS_GENERIC_QUAD(OT,ot)  \
    REDIS_CREATE(OT,ot);           \
    REDIS_REMOVE(OT,ot);           \
    REDIS_SET(OT,ot);              \
    REDIS_GET(OT,ot);

// struct object id

#define REDIS_CREATE_ENTRY(OBJECT_TYPE,object_type)             \
    sai_status_t redis_create_ ## object_type(                  \
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
                &redis_generic_create_ ## object_type);         \
    }

#define REDIS_REMOVE_ENTRY(OBJECT_TYPE,object_type)             \
    sai_status_t redis_remove_ ## object_type(                  \
            _In_ const sai_ ## object_type ## _t *object_type)  \
    {                                                           \
        MUTEX();                                                \
        SWSS_LOG_ENTER();                                       \
        return meta_sai_remove_ ## object_type(                 \
                object_type,                                    \
                &redis_generic_remove_ ## object_type);         \
    }

#define REDIS_SET_ENTRY(OBJECT_TYPE,object_type)                \
    sai_status_t redis_set_ ## object_type ## _attribute(       \
            _In_ const sai_ ## object_type ## _t *object_type,  \
            _In_ const sai_attribute_t *attr)                   \
    {                                                           \
        MUTEX();                                                \
        SWSS_LOG_ENTER();                                       \
        return meta_sai_set_ ## object_type(                    \
                object_type,                                    \
                attr,                                           \
                &redis_generic_set_ ## object_type);            \
    }

#define REDIS_GET_ENTRY(OBJECT_TYPE,object_type)                \
    sai_status_t redis_get_ ## object_type ## _attribute(       \
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
                &redis_generic_get_ ## object_type);            \
    }

#define REDIS_GENERIC_QUAD_ENTRY(OT,ot)  \
    REDIS_CREATE_ENTRY(OT,ot);           \
    REDIS_REMOVE_ENTRY(OT,ot);           \
    REDIS_SET_ENTRY(OT,ot);              \
    REDIS_GET_ENTRY(OT,ot);

// common api

#define REDIS_GENERIC_QUAD_API(ot)     \
    redis_create_ ## ot,               \
    redis_remove_ ## ot,               \
    redis_set_ ## ot ##_attribute,     \
    redis_get_ ## ot ##_attribute,

// stats

#define REDIS_GET_STATS(OBJECT_TYPE,object_type)                    \
    sai_status_t redis_get_ ## object_type ## _stats(               \
            _In_ sai_object_id_t object_type ## _id,                \
            _In_ uint32_t number_of_counters,                       \
            _In_ const sai_stat_id_t *counter_ids, \
            _Out_ uint64_t *counters)                               \
    {                                                               \
        MUTEX();                                                    \
        SWSS_LOG_ENTER();                                           \
        return  meta_sai_get_stats_oid(                             \
                SAI_OBJECT_TYPE_ ## OBJECT_TYPE,                    \
                object_type ## _id,                                 \
                &sai_metadata_enum_sai_ ## object_type ## _stat_t,  \
                number_of_counters,                                 \
                (const int32_t*)counter_ids,                        \
                counters,                                           \
                &redis_generic_get_stats);                          \
    }

#define REDIS_GET_STATS_EXT(OBJECT_TYPE,object_type)                \
    sai_status_t redis_get_ ## object_type ## _stats_ext(           \
            _In_ sai_object_id_t object_type ## _id,                \
            _In_ uint32_t number_of_counters,                       \
            _In_ const sai_stat_id_t *counter_ids, \
            _In_ sai_stats_mode_t mode,                             \
            _Out_ uint64_t *counters)                               \
    {                                                               \
        MUTEX();                                                    \
        SWSS_LOG_ENTER();                                           \
        return redis_generic_get_stats_ext(                         \
                SAI_OBJECT_TYPE_ ## OBJECT_TYPE,                    \
                object_type ## _id,                                 \
                &sai_metadata_enum_sai_ ## object_type ## _stat_t,  \
                number_of_counters,                                 \
                (const int32_t*)counter_ids,                        \
                mode,                                               \
                counters);                                          \
    }

#define REDIS_CLEAR_STATS(OBJECT_TYPE,object_type)                  \
    sai_status_t redis_clear_ ## object_type ## _stats(             \
            _In_ sai_object_id_t object_type ## _id,                \
            _In_ uint32_t number_of_counters,                       \
            _In_ const sai_stat_id_t *counter_ids) \
    {                                                               \
        MUTEX();                                                    \
        SWSS_LOG_ENTER();                                           \
        return redis_generic_clear_stats(                           \
                SAI_OBJECT_TYPE_ ## OBJECT_TYPE,                    \
                object_type ## _id,                                 \
                &sai_metadata_enum_sai_ ## object_type ## _stat_t,  \
                number_of_counters,                                 \
                (const int32_t*)counter_ids);                       \
    }

#define REDIS_GENERIC_STATS(OT, ot)    \
    REDIS_GET_STATS(OT,ot);            \
    REDIS_GET_STATS_EXT(OT,ot);        \
    REDIS_CLEAR_STATS(OT,ot);

// common stats api

#define REDIS_GENERIC_STATS_API(ot)    \
    redis_get_ ## ot ## _stats,        \
    redis_get_ ## ot ## _stats_ext,    \
    redis_clear_ ## ot ## _stats,
