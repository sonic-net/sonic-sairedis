#pragma once

#include <cstddef>
#include <string>

#include "swss/logger.h"

namespace sairedis
{
    // One copy from the already formatted bytes into the JSON string.
    // Quotes and backslashes are escaped in that copy. A control byte or a
    // non-ASCII byte returns false and the caller must discard out.
    inline bool append_json_quoted(
            std::string& out,
            const char* data,
            size_t length)
    {
        SWSS_LOG_ENTER();

        size_t begin = out.size();

        out.push_back('"');

        for (size_t i = 0; i < length; ++i)
        {
            unsigned char c = (unsigned char)data[i];

            if (c == '"')
            {
                out.push_back('\\');
                out.push_back('"');
            }
            else if (c == '\\')
            {
                out.push_back('\\');
                out.push_back('\\');
            }
            else if (c < 0x20 || c >= 0x80)
            {
                out.resize(begin);
                return false;
            }
            else
            {
                out.push_back((char)c);
            }
        }

        out.push_back('"');
        return true;
    }

    inline bool json_is_plain(
            const char* data,
            size_t length)
    {
        SWSS_LOG_ENTER();

        for (size_t i = 0; i < length; ++i)
        {
            unsigned char c = (unsigned char)data[i];

            if (c < 0x20 || c >= 0x80 || c == '"' || c == '\\')
            {
                return false;
            }
        }

        return true;
    }
}
