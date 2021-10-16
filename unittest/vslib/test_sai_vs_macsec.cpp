#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, macsec)
{
    sai_macsec_api_t *api = nullptr;

    sai_api_query(SAI_API_MACSEC, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_macsec(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_macsec(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_macsec_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_macsec_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_macsec_port(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_macsec_port(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_macsec_port_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_macsec_port_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_macsec_port_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_macsec_port_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_macsec_port_stats(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_macsec_flow(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_macsec_flow(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_macsec_flow_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_macsec_flow_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_macsec_flow_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_macsec_flow_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_macsec_flow_stats(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_macsec_sc(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_macsec_sc(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_macsec_sc_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_macsec_sc_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_macsec_sc_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_macsec_sc_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_macsec_sc_stats(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_macsec_sa(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_macsec_sa(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_macsec_sa_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_macsec_sa_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_macsec_sa_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_macsec_sa_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_macsec_sa_stats(0,0,0));
}
