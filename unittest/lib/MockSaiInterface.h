#pragma once

#include <functional>
#include "DummySaiInterface.h"
#include <gmock/gmock.h>

class MockSaiInterface: public saimeta::DummySaiInterface
{
    public:

        MockSaiInterface();

        virtual ~MockSaiInterface();

    public:

        virtual sai_status_t apiInitialize(
                _In_ uint64_t flags,
                _In_ const sai_service_method_table_t *service_method_table) override;
        virtual sai_status_t apiUninitialize(void) override;

    public:
        MOCK_METHOD(sai_status_t, queryStatsCapability,
            (sai_object_id_t switchOid, sai_object_type_t objectType, sai_stat_capability_list_t* statCapList),
            (override));
};

