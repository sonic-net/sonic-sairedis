#include "sai_vs.h"
#include "sai_vs_internal.h"
#include "sai_vs_state.h"
#include "sai_vs_switch_BCM56850.h"
#include "sai_vs_switch_MLNX2700.h"

sai_status_t vs_clear_port_all_stats(
        _In_ sai_object_id_t port_id)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_create_port(
            _Out_ sai_object_id_t *port_id,
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t attr_count,
            _In_ const sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();

    /* create port */
    CHECK_STATUS(meta_sai_create_oid((sai_object_type_t)SAI_OBJECT_TYPE_PORT,
                port_id,switch_id,attr_count,attr_list,&vs_generic_create));

    if (g_vs_switch_type == SAI_VS_SWITCH_TYPE_BCM56850)
    {
        vs_create_port_BCM56850(*port_id, switch_id);
    }
    else if (g_vs_switch_type == SAI_VS_SWITCH_TYPE_MLNX2700)
    {
        vs_create_port_MLNX2700(*port_id, switch_id);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t vs_set_port_attribute(
            _In_ sai_object_id_t port_id,
            _In_ const sai_attribute_t *attr)
{
    MUTEX();
    SWSS_LOG_ENTER();

    std::string cmd;

    // Special handling for the sampling attribute modification
    if (attr->id == SAI_PORT_ATTR_INGRESS_SAMPLEPACKET_ENABLE)
    {
        // Get the sample-packet object id
        sai_object_id_t samplepacket_oid = attr->value.oid;

        // Get the interface name from the port id
        std::string if_name;

        sai_object_id_t vs_switch_id = sai_switch_id_query(port_id);
        if (vs_switch_id == SAI_NULL_OBJECT_ID)
        {
              SWSS_LOG_ERROR("vs_switch_id is null");
              return SAI_STATUS_FAILURE;
        }

        auto it = g_switch_state_map.find(vs_switch_id);
        if (it == g_switch_state_map.end())
        {
              SWSS_LOG_ERROR("No switch state found for the switch id %s",
                 sai_serialize_object_id(vs_switch_id).c_str());
              return SAI_STATUS_FAILURE;
        }

        auto sw = it->second;
        if (sw == nullptr)
        {
              SWSS_LOG_ERROR("switch state for the switch id %s is null",
                 sai_serialize_object_id(vs_switch_id).c_str());
              return SAI_STATUS_FAILURE;
        }

        if (sw->getTapNameFromPortId(port_id, if_name) == false)
        {
              SWSS_LOG_ERROR("tap interface name corresponding to the port id %s is not found",
                 sai_serialize_object_id(port_id).c_str());
              return SAI_STATUS_FAILURE;
        }

        if (samplepacket_oid == SAI_NULL_OBJECT_ID)
        {
              //Delete the sampling session
              cmd.assign("tc qdisc delete dev " + if_name + " handle ffff: ingress");
              if (system(cmd.c_str()) == -1)
              {
                  SWSS_LOG_ERROR("unable to delete the sampling session \
                                 for the interface %s",if_name);
                  SWSS_LOG_ERROR("failed to apply the command: %s",cmd);
                  return SAI_STATUS_FAILURE;
              }
              SWSS_LOG_INFO("successfully applied the command: %s", cmd);
        } else {
              //Get the sample rate from the sample object
              sai_attribute_t samplepacket_attr;
              samplepacket_attr.id = SAI_SAMPLEPACKET_ATTR_SAMPLE_RATE;

              if (SAI_STATUS_SUCCESS == \
                 vs_generic_get(SAI_OBJECT_TYPE_SAMPLEPACKET, samplepacket_oid, 1, &samplepacket_attr))
              {
                  int rate = samplepacket_attr.value.u32;

                  //Set the default sample group ID
                  std::string group("1");

                  //Check if sampling is already enabled on the port
                  sai_attribute_t port_attr;
                  port_attr.id = SAI_PORT_ATTR_INGRESS_SAMPLEPACKET_ENABLE;

                  // When the sampling parameters are updated,
                  // a delete and add operation is performed on the sampling session.
                  // If the sampling session is already created, it is deleted below.
                  if ((vs_generic_get(SAI_OBJECT_TYPE_PORT, port_id, 1, &port_attr) \
                       == SAI_STATUS_SUCCESS) && (port_attr.value.oid != SAI_NULL_OBJECT_ID))
                  {
                      //Sampling session is already created
                      SWSS_LOG_INFO("sampling is already enabled on the port: %s .. Deleting it", \
                                    sai_serialize_object_id(port_id).c_str());

                      //Delete the sampling session
                      cmd.assign("tc qdisc delete dev " + if_name + " handle ffff: ingress");
                      if (system(cmd.c_str()) == -1){
                         SWSS_LOG_ERROR("unable to delete the sampling session \
                                         for the interface %s",if_name);
                         SWSS_LOG_ERROR("failed to apply the command: %s",cmd);
                         return SAI_STATUS_FAILURE;
                      }
                      SWSS_LOG_INFO("successfully applied the command: %s", cmd);
                  }

                  //Create a new sampling session
                  cmd.assign("tc qdisc add dev " + if_name + " handle ffff: ingress");
                  if (system(cmd.c_str()) == -1)
                  {
                      SWSS_LOG_ERROR("unable to create a sampling session for the interface %s", if_name);
                      SWSS_LOG_ERROR("failed to apply the command: %s",cmd);
                      return SAI_STATUS_FAILURE;
                  }
                  SWSS_LOG_INFO("successfully applied the command: %s", cmd);

                  //Set the sampling rate of the port
                  cmd.assign("tc filter add dev " + if_name + \
                        " parent ffff: matchall action sample rate " + std::to_string(rate) + \
                        " group " + group);
                  if (system(cmd.c_str()) == -1)
                  {
                      SWSS_LOG_ERROR("unable to update the sampling rate of the interface %s",if_name);
                      SWSS_LOG_ERROR("failed to apply the command: %s",cmd);
                      return SAI_STATUS_FAILURE;
                  }
                  SWSS_LOG_INFO("successfully applied the command: %s", cmd);
              } else {
                  SWSS_LOG_ERROR("failed to update the port %s, unable to read the sample attr", if_name);
                  return SAI_STATUS_FAILURE;
              }
        }
        SWSS_LOG_INFO("successfully modified the sampling config of the port: %s",
                      sai_serialize_object_id(port_id).c_str());
    }

    return meta_sai_set_oid((sai_object_type_t)SAI_OBJECT_TYPE_PORT, port_id, attr, &vs_generic_set);
}

VS_REMOVE(PORT,port);
VS_GET(PORT,port);
VS_GENERIC_QUAD(PORT_POOL,port_pool);
VS_GENERIC_STATS(PORT,port);
VS_GENERIC_STATS(PORT_POOL,port_pool);

const sai_port_api_t vs_port_api = {

    VS_GENERIC_QUAD_API(port)
    VS_GENERIC_STATS_API(port)

    vs_clear_port_all_stats,

    VS_GENERIC_QUAD_API(port_pool)
    VS_GENERIC_STATS_API(port_pool)
};
