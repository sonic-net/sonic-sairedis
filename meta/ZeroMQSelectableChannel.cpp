#include "ZeroMQSelectableChannel.h"

#include "swss/logger.h"
#include "swss/json.h"

#include <zmq.h>
#include <unistd.h>

//#define ZMQ_POLL_TIMEOUT (2*60*1000)
#define ZMQ_POLL_TIMEOUT (1000)

using namespace sairedis;

ZeroMQSelectableChannel::ZeroMQSelectableChannel(
        _In_ const std::string& endpoint,
        _In_ long zmqResponseBufferSize):
    m_endpoint(endpoint),
    m_context(nullptr),
    m_socket(nullptr),
    m_fd(0),
    m_allowZmqPoll(false),
    m_runThread(true),
    m_zmqResponseBufferSize(zmqResponseBufferSize)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("binding on %s", endpoint.c_str());

    if (m_zmqResponseBufferSize != ZMQ_RESPONSE_DEFAULT_BUFFER_SIZE)
    {
        SWSS_LOG_NOTICE("setting zmq response buffer size to %ld bytes", m_zmqResponseBufferSize);
    }
    else
    {
        SWSS_LOG_NOTICE("using default zmq response buffer size of %ld bytes", ZMQ_RESPONSE_DEFAULT_BUFFER_SIZE);
    }

    m_buffer.resize(m_zmqResponseBufferSize);

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

    m_zmlPollThread = std::make_shared<std::thread>(&ZeroMQSelectableChannel::zmqPollThread, this);
}

ZeroMQSelectableChannel::~ZeroMQSelectableChannel()
{
    SWSS_LOG_ENTER();

    m_runThread = false;
    m_allowZmqPoll = true;

    zmq_close(m_socket);
    zmq_ctx_destroy(m_context);

    SWSS_LOG_NOTICE("ending zmq poll thread for channel %s", m_endpoint.c_str());

    m_zmlPollThread->join();

    SWSS_LOG_NOTICE("ended zmq poll thread for channel %s", m_endpoint.c_str());
}

void ZeroMQSelectableChannel::zmqPollThread()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("begin");

    while (m_runThread)
    {
        zmq_pollitem_t items [1] = { };

        items[0].socket = m_socket;
        items[0].events = ZMQ_POLLIN;

        m_allowZmqPoll = false;

        int rc = zmq_poll(items, 1, ZMQ_POLL_TIMEOUT);

        if (m_runThread == false)
        {
            SWSS_LOG_NOTICE("ending pool thread, since run is false");
            break;
        }

        if (rc <= 0 && zmq_errno() == ETERM)
        {
            SWSS_LOG_NOTICE("zmq_poll ETERM");
            break;
        }

        if (rc == 0)
        {
            SWSS_LOG_DEBUG("zmq_poll: no events, continue");
            continue;
        }

        // TODO we should have loop here in case we get multiple events since
        // zmq poll will only signal events once, but in our case we don't
        // expect multiple events, since we want to send/receive

        int zmq_events = 0;
        size_t zmq_events_len = sizeof(zmq_events);

        rc = zmq_getsockopt(m_socket, ZMQ_EVENTS, &zmq_events, &zmq_events_len);

        if (rc != 0)
        {
            SWSS_LOG_ERROR("zmq_getsockopt FAILED, zmq_errno: %d", zmq_errno());
            break;
        }

        if (rc == 0 && zmq_events & ZMQ_POLLIN)
        {
            m_selectableEvent.notify(); // will release epoll

            while (m_runThread && !m_allowZmqPoll)
            {
                usleep(10); // could be increased or replaced by spin lock

                //SWSS_LOG_NOTICE("m_allowZmqPoll == false");
            }
        }
        else
        {
            // should not happen, we only except ZMQ_POLLIN events

            SWSS_LOG_ERROR("unknown condition: rc: %d, zmq_events: %d, bug?", rc, zmq_events);
            break;
        }
    }

    SWSS_LOG_NOTICE("end");
}

// SelectableChannel overrides

bool ZeroMQSelectableChannel::empty()
{
    SWSS_LOG_ENTER();

    return m_queue.size() == 0;
}

void ZeroMQSelectableChannel::pop(
        _Out_ swss::KeyOpFieldsValuesTuple& kco,
        _In_ bool initViewMode)
{
    SWSS_LOG_ENTER();

    if (m_queue.empty())
    {
        SWSS_LOG_THROW("queue is empty, can't pop");
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

    copy.insert(copy.begin(), opdata);

    std::string msg = swss::JSon::buildJson(copy);

    SWSS_LOG_DEBUG("sending: %s", msg.c_str());

    int rc = zmq_send(m_socket, msg.c_str(), msg.length(), 0);

    // at this point we already did send/receive pattern, so we can notify
    // thread that we can poll again
    m_allowZmqPoll = true;

    if (rc <= 0)
    {
        SWSS_LOG_THROW("zmq_send failed, on endpoint %s, zmqerrno: %d: %s",
                m_endpoint.c_str(),
                zmq_errno(),
                zmq_strerror(zmq_errno()));
    }
}

// Selectable overrides

int ZeroMQSelectableChannel::getFd()
{
    SWSS_LOG_ENTER();

    return m_selectableEvent.getFd();
}

uint64_t ZeroMQSelectableChannel::readData()
{
    SWSS_LOG_ENTER();

    // clear selectable event so it could be triggered in next select()
    m_selectableEvent.readData();

    int rc = zmq_recv(m_socket, m_buffer.data(), m_zmqResponseBufferSize, 0);

    if (rc < 0)
    {
        SWSS_LOG_THROW("zmq_recv failed, zmqerrno: %d", zmq_errno());
    }

    if (rc >= m_zmqResponseBufferSize)
    {
        SWSS_LOG_THROW("zmq_recv message was truncated (over %d bytes, received %d), increase buffer size, message DROPPED",
                m_zmqResponseBufferSize,
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
