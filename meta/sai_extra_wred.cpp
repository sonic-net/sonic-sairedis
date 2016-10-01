#include "sai_meta.h"
#include "sai_extra.h"

sai_status_t meta_pre_create_wred_profile(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_ecn_mark_mode_t ecn_mark_mode = SAI_ECN_MARK_MODE_NONE;

    const sai_attribute_t* attr_ecn_mark_mode = get_attribute_by_id(SAI_WRED_ATTR_ECN_MARK_MODE, attr_count, attr_list);

    if (attr_ecn_mark_mode != NULL)
    {
        ecn_mark_mode = (sai_ecn_mark_mode_t)attr_ecn_mark_mode->value.s32;

        switch (ecn_mark_mode)
        {
            case SAI_ECN_MARK_MODE_NONE:
            case SAI_ECN_MARK_MODE_GREEN:
            case SAI_ECN_MARK_MODE_YELLOW:
            case SAI_ECN_MARK_MODE_RED:
            case SAI_ECN_MARK_MODE_GREEN_YELLOW:
            case SAI_ECN_MARK_MODE_GREEN_RED:
            case SAI_ECN_MARK_MODE_YELLOW_RED:
            case SAI_ECN_MARK_MODE_ALL:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("invalid ecn mark mode value: %d", ecn_mark_mode);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    // TODO there should be probably logic here like:
    // green_min < green_max < yellow_min < yellow_max < red_min < red_max
    // This logic will be required also inside SET, so

    // TODO need to be obrained from switch, also will be required during set
    uint32_t max_buffer_size = 0x10000; // is this SAI_SWITCH_ATTR_TOTAL_BUFFER_SIZE ?

    // TODO change numbers to defines

    // GREEN

    bool green_enable = false; // default value

    const sai_attribute_t* attr_green_enable = get_attribute_by_id(SAI_WRED_ATTR_GREEN_ENABLE, attr_count, attr_list);

    if (attr_green_enable != NULL)
    {
        green_enable = attr_green_enable->value.booldata;
    }

    const sai_attribute_t* attr_green_min_threshold = get_attribute_by_id(SAI_WRED_ATTR_GREEN_MIN_THRESHOLD, attr_count, attr_list);
    const sai_attribute_t* attr_green_max_threshold = get_attribute_by_id(SAI_WRED_ATTR_GREEN_MAX_THRESHOLD, attr_count, attr_list);

    if (green_enable ||
            ecn_mark_mode == SAI_ECN_MARK_MODE_GREEN ||
            ecn_mark_mode == SAI_ECN_MARK_MODE_GREEN_YELLOW ||
            ecn_mark_mode == SAI_ECN_MARK_MODE_GREEN_RED ||
            ecn_mark_mode == SAI_ECN_MARK_MODE_ALL)
    {
        if (attr_green_min_threshold == NULL)
        {
            SWSS_LOG_ERROR("missing green min threshold attribute");

            return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
        }

        uint32_t green_min_threshold = attr_green_min_threshold->value.u32;

        // TODO this is extra validation
        // TODO get max buffer size
        if (green_min_threshold < 1 || green_min_threshold > max_buffer_size)
        {
            SWSS_LOG_ERROR("invalid green min threshold value: %u <%u..%u>", green_min_threshold, 1, max_buffer_size);

            return SAI_STATUS_INVALID_PARAMETER;
        }

        uint32_t green_max_threshold = attr_green_max_threshold->value.u32;

        // TODO this is extra validation
        // TODO get max buffer size
        if (green_max_threshold < 1 || green_max_threshold > max_buffer_size)
        {
            SWSS_LOG_ERROR("invalid green max threshold value: %u <%u..%u>", green_max_threshold, 1, max_buffer_size);

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    // TODO should this be only valid when green enable?
    const sai_attribute_t* attr_green_drop_probability = get_attribute_by_id(SAI_WRED_ATTR_GREEN_DROP_PROBABILITY, attr_count, attr_list);

    uint32_t green_drop_probability = 100; // default value

    if (attr_green_drop_probability != NULL)
    {
        green_drop_probability = attr_green_drop_probability->value.u32;

        if (green_drop_probability > 100)
        {
            SWSS_LOG_ERROR("invalid green drop probability: %u <%u..%u>", green_drop_probability, 0, 100);

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    // YELLOW

    bool yellow_enable = false; // default value

    const sai_attribute_t* attr_yellow_enable = get_attribute_by_id(SAI_WRED_ATTR_YELLOW_ENABLE, attr_count, attr_list);

    if (attr_yellow_enable != NULL)
    {
        yellow_enable = attr_yellow_enable->value.booldata;
    }

    const sai_attribute_t* attr_yellow_min_threshold = get_attribute_by_id(SAI_WRED_ATTR_YELLOW_MIN_THRESHOLD, attr_count, attr_list);
    const sai_attribute_t* attr_yellow_max_threshold = get_attribute_by_id(SAI_WRED_ATTR_YELLOW_MAX_THRESHOLD, attr_count, attr_list);

    if (yellow_enable ||
            ecn_mark_mode == SAI_ECN_MARK_MODE_YELLOW ||
            ecn_mark_mode == SAI_ECN_MARK_MODE_GREEN_YELLOW ||
            ecn_mark_mode == SAI_ECN_MARK_MODE_YELLOW_RED ||
            ecn_mark_mode == SAI_ECN_MARK_MODE_ALL)
    {
        if (attr_yellow_min_threshold == NULL)
        {
            SWSS_LOG_ERROR("missing yellow min threshold attribute");

            return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
        }

        uint32_t yellow_min_threshold = attr_yellow_min_threshold->value.u32;

        // TODO this is extra validation
        // TODO get max buffer size
        if (yellow_min_threshold < 1 || yellow_min_threshold > max_buffer_size)
        {
            SWSS_LOG_ERROR("invalid yellow min threshold value: %u <%u..%u>", yellow_min_threshold, 1, max_buffer_size);

            return SAI_STATUS_INVALID_PARAMETER;
        }

        uint32_t yellow_max_threshold = attr_yellow_max_threshold->value.u32;

        // TODO this is extra validation
        // TODO get max buffer size
        if (yellow_max_threshold < 1 || yellow_max_threshold > max_buffer_size)
        {
            SWSS_LOG_ERROR("invalid yellow max threshold value: %u <%u..%u>", yellow_max_threshold, 1, max_buffer_size);

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    // TODO should this be only valid when yellow enable?
    const sai_attribute_t* attr_yellow_drop_probability = get_attribute_by_id(SAI_WRED_ATTR_YELLOW_DROP_PROBABILITY, attr_count, attr_list);

    uint32_t yellow_drop_probability = 100; // default value

    if (attr_yellow_drop_probability != NULL)
    {
        yellow_drop_probability = attr_yellow_drop_probability->value.u32;

        if (yellow_drop_probability > 100)
        {
            SWSS_LOG_ERROR("invalid yellow drop probability: %u <%u..%u>", yellow_drop_probability, 0, 100);

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    // RED

    bool red_enable = false; // default value

    const sai_attribute_t* attr_red_enable = get_attribute_by_id(SAI_WRED_ATTR_RED_ENABLE, attr_count, attr_list);

    if (attr_red_enable != NULL)
    {
        red_enable = attr_red_enable->value.booldata;
    }

    const sai_attribute_t* attr_red_min_threshold = get_attribute_by_id(SAI_WRED_ATTR_RED_MIN_THRESHOLD, attr_count, attr_list);
    const sai_attribute_t* attr_red_max_threshold = get_attribute_by_id(SAI_WRED_ATTR_RED_MAX_THRESHOLD, attr_count, attr_list);

    if (red_enable ||
            ecn_mark_mode == SAI_ECN_MARK_MODE_RED ||
            ecn_mark_mode == SAI_ECN_MARK_MODE_GREEN_RED ||
            ecn_mark_mode == SAI_ECN_MARK_MODE_YELLOW_RED ||
            ecn_mark_mode == SAI_ECN_MARK_MODE_ALL)
    {
        if (attr_red_min_threshold == NULL)
        {
            SWSS_LOG_ERROR("missing red min threshold attribute");

            return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
        }

        uint32_t red_min_threshold = attr_red_min_threshold->value.u32;

        // TODO this is extra validation
        // TODO get max buffer size
        if (red_min_threshold < 1 || red_min_threshold > max_buffer_size)
        {
            SWSS_LOG_ERROR("invalid red min threshold value: %u <%u..%u>", red_min_threshold, 1, max_buffer_size);

            return SAI_STATUS_INVALID_PARAMETER;
        }

        uint32_t red_max_threshold = attr_red_max_threshold->value.u32;

        // TODO this is extra validation
        // TODO get max buffer size
        if (red_max_threshold < 1 || red_max_threshold > max_buffer_size)
        {
            SWSS_LOG_ERROR("invalid red max threshold value: %u <%u..%u>", red_max_threshold, 1, max_buffer_size);

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    // TODO should this be only valid when red enable?
    const sai_attribute_t* attr_red_drop_probability = get_attribute_by_id(SAI_WRED_ATTR_RED_DROP_PROBABILITY, attr_count, attr_list);

    uint32_t red_drop_probability = 100; // default value

    if (attr_red_drop_probability != NULL)
    {
        red_drop_probability = attr_red_drop_probability->value.u32;

        if (red_drop_probability > 100)
        {
            SWSS_LOG_ERROR("invalid red drop probability: %u <%u..%u>", red_drop_probability, 0, 100);

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    // END OF GREEN YELLOW RED

    sai_uint8_t weight = 0; // default weight

    const sai_attribute_t* attr_weight = get_attribute_by_id(SAI_WRED_ATTR_WEIGHT, attr_count, attr_list);

    if (attr_weight != NULL)
    {
        weight = attr_weight->value.u8;
    }

    if (weight > 15)
    {
        SWSS_LOG_ERROR("invalid weight value: %u <%u..%u>", weight, 0, 15);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_wred_profile(
        _In_ sai_object_id_t wred_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_wred_attribute(
        _In_ sai_object_id_t wred_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    switch (attr->id)
    {
        // TODO what happen if enable green and green threshold is not set ?
        case SAI_WRED_ATTR_GREEN_MIN_THRESHOLD:
        case SAI_WRED_ATTR_GREEN_MAX_THRESHOLD:
            // TODO check if min < max
            break;
        case SAI_WRED_ATTR_GREEN_DROP_PROBABILITY:
            break;

        case SAI_WRED_ATTR_YELLOW_MIN_THRESHOLD:
        case SAI_WRED_ATTR_YELLOW_MAX_THRESHOLD:
            // TODO check if min < max
            break;
        case SAI_WRED_ATTR_YELLOW_DROP_PROBABILITY:
            break;

        case SAI_WRED_ATTR_RED_MIN_THRESHOLD:
        case SAI_WRED_ATTR_RED_MAX_THRESHOLD:
            // TODO check if min < max
            break;
        case SAI_WRED_ATTR_RED_DROP_PROBABILITY:
            break;

        case SAI_WRED_ATTR_WEIGHT:
            break;

            // TODO can ECN be changed ? some attributes may be invalid or they may start to be valid
            // case SAI_WRED_ATTR_ECN_MARK_MODE: // TODO this should be CREATE_ONLY attribute
            //     break;

        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_wred_attribute(
        _In_ sai_object_id_t wred_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}
