#include "AsicCmp.h"
#include "ViewCmp.h"

#include "swss/logger.h"

#include <iostream>

using namespace saiasiccmp;

AsicCmp::AsicCmp(
        _In_ std::shared_ptr<CommandLineOptions> options):
    m_commandLineOptions(options)
{
    SWSS_LOG_ENTER();

    // empty
}

bool AsicCmp::compare()
{
    SWSS_LOG_ENTER();

    auto& args = m_commandLineOptions->m_args;

    if (args.size() != 2)
    {
        SWSS_LOG_ERROR("ERROR: expected 2 input files, but given: %zu", args.size());
        return false;
    }

    try
    {
        auto a = std::make_shared<View>(args[0]);
        auto b = std::make_shared<View>(args[1]);

        SWSS_LOG_NOTICE("max objects: %lu %lu", a->m_maxObjectIndex, b->m_maxObjectIndex);

        b->translateViewVids(a->m_maxObjectIndex);

        ViewCmp cmp(a, b);

        return cmp.compareViews(m_commandLineOptions->m_dumpDiffToStdErr);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;

        SWSS_LOG_ERROR("Exception: %s", e.what());

        return false;
    }
}
