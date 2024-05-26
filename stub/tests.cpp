extern "C" {
#include "saimetadata.h"
}

#include "swss/logger.h"

#include "sai_stub.h"
#include "meta/DummySaiInterface.h"

std::shared_ptr<sairedis::SaiInterface> stub_sai = std::make_shared<saimeta::DummySaiInterface>();

int main()
{
    SWSS_LOG_ENTER();

    stub_sai = std::make_shared<saimeta::DummySaiInterface>();

    sai_api_initialize(0,0);

    return 0;
}
