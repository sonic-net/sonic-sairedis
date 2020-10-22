#include "ZeroMQNotificationProducer.h"

#include <zmq.h>

using namespace syncd;

ZeroMQNotificationProducer::ZeroMQNotificationProducer(
        _In_ const std::string& ntfEndpoint):
    m_ntfContext(nullptr),
    m_ntfSocket(nullptr)
{
    SWSS_LOG_ENTER();

    m_ntfContext = zmq_ctx_new();

    m_ntfSocket = zmq_socket(m_ntfContext, ZMQ_PUB);

    SWSS_LOG_NOTICE("opening zmq ntf endpoint: %s", ntfEndpoint.c_str());

    int rc = zmq_connect(m_ntfSocket, ntfEndpoint.c_str());

    if (rc != 0)
    {
        SWSS_LOG_THROW("failed to open zmq ntf endpoint %s, zmqerrno: %d",
                ntfEndpoint.c_str(),
                zmq_errno());
    }
}

ZeroMQNotificationProducer::~ZeroMQNotificationProducer()
{
    SWSS_LOG_ENTER();

    zmq_close(m_ntfSocket);
    zmq_ctx_destroy(m_ntfContext);
}

void ZeroMQNotificationProducer::send(
        _In_ const std::string& op, 
        _In_ const std::string& data, 
        _In_ const std::vector<swss::FieldValueTuple>& values)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> vals = values;

    swss::FieldValueTuple opdata(op, data);

    vals.insert(vals.begin(), opdata);

    std::string msg = swss::JSon::buildJson(vals);

    SWSS_LOG_DEBUG("sending: %s", msg.c_str());

    int rc = zmq_send(m_ntfSocket, msg.c_str(), msg.length(), 0);

    if (rc < 0)
    {
        SWSS_LOG_THROW("zmq_send failed, zmqerrno: %d", zmq_errno());
    }
}
