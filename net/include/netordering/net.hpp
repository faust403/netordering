# pragma once

# include <iostream>
# include <cassert>
# include <vector>
# include <memory>
# include <thread>
# include <future>
# include <queue>
# include <type_traits>
# include <string_view>

# include <boost/noncopyable.hpp>
# include <boost/asio.hpp>

constexpr std::string_view ErrorMessage = "Sorry";

namespace net
{
	/*
	*		*------------------------------*
	*		| connection:			|
	*		| -> unique_ptr<socket> Socket |
	*		| -> unique_ptr<socket> ios    |
	*		| #  const size_t Port		|
	*		*------------------------------*
	* 
	*	Pointer to this structure is a return type of pull_one()
	*	
	*	`socket` has type std::unique_ptr<boost::asio::ip::tcp::socket>
	*	`ios` has type std::unique_ptr<boost::asio::io_service>
	*	`port` has type const std::size_t
	* 
	*	Note that you have freedom working with this instance.
	*   You can distruct them or something else. Listener is have not access for this object after pull_one()
	*	and have not any relations with this instance after pull_one();
	*/
	struct connection final : public boost::noncopyable
	{
		std::unique_ptr<boost::asio::io_service> ios = nullptr;
		std::unique_ptr<boost::asio::ip::tcp::socket> socket = nullptr;
		const std::size_t port;

		connection(void) = default;
		template<typename Type>
		explicit connection(const Type __Port): port(__Port)
		{
			static_assert(std::is_integral_v<Type>, "Given Port is not integral");
		}
		explicit connection(std::unique_ptr<boost::asio::io_service>&& __ios, std::unique_ptr<boost::asio::ip::tcp::socket>&& __Socket, std::size_t __Port)
			: ios(std::move(__ios)), socket(std::move(__Socket)), port(std::move(__Port))
		{ }
		~connection(void) = default;
	};


	/*
	*			 *-----------------------*
	*	pull_one()  <->	 |	 Connections Order   |	<->	  Background thread listener
	*			 *-----------------------*
	*
	*	Listener is listening given port. By deafult this port is 80(HTTP).
	* 
	*	You can change port in runtime by set_port() and get it by get_port().
	* 
	*	You can change a limit of order by set_limit() and get it by get_limit().
	* 
	*	You can get a size of current queue of connections by size().
	* 
	*	You can enable listener by enable() and disable it by disable().
	*	Note that disable() waiting for last connection in current thread and then disabling listener.
	* 
	*	Distructor calls disable() and waiting for last connection.
	* 
	*	Instances of this object are thread-safety.
	*/
	class listener final : public boost::noncopyable
	{
		bool Sleep = false;
		std::thread Listener;
		std::mutex ClientsMutex;
		std::mutex EnabledMutex;
		std::mutex ThreadSafety;
		std::atomic<bool> Enabled;
		std::atomic<bool> IsLocked;
		std::atomic<std::size_t> Port;
		std::atomic<std::size_t> Limit;
		std::queue<std::unique_ptr<connection>> Clients;

		public:
			listener(void) : IsLocked(false),
							 Enabled(true),
							 Port(80),
							 Limit(0)
			{
				launch();
			}

			template<typename Type>
			explicit listener(const Type Port) : Port(static_cast<std::size_t>(Port)),
								IsLocked(false),
								Enabled(true),
								Limit(0)
			{
				static_assert(std::is_integral_v<Type>, "Given Port is not integral");

				launch();
			}

			template<typename Type1, typename Type2>
			explicit listener(const Type1 Port, const Type2 Limit) : Limit(static_cast<std::size_t>(Limit)),
										Port(static_cast<std::size_t>(Port)),
										IsLocked(false),
										Enabled(true)
			{
				static_assert(std::is_integral_v<Type1>, "Given Port is not integral");
				static_assert(std::is_integral_v<Type2>, "Given Limit is not integral");

				launch();
			}

			~listener(void)
			{
				Enabled.store(false, std::memory_order_release);

				if (Sleep)
					EnabledMutex.unlock();

				if (Listener.joinable())
					Listener.join();
				else
					throw std::runtime_error("Listener is not joinable");
			}

			void enable(void)
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

			void disable(void)
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

			std::size_t get_port(void)
			{
				std::lock_guard<std::mutex> LockGuard(ThreadSafety);

				return Port.load(std::memory_order_relaxed);
			}

			template<typename Type>
			void set_port(const Type __Port)
			{
				static_assert(std::is_integral_v<Type>, "Given Port is not integral");

				disable();
				Port.store(__Port, std::memory_order_relaxed);
				enable();
			}

			std::size_t get_limit(void)
			{
				std::lock_guard<std::mutex> LockGuard(ThreadSafety);

				return Limit.load(std::memory_order_relaxed);
			}

			template<typename Type>
			void set_limit(const Type __Port)
			{
				static_assert(std::is_integral_v<Type>, "Given Limit is not integral");

				disable();
				Limit.store(__Port, std::memory_order_relaxed);
				enable();
			}

			std::size_t size(void)
			{
				std::lock_guard<std::mutex> LockGuard(ThreadSafety);
				std::lock_guard<std::mutex> __LockGuard(ClientsMutex);

				return Clients.size();
			}

			[[ nodiscard ]]
			std::unique_ptr<connection> pull_one(void)
			{
				std::lock_guard<std::mutex> LockGuard(ThreadSafety);
				std::lock_guard<std::mutex> __LockGuard(ClientsMutex);

				if (Clients.size() == 0)
					return nullptr;
				else
				{
					std::unique_ptr<connection> Result = std::move(Clients.front());

					Clients.pop();
					return Result;
				}
			}

			bool is_enabled(void)
			{
				return Sleep;
			}

		private:
			void launch(void)
			{
				std::lock_guard<std::mutex> LockGuard(ThreadSafety);

				Listener = std::thread([&](void) -> void {
					boost::asio::io_service IO_ServiceAcceptor;
					std::size_t CachedLimit = Limit.load(std::memory_order_acquire);

					while (Enabled.load(std::memory_order_acquire))
					{
						EnabledMutex.lock();
						IsLocked.store(true, std::memory_order_seq_cst);

						std::unique_ptr<connection> Connection = std::make_unique<connection>(Port.load(std::memory_order_acquire));
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
							boost::asio::write(*Connection->socket, boost::asio::buffer(ErrorMessage.data(), ErrorMessage.size()));
						
						EnabledMutex.unlock();
						IsLocked.store(false, std::memory_order_seq_cst);
					}
				});
			}
	};

	/*
	*									 /  net::listener(Port1)
	*									/  net::listener(Port2)
	*	pull_one()  <->  net::queue --*
	*									\  net::listener(Port3)
	*									...
	*									  \  net::listener(PortN)
	* 
	*	net::queue is thread-safety object - you can work with his methods from any thread
	*	methods are same with net::listener
	* 
	*	But also you have ability to shutdown specific port(-s) and enable them again
	*/
	class queue : public boost::noncopyable
	{
		protected:
			std::thread Updater;
			std::mutex ThreadSafety;
			std::atomic<bool> Status;
			std::mutex QueueProtector;
			std::atomic<bool> Enabled;
			std::atomic<std::size_t> LimitOrder;
			std::vector<std::unique_ptr<listener>> Listeners;

			std::queue<std::unique_ptr<connection>> Queue;

		public:
			queue(void) : Enabled(true), Status(false), LimitOrder(0)
			{
				launcher();
			}

			template<typename... Args>
			queue(Args... args) : Enabled(true), Status(true), LimitOrder(0)
			{
				static_assert((std::is_integral_v<decltype(args)> && ...), "Given Port is not integral");

				(Listeners.push_back(std::make_unique<listener>(args)), ...);

				launcher();
			}

			~queue(void)
			{
				Enabled.store(false, std::memory_order_seq_cst);
				Listeners.clear();

				if (Updater.joinable())
					Updater.join();
				else
					throw std::runtime_error("Updater is not joinable");
			}

			template<typename Type>
			void add(const Type Port)
			{
				static_assert(std::is_integral_v<Type>, "Given Port is not integral");

				std::lock_guard<std::mutex> ThreadSafetyLockGuard(ThreadSafety);

				std::vector<std::unique_ptr<listener>>::iterator Iterator;
				for (Iterator = Listeners.begin(); Iterator != Listeners.end(); Iterator += 1)
					if (Iterator->get()->get_port() == Port)
					{
						Iterator = Listeners.end();
						break;
					}

				if (Iterator != Listeners.end())
				{
					Listeners.push_back(std::make_unique<listener>(Port));
					update();
				}
			}

			template<typename... Args>
			void add_list(Args... args)
			{
				(add(args), ...);
			}

			template<typename Type>
			void remove(const Type Port)
			{
				static_assert(std::is_integral_v<Type>, "Given Port is not integral");

				std::lock_guard<std::mutex> ThreadSafetyLockGuard(ThreadSafety);

				for(std::vector<std::unique_ptr<listener>>::iterator Iterator = Listeners.begin(); Iterator != Listeners.end(); Iterator += 1)
					if (Iterator->get()->get_port() == Port)
						Iterator = Listeners.erase(Iterator);

				update();
			}

			template<typename... Args>
			void remove_list(Args... args)
			{
				(remove(args), ...);
			}

			std::size_t size(void)
			{
				std::lock_guard<std::mutex> ThreadSafetyLockGuard(ThreadSafety);
				std::lock_guard<std::mutex> QueueProtectorLockGuard(QueueProtector);

				return Queue.size();
			}

			std::size_t get_limit_order(void)
			{
				return LimitOrder.load(std::memory_order_relaxed);
			}

			template<typename Type>
			void set_limit_order(const Type __Limit)
			{
				static_assert(std::is_integral_v<Type>, "Given Limit is not integral");

				LimitOrder.store(__Limit, std::memory_order_relaxed);
			}

			[[ nodiscard ]]
			std::unique_ptr<connection> pull_one(void)
			{
				std::lock_guard<std::mutex> ThreadSafetyLockGuard(ThreadSafety);
				std::lock_guard<std::mutex> QueueProtectorLockGuard(QueueProtector);

				if (Queue.size() == 0)
					return nullptr;

				else
				{
					std::unique_ptr<connection> Result = std::move(Queue.front());
					Queue.pop();
					return Result;
				}
			}

			void enable(void)
			{
 				std::lock_guard<std::mutex> ThreadSafetyLockGuard(ThreadSafety);

				for (auto& Listener : Listeners)
					Listener->enable();
				update();
			}

			void disable(void)
			{
				std::lock_guard<std::mutex> ThreadSafetyLockGuard(ThreadSafety);

				for (auto& Listener : Listeners)
					Listener->disable();
				update();
			}

			template<typename Type>
			void enable(const Type Port)
			{
				static_assert(std::is_integral_v<Type>, "Given Port is not integral");

				for (std::vector<std::unique_ptr<listener>>::iterator Iterator = Listeners.begin(); Iterator != Listeners.end(); Iterator += 1)
					if (Iterator->get()->get_port() == Port)
						Iterator->get()->enable();

				update();
			}

			template<typename... Type>
			void enable_list(const Type... Port)
			{
				static_assert((std::is_integral_v<decltype(Port)> && ...), "Given Port is not integral");

				(enable(Port), ...);
			}

			template<typename... Type>
			void disable_list(const Type... Port)
			{
				static_assert((std::is_integral_v<decltype(Port)> && ...), "Given Port is not integral");

				(disable(Port), ...);
			}

			bool is_enabled(void)
			{
				return Status.load(std::memory_order_relaxed);
			}

		private:
			void launcher(void)
			{
				Updater = std::thread([&](void) -> void {
					while(Enabled.load(std::memory_order_acquire))
					{
						for (auto& Listener : Listeners)
						{
							std::unique_ptr<connection> Connection = Listener->pull_one();

							if (Connection != nullptr)
							{
								std::lock_guard<std::mutex> QueueProtectorLockGuard(QueueProtector);
								const std::size_t CachedLimit = LimitOrder.load(std::memory_order_acquire);

								if(CachedLimit == 0 || Queue.size() < CachedLimit)
									Queue.push(std::move(Connection));
								else
									boost::asio::write(*Connection->socket, boost::asio::buffer(ErrorMessage.data(), ErrorMessage.size()));
							}
						}
					}
				});
			}

			void update(void)
			{
				if (Listeners.size() == 0)
					Status.store(false, std::memory_order_release);

				bool Result = false;
				for (auto& Listener : Listeners)
					Result |= Listener->is_enabled();

				Status.store(Result, std::memory_order_release);
			}
	};

	/*
	*									 /  <->  pull_one() <-> net::listener(Port1)
	*									/  <->  pull_one() <-> net::listener(Port2)
	*	net::server  <->  net::queue --*
	*									\  <->  pull_one() <-> net::listener(Port3)
	*									...
	*									  \  <->  pull_one() <-> net::listener(PortN)
	* 
	*	net::server is a main class in this library
	*	Constructor waiting for your callback function. It could be any callable object type of void(std::unique_ptr<net::connection>)
	*	Then in special race order server will call your callback function for any accepted client from any port
	* 
	*	But, server creating a new thread for each callback invoke - for each new net::connection
	*	You can set up the limit of this threads by set_limit_executor() or check it by get_limit_executor()
	*	By default it is std::thread::hardware_concurency()
	*/
	class server final : public boost::noncopyable, public queue
	{
		std::thread Updater;
		std::atomic<std::size_t> LimitExecutor;
		std::vector<std::pair<std::thread, std::future<void>>> Executors;

		public:
			template<typename Callback>
			server(const Callback CallBack) :queue(), LimitExecutor(std::thread::hardware_concurrency())
			{
				launch(CallBack);
			}

			template<typename Callback, typename... Args>
			server(const Callback CallBack, Args... args) :queue(args...), LimitExecutor(std::thread::hardware_concurrency())
			{
				launch(CallBack);
			}

			~server(void)
			{
				Enabled.store(false, std::memory_order_release);

				if (Updater.joinable())
					Updater.join();
				else
					throw std::runtime_error("Updater is not joinable");
			}

			template<typename Type>
			void set_limit_executor(const Type Limit)
			{
				static_assert(std::is_integral_v<Type>, "Given Limit is not integral");

				LimitExecutor.store(Limit, std::memory_order_relaxed);
			}

			std::size_t get_limit_executor(void)
			{
				return LimitExecutor.load(std::memory_order_relaxed);
			}

		private:
			template<typename Callback>
			void launch(const Callback CallBack)
			{
				static_assert(std::is_invocable_v<Callback, std::unique_ptr<connection>>, "Callable object must have unique_ptr<connection> as entry type and has operator()");
			
				Updater = std::thread([&](void) -> void {
					while (Enabled.load(std::memory_order_acquire))
					{
						for(decltype(Executors)::iterator Iterator = Executors.begin(); Iterator != Executors.end();)
						{
							if (Iterator->second.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
							{
								if (Iterator->first.joinable())
									Iterator->first.join();

								Iterator = Executors.erase(Iterator);
							}
							else
								Iterator += 1;
						}
						const std::size_t CachedLimit = LimitExecutor.load(std::memory_order_acquire);

						if (CachedLimit == 0 || Executors.size() < CachedLimit)
						{
							std::unique_ptr<connection> Connection = pull_one();
							if (Connection == nullptr)
								continue;

							std::packaged_task<void(std::unique_ptr<connection>)> Task{CallBack};
							std::future<void> Future = Task.get_future();
							Executors.push_back(std::make_pair(std::thread{ std::move(Task), std::move(Connection) }, std::move(Future)));
						}
					}
				});
			}
	};
}