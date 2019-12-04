#pragma once

#include <mutex>

namespace sairedis
{
    class Globals
    {
        private:

            Globals() = delete;
            ~Globals() = delete;

        public:

            /**
             * @brief Mutex that will protect all SAI interface API.
             *
             * All SAI interface API should be protected by this mutex to make
             * sure there can be only one API executed at a given time.  This
             * will also make sure that underlying metadata database will be
             * also accessed only from one place at a given time.
             *
             * Since SAI interface is global for entire sairedis library there
             * is no need to have more than one mutex protecting API access, so
             * a global API mutex is declared.
             */
            static std::mutex apimutex;

            /**
             * @brief Indicates whether SAI interface API is initialized.
             */
            static bool apiInitialized;
    };
}
