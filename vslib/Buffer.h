#pragma once

#include "swss/sal.h"

#include <inttypes.h>

#include <cstddef>
#include <vector>

namespace saivs
{
    class Buffer
    {
        public:

            Buffer(
                    _In_ const uint8_t* data,
                    _In_ size_t size);

            virtual ~Buffer() = default;

        public:

            const uint8_t* getData() const;

            size_t getSize() const;

        private:

            std::vector<uint8_t> m_data;

            size_t m_size;
    };
}
