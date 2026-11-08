#pragma once

#include "swss/sal.h"
#include "swss/selectable.h"

namespace saivs
{
    class SelectableFd:
        public swss::Selectable
    {
        private:

            SelectableFd(const SelectableFd&) = delete;
            SelectableFd& operator=(const SelectableFd&) = delete;

        public:

            SelectableFd(
                    _In_ int fd);

            virtual ~SelectableFd() = default;

            int getFd() override;

            uint64_t readData() override;

        private:

            int m_fd;
    };
}
