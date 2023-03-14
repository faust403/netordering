# pragma once

# include <iostream>
# include <vector>
# include <memory>
# include <thread>
# include <queue>
# include <condition_variable>
# include <type_traits>
# include <string_view>

# include <boost/noncopyable.hpp>
# include <boost/asio.hpp>

constexpr std::string_view ErrorMessage = "Sorry";

namespace net
{
	struct connection final : public boost::noncopyable
	{
		std::unique_ptr<boost::asio::io_service> ios = nullptr;
		std::unique_ptr<boost::asio::ip::tcp::socket> Socket = nullptr;
		const std::size_t Port;

		connection(void) = default;
		template<typename Type>
		explicit connection(const Type __Port): Port(__Port)
		{
			if constexpr (!std::is_integral_v<Type>)
				throw std::invalid_argument("Given Port is not integral");
		}
		explicit connection(std::unique_ptr<boost::asio::io_service>&& __ios, std::unique_ptr<boost::asio::ip::tcp::socket>&& __Socket, std::size_t __Port)
			: ios(std::move(__ios)), Socket(std::move(Socket)), Port(std::move(__Port))
		{ }
		~connection(void) = default;
	};

	class listener final : public boost::noncopyable
	{
		bool Sleep = false;
		std::thread Listener;
		std::mutex ClientsMutex;
		std::mutex EnabledMutex;
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
				if constexpr (!std::is_integral_v<Type>)
					throw std::invalid_argument("Given Port is not integral");

				launch();
			}

			template<typename Type1, typename Type2>
			explicit listener(const Type1 Port, const Type2 Limit) : Limit(static_cast<std::size_t>(Limit)),
																	 Port(static_cast<std::size_t>(Port)),
																	 IsLocked(false),
																	 Enabled(true)
			{
				if constexpr (!std::is_integral_v<Type1> && !std::is_integral_v<Type2>)
					throw std::invalid_argument("Given Port or Limit is not integral");

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
				return Port.load(std::memory_order_relaxed);
			}

			template<typename Type>
			void set_port(const Type __Port)
			{
				if constexpr(!std::is_integral_v<Type>)
					throw std::invalid_argument("Given Port is not integral");

				disable();
				Port.store(__Port, std::memory_order_relaxed);
				enable();
			}

			std::size_t get_limit(void)
			{
				return Limit.load(std::memory_order_relaxed);
			}

			template<typename Type>
			void set_limit(const Type __Port)
			{
				if constexpr (!std::is_integral_v<Type>)
					throw std::invalid_argument("Given Limit is not integral");

				disable();
				Limit.store(__Port, std::memory_order_relaxed);
				enable();
			}

			std::unique_ptr<connection> pull_one(void)
			{
				std::lock_guard<std::mutex> LockGuard(ClientsMutex);

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
				Listener = std::thread([&](void) -> void {
					boost::asio::io_service IO_ServiceAcceptor;
					std::size_t CachedLimit = Limit.load(std::memory_order_acquire);

					while (Enabled.load(std::memory_order_acquire))
					{
						EnabledMutex.lock();
						IsLocked.store(true, std::memory_order_seq_cst);

						std::unique_ptr<connection> Connection = std::make_unique<connection>(Port.load(std::memory_order_acquire));
						Connection->ios = std::make_unique<boost::asio::io_service>();
						Connection->Socket = std::make_unique<boost::asio::ip::tcp::socket>(*Connection->ios);
						
						boost::asio::ip::tcp::endpoint EndPoint(boost::asio::ip::tcp::v4(), static_cast<boost::asio::ip::port_type>(Connection->Port));
						
						boost::asio::ip::tcp::acceptor Acceptor(IO_ServiceAcceptor, EndPoint);
						Acceptor.accept(*Connection->Socket);
						
						std::lock_guard<std::mutex> LockGuard(ClientsMutex);
						CachedLimit = Limit.load(std::memory_order_seq_cst);

						if (CachedLimit == 0 || Clients.size() < CachedLimit)
							Clients.push(std::move(Connection));
						else
							boost::asio::write(*Connection->Socket, boost::asio::buffer(ErrorMessage.data(), ErrorMessage.size()));
						
						EnabledMutex.unlock();
						IsLocked.store(false, std::memory_order_seq_cst);
					}
				});
			}
	};
}