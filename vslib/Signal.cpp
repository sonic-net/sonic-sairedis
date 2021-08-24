#include "Signal.h"

#include "swss/logger.h"

using namespace saivs;

void Signal::notifyAll()
{
    SWSS_LOG_ENTER();

    m_cv.notify_all();
}

void Signal::notifyOne()
{
    SWSS_LOG_ENTER();

    m_cv.notify_one();
}

void Signal::wait()
{
    SWSS_LOG_ENTER();

    std::unique_lock<std::mutex> lock(m_mutex);

    m_cv.wait(lock);
}
