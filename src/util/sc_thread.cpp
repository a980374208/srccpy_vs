#ifdef _WIN32
// 防止 windows.h 包含 winsock.h（通过预定义 _WINSOCKAPI_）
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#include <windows.h>
#endif

#include "sc_thread.h"
#include <cassert>
#include <cstring>
#include <iostream>

bool sc_thread_create(sc_thread *thread, sc_thread_fn fn, const char *name, void *userdata)
{
    assert(strlen(name) <= 15);

    try
    {
        thread->thread = std::make_unique<std::thread>(fn, userdata);
    }
    catch (const std::system_error &e)
    {
        return false;
    }

#ifdef __linux__
    pthread_setname_np(thread->thread->native_handle(), name);
#elif defined(_WIN32)
    // Windows 10+  SetThreadDescription need wchar_t
#endif

    return true;
}

sc_mutex::sc_mutex() : mutex_(std::make_unique<std::mutex>())
{
}

sc_mutex::~sc_mutex()
{
#ifndef NDEBUG
    assert(!held());
#endif
}

void sc_mutex::lock()
{
    assert(mutex_);
#ifndef NDEBUG
	assert(!held());   //forbid recursive locking
#endif
    mutex_->lock();
#ifndef NDEBUG
    locker_.store(std::this_thread::get_id(),
        std::memory_order_relaxed);
#endif
}

void sc_mutex::unlock()
{
#ifndef NDEBUG
    assert(held());
    locker_.store(sc_thread_id{},
        std::memory_order_relaxed);
#endif
    mutex_->unlock();
}

bool sc_mutex::held() const
{
    return locker_.load(std::memory_order_relaxed)
        == std::this_thread::get_id();
}

