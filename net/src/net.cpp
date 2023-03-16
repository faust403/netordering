# include <netordering/net.hpp>

std::size_t net::server::get_limit_executor(void) const
{
	return LimitExecutor.load(std::memory_order_relaxed);
}

std::vector<std::size_t> net::server::listeners(void)
{
	std::vector<std::size_t> Result;

	ListenersProtector.lock();
	for (decltype(Listeners)::iterator Iterator = Listeners.begin(); Iterator != Listeners.end(); Iterator += 1)
		Result.push_back(Iterator->get()->get_port());
	ListenersProtector.unlock();

	return Result;
}

std::vector<std::size_t> net::server::active_listeners(void)
{
	std::vector<std::size_t> Result;

	ListenersProtector.lock();
	for (decltype(Listeners)::iterator Iterator = Listeners.begin(); Iterator != Listeners.end(); Iterator += 1)
		if (Iterator->get()->is_enabled())
			Result.push_back(Iterator->get()->get_port());
	ListenersProtector.unlock();

	return Result;
}

void net::queue::launcher(void)
{
	Updater = std::thread([&](void) -> void {
		while (Enabled.load(std::memory_order_acquire))
		{
			std::lock_guard<std::mutex> ListenersProtectorLockGuard(ListenersProtector);

			for (auto& Listener : Listeners)
			{
				std::unique_ptr<net::connection> Connection = Listener->pull_one();

				if (Connection != nullptr)
				{
					std::lock_guard<std::mutex> QueueProtectorLockGuard(QueueProtector);
					const std::size_t CachedLimit = LimitOrder.load(std::memory_order_acquire);

					if (CachedLimit == 0 || Queue.size() < CachedLimit)
						Queue.push(std::move(Connection));
					else
					{
						boost::asio::write(*Connection->socket, boost::asio::buffer(ErrorMessage.data(), ErrorMessage.size()));

						Connection->socket->close();
					}
				}
			}
		}
	});
}

void net::queue::whileIsNotConstructed(void)
{
	while (IsConstructed.load(std::memory_order_acquire))
		continue;
}

void net::queue::update(void)
{
	if (Listeners.size() == 0)
		Status.store(false, std::memory_order_release);

	std::lock_guard<std::mutex> ListenersProtectorLockGuard(ListenersProtector);

	bool Result = false;
	for (auto& Listener : Listeners)
		Result |= Listener->is_enabled();

	Status.store(Result, std::memory_order_release);
}

bool net::queue::is_enabled(void) const
{
	return Status.load(std::memory_order_relaxed);
}

[[ nodiscard ]]
std::unique_ptr<net::connection> net::queue::pull_one(void)
{
	std::lock_guard<std::mutex> ThreadSafetyLockGuard(ThreadSafety);
	std::lock_guard<std::mutex> QueueProtectorLockGuard(QueueProtector);

	if (Queue.size() == 0)
		return nullptr;

	else
	{
		std::unique_ptr<net::connection> Result = std::move(Queue.front());
		Queue.pop();
		return Result;
	}
}

void net::queue::enable(void)
{
	std::lock_guard<std::mutex> ThreadSafetyLockGuard(ThreadSafety);
	ListenersProtector.lock();

	for (auto& Listener : Listeners)
		Listener->enable();

	ListenersProtector.unlock();
	update();
}

void net::queue::disable(void)
{
	std::lock_guard<std::mutex> ThreadSafetyLockGuard(ThreadSafety);
	ListenersProtector.lock();

	for (auto& Listener : Listeners)
		Listener->disable();

	ListenersProtector.unlock();
	update();
}

std::size_t net::queue::size(void)
{
	std::lock_guard<std::mutex> ThreadSafetyLockGuard(ThreadSafety);
	std::lock_guard<std::mutex> QueueProtectorLockGuard(QueueProtector);

	return Queue.size();
}

std::size_t net::queue::get_limit_order(void)
{
	return LimitOrder.load(std::memory_order_relaxed);
}

void net::listener::whileIsNotConstructed(void)
{
	while (!IsConstructed.load(std::memory_order_acquire))
		continue;
}

void net::listener::launch(void)
{
	std::lock_guard<std::mutex> LockGuard(ThreadSafety);

	Listener = std::thread([&](void) -> void {
		boost::asio::io_service IO_ServiceAcceptor;
		std::size_t CachedLimit = Limit.load(std::memory_order_acquire);

		IsConstructed.store(true, std::memory_order_seq_cst);
		while (Enabled.load(std::memory_order_acquire))
		{
			EnabledMutex.lock();
			IsLocked.store(true, std::memory_order_seq_cst);

			std::unique_ptr<net::connection> Connection = std::make_unique<net::connection>(Port.load(std::memory_order_acquire));
			Connection->ios = std::make_unique<boost::asio::io_service>();
			Connection->socket = std::make_unique<boost::asio::ip::tcp::socket>(*Connection->ios);

			boost::asio::ip::tcp::endpoint EndPoint(boost::asio::ip::tcp::v4(), static_cast<boost::asio::ip::port_type>(Connection->port));

			boost::asio::ip::tcp::acceptor Acceptor(IO_ServiceAcceptor, EndPoint);
			Acceptor.accept(*Connection->socket);

			std::lock_guard<std::mutex> LockGuard(ClientsMutex);
			CachedLimit = Limit.load(std::memory_order_seq_cst);

			if (CachedLimit == 0 || Clients.size() < CachedLimit)
				Clients.push(std::move(Connection));
			else
			{
				boost::asio::write(*Connection->socket, boost::asio::buffer(ErrorMessage.data(), ErrorMessage.size()));
			
				Connection->socket->close();
			}
			EnabledMutex.unlock();
			IsLocked.store(false, std::memory_order_seq_cst);
		}
	});
}

[[ nodiscard ]]
std::unique_ptr<net::connection> net::listener::pull_one(void)
{
	std::lock_guard<std::mutex> LockGuard(ThreadSafety);
	std::lock_guard<std::mutex> __LockGuard(ClientsMutex);

	if (Clients.size() == 0)
		return nullptr;
	else
	{
		std::unique_ptr<net::connection> Result = std::move(Clients.front());

		Clients.pop();
		return Result;
	}
}

bool net::listener::is_enabled(void) const
{
	return !Sleep;
}

std::size_t net::listener::size(void)
{
	std::lock_guard<std::mutex> LockGuard(ThreadSafety);
	std::lock_guard<std::mutex> __LockGuard(ClientsMutex);

	return Clients.size();
}

std::size_t net::listener::get_limit(void)
{
	std::lock_guard<std::mutex> LockGuard(ThreadSafety);

	return Limit.load(std::memory_order_relaxed);
}

void net::listener::enable(void)
{
	std::lock_guard<std::mutex> LockGuard(ThreadSafety);

	if (Sleep)
	{
		while (!IsLocked.load(std::memory_order_acquire))
			continue;

		EnabledMutex.unlock();
		IsLocked.store(false, std::memory_order_seq_cst);
		Sleep = false;
	}
}

void net::listener::disable(void)
{
	std::lock_guard<std::mutex> LockGuard(ThreadSafety);

	if (!Sleep)
	{
		while (IsLocked.load(std::memory_order_acquire))
			continue;

		EnabledMutex.lock();
		IsLocked.store(true, std::memory_order_seq_cst);
		Sleep = true;
	}
}

std::size_t net::listener::get_port(void)
{
	std::lock_guard<std::mutex> LockGuard(ThreadSafety);

	return Port.load(std::memory_order_relaxed);
}