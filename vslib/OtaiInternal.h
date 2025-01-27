#pragma once

#define MUTEX() std::lock_guard<std::recursive_mutex> _lock(m_apimutex)
