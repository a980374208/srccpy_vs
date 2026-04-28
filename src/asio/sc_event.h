#pragma once
#include <future>
#include "asio.hpp"

struct ServerConnectSignal {
	std::promise<bool> promise;
};
