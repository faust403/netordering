# pragma once

# include <iostream>
# include <cassert>
# include <vector>
# include <memory>
# include <thread>
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
	*		| connection:				   |
	*		| -> unique_ptr<socket> Socket |
	*		| -> unique_ptr<socket> ios	   |
	*		| #  const size_t Port		   |
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
	*					 *-----------------------*
	*	pull_one()  <->	 |	 Connections Order   |	<->	  Background thread listener
	*					 *-----------------------*
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

				disable();
				if (Listener.joinable())
					Listener.join();
				else
					throw std::runtime_error("Listener is not joinable");

				EnabledMutex.unlock();
			}

			void enable(void)
			{
				std::lock_guard<std::mutex> LockGuard(ThreadSafety);

				if(Sleep)
					if (!IsLocked.load(std::memory_order_acquire))
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

				if(!Sleep)
					if (IsLocked.load(std::memory_order_acquire))
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

	class server final : public boost::noncopyable
	{
		std::vector<std::unique_ptr<listener>> Listeners;

		public:
			server(void) = default;

			template<typename... Args>
			server(Args... args)
			{
				static_assert((std::is_integral_v<decltype(args)> && ...), "Given Port is not integral");

				(Listeners.push_back(std::make_unique<listener>(args)), ...);
			}

			~server(void) = default;

			template<typename Type>
			void add(const Type Port){}
	};
}