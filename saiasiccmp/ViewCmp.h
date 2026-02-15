#pragma once

#include "swss/sal.h"

#include "View.h"

#include <memory>

namespace saiasiccmp
{
    class ViewCmp
    {
        public:

            ViewCmp(
                    _In_ std::shared_ptr<View> a,
                    _In_ std::shared_ptr<View> b);

        public:

            bool compareViews(
                    _In_ bool dumpDiffToStdErr);

        private:

            void checkColdVids();

            void checkHidden();

            void checkVidRidMaps();

            void checkVidRidMaps(
                    _In_ std::shared_ptr<View> a,
                    _In_ std::shared_ptr<View> b);

            void checkStartingPoint();

            void checkStartingPoint(
                    _In_ sai_object_type_t ot);

        public:

            std::shared_ptr<View> m_va;
            std::shared_ptr<View> m_vb;
    };
}
