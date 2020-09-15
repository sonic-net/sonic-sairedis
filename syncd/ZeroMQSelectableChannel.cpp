#include "ZeroMQSelectableChannel.h"

#include "swss/logger.h"
#include "swss/json.h"

#include <zmq.h>

#define ZMQ_RESPONSE_BUFFER_SIZE (4*1024*1024)

using namespace syncd;

ZeroMQSelectableChannel::ZeroMQSelectableChannel(
        _In_ const std::string& endpoint):
    m_endpoint(endpoint),
    m_context(nullptr),
    m_socket(nullptr),
    m_fd(0)
{
    SWSS_LOG_ENTER();

    m_buffer.resize(ZMQ_RESPONSE_BUFFER_SIZE);

    m_context = zmq_ctx_new();;

    m_socket = zmq_socket(m_context, ZMQ_REP);

    int rc = zmq_bind(m_socket, endpoint.c_str());

    if (rc != 0)
    {
        SWSS_LOG_THROW("zmq_bind failed on endpoint: %s, zmqerrno: %d",
                endpoint.c_str(),
                zmq_errno());
    }

    size_t fd_len = sizeof(m_fd);

    rc = zmq_getsockopt(m_socket, ZMQ_FD, &m_fd, &fd_len);

    if (rc != 0)
    {
        SWSS_LOG_THROW("zmq_getsockopt failed on endpoint: %s, zmqerrno: %d",
                endpoint.c_str(),
                zmq_errno());
    }
}

ZeroMQSelectableChannel::~ZeroMQSelectableChannel()
{
    SWSS_LOG_ENTER();

    zmq_close(m_socket);
    zmq_ctx_destroy(m_context);
}

// SelectableChannel overrides

bool ZeroMQSelectableChannel::empty()
{
    SWSS_LOG_ENTER();

    // TODO

    return m_queue.size() == 0;
}

void ZeroMQSelectableChannel::pop(
        _Out_ swss::KeyOpFieldsValuesTuple& kco,
        _In_ bool initViewMode)
{
    SWSS_LOG_ENTER();

    if (m_queue.empty())
    {
        SWSS_LOG_ERROR("queue is empty, can't pop");
        throw std::runtime_error("queue is empty, can't pop");
    }

    std::string msg = m_queue.front();
    m_queue.pop();

    auto& values = kfvFieldsValues(kco);

    values.clear();

    swss::JSon::readJson(msg, values);

    swss::FieldValueTuple fvt = values.at(0);

    kfvKey(kco) = fvField(fvt);
    kfvOp(kco) = fvValue(fvt);

    values.erase(values.begin());
}

void ZeroMQSelectableChannel::set(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& values,
        _In_ const std::string& op)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> copy = values;

    swss::FieldValueTuple opdata(key, op);

    //copy.push_back(opdata);
    copy.insert(copy.begin(), opdata);

    std::string msg = swss::JSon::buildJson(copy);

    SWSS_LOG_DEBUG("sending: %s", msg.c_str());

    int rc = zmq_send(m_socket, msg.c_str(), msg.length(), 0);

    if (rc <= 0)
    {
        SWSS_LOG_THROW("zmq_send failed, on endpoint %s, zmqerrno: %d",
                m_endpoint.c_str(),
                zmq_errno());
    }
}

// Selectable overrides

int ZeroMQSelectableChannel::getFd()
{
    SWSS_LOG_ENTER();

    return m_fd;
}

uint64_t ZeroMQSelectableChannel::readData()
{
    SWSS_LOG_ENTER();

//    int zmq_events;
//    size_t zmq_events_len = sizeof(zmq_events);
//
//    int rc1 = zmq_getsockopt(m_socket, ZMQ_EVENTS, &zmq_events, &zmq_events_len);
//
//    printf("rc1 = %d\n", rc1);
//
//    printf("calling recv\n");

    int rc = zmq_recv(m_socket, m_buffer.data(), ZMQ_RESPONSE_BUFFER_SIZE, 0);

    if (rc < 0)
    {
        SWSS_LOG_THROW("zmq_recv failed, zmqerrno: %d", zmq_errno());
    }

    if (rc >= ZMQ_RESPONSE_BUFFER_SIZE)
    {
        SWSS_LOG_THROW("zmq_recv message was turncated (over %d bytes, recived %d), increase buffer size, message DROPPED",
                ZMQ_RESPONSE_BUFFER_SIZE,
                rc);
    }

    m_buffer.at(rc) = 0; // make sure that we end string with zero before parse

    m_queue.push((char*)m_buffer.data());

    return 0;
}

bool ZeroMQSelectableChannel::hasData()
{
    SWSS_LOG_ENTER();

    return m_queue.size() > 0;
}

bool ZeroMQSelectableChannel::hasCachedData()
{
    SWSS_LOG_ENTER();

    return m_queue.size() > 1;
}
