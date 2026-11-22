#include "ZeroMQChannel.h"

#include "sairediscommon.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"
#include "swss/select.h"

#include <zmq.h>
#include <unistd.h>
#include <cstring>

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

static bool json_needs_escape(const std::string& s)
{
    SWSS_LOG_ENTER();

    for (unsigned char c : s)
    {
        if (c < 0x20 || c == '"' || c == '\\')
            return true;
    }

    return false;
}

static void json_append_plain(std::string& out, const std::string& s)
{
    SWSS_LOG_ENTER();

    out.push_back('"');
    out.append(s);
    out.push_back('"');
}

void ZeroMQChannel::set(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& values,
        _In_ const std::string& command)
{
    SWSS_LOG_ENTER();

    bool plain = !json_needs_escape(key) && !json_needs_escape(command);

    if (plain)
    {
        for (const auto& item : values)
        {
            if (json_needs_escape(fvField(item)) || json_needs_escape(fvValue(item)))
            {
                plain = false;
                break;
            }
        }
    }

    std::string msg;

    if (plain)
    {
        size_t bytes = key.size() + command.size() + 5;

        for (const auto& item : values)
            bytes += fvField(item).size() + fvValue(item).size() + 6;

        msg.reserve(bytes);
        msg.push_back('[');
        json_append_plain(msg, key);
        msg.push_back(',');
        json_append_plain(msg, command);

        for (const auto& item : values)
        {
            msg.push_back(',');
            json_append_plain(msg, fvField(item));
            msg.push_back(',');
            json_append_plain(msg, fvValue(item));
        }

        msg.push_back(']');
    }
    else
    {
        std::vector<swss::FieldValueTuple> copy = values;

        swss::FieldValueTuple opdata(key, command);

        copy.insert(copy.begin(), opdata);

        msg = swss::JSon::buildJson(copy);
    }

    send(msg);
}

void ZeroMQChannel::send(
        _In_ const std::string& msg)
{
    SWSS_LOG_ENTER();

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

int ZeroMQChannel::receiveMessage(
        _In_ const std::string& command)
{
    SWSS_LOG_ENTER();

    zmq_pollitem_t items [1] = { };

    items[0].socket = m_socket;
    items[0].events = ZMQ_POLLIN;

    int rc;

    for (int i = 0; true ; ++i)
    {
        rc = zmq_poll(items, 1, (int)m_responseTimeoutMs);

        if (rc == 0)
        {
            // REQ socket is now stuck in WAITING FOR REPLY state because
            // zmq_send was done but zmq_recv did not complete. Close and
            // recreate the socket to reset the state machine (Issue #26300).

            SWSS_LOG_ERROR("zmq_poll timed out for: %s, resetting REQ socket", command.c_str());

            zmq_close(m_socket);

            m_socket = zmq_socket(m_context, ZMQ_REQ);

            int connect_rc = zmq_connect(m_socket, m_endpoint.c_str());

            if (connect_rc != 0)
            {
                SWSS_LOG_THROW("failed to reconnect zmq endpoint %s after timeout, zmqerrno: %d",
                        m_endpoint.c_str(),
                        zmq_errno());
            }

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

    return rc;
}

static bool bulk_reply_all_success(
        _In_ const char* message,
        _In_ int length,
        _In_ uint32_t object_count)
{
    SWSS_LOG_ENTER();

    const std::string prefix = std::string("[\"SAI_STATUS_SUCCESS\",\"") + REDIS_ASIC_STATE_COMMAND_GETRESPONSE + "\"";
    const char item[] = ",\"SAI_STATUS_SUCCESS\",\"\"";
    const size_t item_len = sizeof(item) - 1;
    const size_t expect = prefix.size() + item_len * object_count + 1;

    if (object_count == 0 || (size_t)length != expect)
    {
        return false;
    }

    if (memcmp(message, prefix.data(), prefix.size()) != 0)
    {
        return false;
    }

    const char* cursor = message + prefix.size();

    for (uint32_t idx = 0; idx < object_count; ++idx)
    {
        if (memcmp(cursor, item, item_len) != 0)
        {
            return false;
        }

        cursor += item_len;
    }

    return cursor[0] == ']' && cursor[1] == '\0';
}

sai_status_t ZeroMQChannel::wait(
        _In_ const std::string& command,
        _Out_ swss::KeyOpFieldsValuesTuple& kco)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("wait for %s response", command.c_str());

    const int rc = receiveMessage(command);

    if (rc < 0)
    {
        return SAI_STATUS_FAILURE;
    }

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

sai_status_t ZeroMQChannel::waitBulkResponse(
        _In_ uint32_t object_count,
        _Out_ sai_status_t* object_statuses)
{
    SWSS_LOG_ENTER();

    const int rc = receiveMessage(REDIS_ASIC_STATE_COMMAND_GETRESPONSE);

    if (rc < 0)
    {
        return SAI_STATUS_FAILURE;
    }

    if (bulk_reply_all_success((const char*)m_buffer.data(), rc, object_count))
    {
        for (uint32_t idx = 0; idx < object_count; ++idx)
        {
            object_statuses[idx] = SAI_STATUS_SUCCESS;
        }

        return SAI_STATUS_SUCCESS;
    }

    std::vector<swss::FieldValueTuple> values;

    swss::JSon::readJson((char*)m_buffer.data(), values);

    swss::FieldValueTuple fvt = values.at(0);

    const std::string& opkey = fvField(fvt);
    const std::string& op = fvValue(fvt);

    values.erase(values.begin());

    if (op != REDIS_ASIC_STATE_COMMAND_GETRESPONSE)
    {
        SWSS_LOG_THROW("got not expected response: %s:%s, expected: %s",
                opkey.c_str(),
                op.c_str(),
                REDIS_ASIC_STATE_COMMAND_GETRESPONSE);
    }

    if (values.size() != object_count)
    {
        SWSS_LOG_THROW("wrong number of statuses, got %zu, expected %u", values.size(), object_count);
    }

    sai_status_t status;

    sai_deserialize_status(opkey, status);

    for (uint32_t idx = 0; idx < object_count; ++idx)
    {
        sai_deserialize_status(fvField(values[idx]), object_statuses[idx]);
    }

    return status;
}
