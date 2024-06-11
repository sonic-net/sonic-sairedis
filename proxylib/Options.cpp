#include "Options.h"

#include "swss/logger.h"

#include <sstream>

using namespace saiproxy;

Options::Options()
{
    SWSS_LOG_ENTER();

    m_config = "config.ini";

    m_zmqChannel = "tcp://127.0.0.1:5555";
    m_zmqNtfChannel = "tcp://127.0.0.1:5556";
}

std::string Options::getString() const
{
    SWSS_LOG_ENTER();

    std::stringstream ss;

    ss << " Config=" << m_config;
    ss << " ZmqChannel=" << m_zmqChannel;
    ss << " ZmqNtfChannel=" << m_zmqNtfChannel;

    return ss.str();
}
