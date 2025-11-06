#include "ZeroMQChannel.h"

#include "sairediscommon.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"
#include "swss/select.h"

#include <zmq.h>
#include <unistd.h>

using namespace sairedis;

#define ZMQ_MAX_RETRY 10

ZeroMQChannel::ZeroMQChannel(
        _In_ const std::string& endpoint,
        _In_ const std::string& ntfEndpoint,
        _In_ Channel::Callback callback,
        _In_ long zmqResponseBufferSize):
    Channel(callback),
    m_endpoint(endpoint),
    m_ntfEndpoint(ntfEndpoint),
    m_context(nullptr),
    m_socket(nullptr),
    m_ntfContext(nullptr),
    m_ntfSocket(nullptr),
    m_zmqResponseBufferSize(zmqResponseBufferSize)
{
    SWSS_LOG_ENTER();
    if (m_zmqResponseBufferSize != ZMQ_RESPONSE_DEFAULT_BUFFER_SIZE)
    {
        SWSS_LOG_NOTICE("setting zmq response buffer size to %ld bytes", m_zmqResponseBufferSize);
    }
    else
    {
        SWSS_LOG_NOTICE("using default zmq response buffer size of %ld bytes", ZMQ_RESPONSE_DEFAULT_BUFFER_SIZE);
    }

    m_buffer.resize(m_zmqResponseBufferSize);

    // configure ZMQ for main communication

    m_context = zmq_ctx_new();

    m_socket = zmq_socket(m_context, ZMQ_REQ);

    SWSS_LOG_NOTICE("opening zmq main endpoint: %s", endpoint.c_str());

    int rc = zmq_connect(m_socket, endpoint.c_str());

    if (rc != 0)
    {
        SWSS_LOG_THROW("failed to open zmq main endpoint %s, zmqerrno: %d",
                endpoint.c_str(),
                zmq_errno());
    }

    // configure ZMQ notification endpoint

    m_ntfContext = zmq_ctx_new();

    m_ntfSocket = zmq_socket(m_ntfContext, ZMQ_PULL);

    SWSS_LOG_NOTICE("opening zmq ntf endpoint: %s", ntfEndpoint.c_str());

    rc = zmq_bind(m_ntfSocket, ntfEndpoint.c_str());

    if (rc != 0)
    {
        SWSS_LOG_THROW("failed to open zmq ntf endpoint %s, zmqerrno: %d",
                ntfEndpoint.c_str(),
                zmq_errno());
    }

    // start thread

    m_runNotificationThread = true;

    SWSS_LOG_NOTICE("creating notification thread");

    m_notificationThread = std::make_shared<std::thread>(&ZeroMQChannel::notificationThreadFunction, this);
}

ZeroMQChannel::~ZeroMQChannel()
{
    SWSS_LOG_ENTER();

    m_runNotificationThread = false;

    zmq_close(m_socket);
    zmq_ctx_destroy(m_context);

    // create new context, and perform send to break notification recv

    void* ctx = zmq_ctx_new();
    void* socket = zmq_socket(ctx, ZMQ_PUSH);

    int rc = zmq_connect(socket, m_ntfEndpoint.c_str());

    if (rc != 0)
    {
        SWSS_LOG_THROW("failed to open zmq ntf endpoint %s, zmqerrno: %d",
                m_ntfEndpoint.c_str(),
                zmq_errno());
    }

    rc = zmq_send(socket, "1", 1, 0);

    if (rc < 0)
    {
        SWSS_LOG_THROW("send error: %d, errno: %d, %s", rc, zmq_errno(), strerror(zmq_errno()));
    }

    zmq_close(socket);
    zmq_ctx_destroy(ctx);

    // when zmq context is destroyed, zmq_recv will be interrupted and errno
    // will be set to ETERM, so we don't need actual FD to be used in
    // selectable event

    SWSS_LOG_NOTICE("join ntf thread begin");

    m_notificationThread->join();

    SWSS_LOG_NOTICE("join ntf thread end");

    zmq_close(m_ntfSocket);
    zmq_ctx_destroy(m_ntfContext);
}

void ZeroMQChannel::notificationThreadFunction()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("start listening for notifications");

    std::vector<uint8_t> buffer;

    buffer.resize(m_zmqResponseBufferSize);

    while (m_runNotificationThread)
    {
        // NOTE: this entire loop internal could be encapsulated into separate class
        // which will inherit from Selectable class, and name this as ntf receiver

        int rc = zmq_recv(m_ntfSocket, buffer.data(), m_zmqResponseBufferSize, 0);

        if (!m_runNotificationThread)
            break;

        if (rc <= 0 && zmq_errno() == ETERM)
        {
            SWSS_LOG_NOTICE("zmq_recv interrupted with ETERM, ending thread");
            break;
        }

        if (rc < 0)
        {
            SWSS_LOG_ERROR("zmq_recv failed, zmqerrno: %d", zmq_errno());

            // at this point we don't know if next zmq_recv will succeed

            continue;
        }

        if (rc >= m_zmqResponseBufferSize)
        {
            SWSS_LOG_WARN("zmq_recv message was truncated (over %d bytes, received %d), increase buffer size, message DROPPED",
                    m_zmqResponseBufferSize,
                    rc);

            continue;
        }

        buffer.at(rc) = 0; // make sure that we end string with zero before parse

        SWSS_LOG_DEBUG("ntf: %s", buffer.data());

        std::vector<swss::FieldValueTuple> values;

        swss::JSon::readJson((char*)buffer.data(), values);

        swss::FieldValueTuple fvt = values.at(0);

        const std::string& op = fvField(fvt);
        const std::string& data = fvValue(fvt);

        values.erase(values.begin());

        SWSS_LOG_DEBUG("notification: op = %s, data = %s", op.c_str(), data.c_str());

        m_callback(op, data, values);
    }

    SWSS_LOG_NOTICE("exiting notification thread");
}

void ZeroMQChannel::setBuffered(
        _In_ bool buffered)
{
    SWSS_LOG_ENTER();

    // not supported
}

void ZeroMQChannel::flush()
{
    SWSS_LOG_ENTER();

    // not supported
}

void ZeroMQChannel::set(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& values,
        _In_ const std::string& command)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> copy = values;

    swss::FieldValueTuple opdata(key, command);

    copy.insert(copy.begin(), opdata);

    std::string msg = swss::JSon::buildJson(copy);

    SWSS_LOG_DEBUG("sending: %s", msg.c_str());

    for (int i = 0; true ; ++i)
    {
        int rc = zmq_send(m_socket, msg.c_str(), msg.length(), 0);

        if (rc <= 0 && zmq_errno() == EINTR && i < ZMQ_MAX_RETRY)
        {
            continue;
        }
        if (rc <= 0)
        {
            SWSS_LOG_THROW("zmq_send failed, on endpoint %s, zmqerrno: %d: %s",
                    m_endpoint.c_str(),
                    zmq_errno(),
                    zmq_strerror(zmq_errno()));
        }
        break;
    }
}

void ZeroMQChannel::del(
        _In_ const std::string& key,
        _In_ const std::string& command)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> values;

    set(key, values, command);
}

sai_status_t ZeroMQChannel::wait(
        _In_ const std::string& command,
        _Out_ swss::KeyOpFieldsValuesTuple& kco)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("wait for %s response", command.c_str());

    zmq_pollitem_t items [1] = { };

    items[0].socket = m_socket;
    items[0].events = ZMQ_POLLIN;

    int rc;

    for (int i = 0; true ; ++i)
    {
        rc = zmq_poll(items, 1, (int)m_responseTimeoutMs);

        if (rc == 0)
        {
            SWSS_LOG_ERROR("zmq_poll timed out for: %s", command.c_str());

            // notice, at this point we could throw, since in REP/REQ pattern
            // we are forced to use send/recv in that specific order

            return SAI_STATUS_FAILURE;
        }
        if (rc < 0 && zmq_errno() == EINTR && i < ZMQ_MAX_RETRY)
        {
            continue;
        }
        if (rc < 0)
        {
            SWSS_LOG_THROW("zmq_poll failed, zmqerrno: %d", zmq_errno());
        }
        break;
    }

    for (int i = 0; true ; ++i)
    {
        rc = zmq_recv(m_socket, m_buffer.data(), m_zmqResponseBufferSize, 0);

        if (rc < 0 && zmq_errno() == EINTR && i < ZMQ_MAX_RETRY)
        {
            continue;
        }
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
        break;
    }

    m_buffer.at(rc) = 0; // make sure that we end string with zero before parse

    SWSS_LOG_DEBUG("response: %s", m_buffer.data());

    std::vector<swss::FieldValueTuple> values;

    swss::JSon::readJson((char*)m_buffer.data(), values);

    swss::FieldValueTuple fvt = values.at(0);

    const std::string& opkey = fvField(fvt);
    const std::string& op= fvValue(fvt);

    values.erase(values.begin());

    kfvFieldsValues(kco) = values;
    kfvOp(kco) = op;
    kfvKey(kco) = opkey;

    SWSS_LOG_INFO("response: op = %s, key = %s", opkey.c_str(), op.c_str());

    if (op != command)
    {
        // we can hit this place if there were some timeouts
        // as well, if there will be multiple "GET" messages, then
        // we can receive response from not the expected GET

        SWSS_LOG_THROW("got not expected response: %s:%s, expected: %s", opkey.c_str(), op.c_str(), command.c_str());
    }

    sai_status_t status;
    sai_deserialize_status(opkey, status);

    SWSS_LOG_DEBUG("%s status: %s", command.c_str(), opkey.c_str());

    return status;
}
