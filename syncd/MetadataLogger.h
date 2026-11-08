#pragma once

namespace syncd
{
    class MetadataLogger
    {
        private:

            MetadataLogger() = delete;
            ~MetadataLogger() = delete;

        public:

            static void initialize();
    };
}
