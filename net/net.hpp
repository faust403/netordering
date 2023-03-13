# pragma once

# include <boost/asio.hpp>
# include <boost/noncopyable.hpp>

# include <queue>
# include <array>
# include <thread>
# include <vector>
# include <iostream>
# include <type_traits>
# include <condition_variable>

namespace net
{	
	struct client final : public boost::noncopyable
	{
		boost::asio::ip::tcp::socket Socket;
		const std::size_t Port;
	};

	class listener final : public boost::noncopyable
	{
		const bool is_child = true;
		std::thread Listener;
		const std::size_t Port;
		boost::asio::io_service IO_Service;

		std::atomic<bool> Enabled;
		std::atomic<bool> ThreadCreated;
		std::atomic<bool>* ParentEnabled = new std::atomic<bool>(true);

		std::queue<boost::asio::ip::tcp::socket> Queue;
		std::queue<boost::asio::ip::tcp::socket>* ParentQueue = NULL;

		std::mutex* ParentMutex;

		const std::size_t MaxOrder = 0;

		public:
			listener(void) = delete;

			template<typename Type>
			explicit listener(const Type Port): Port(Port),
												Enabled(false),
												ThreadCreated(false)
			{
				if constexpr (!std::is_integral_v<Type>)
					throw std::invalid_argument("Invalid argument given");

				else
					enable();
			}

			template<typename Type>
			explicit listener(const Type __Port,
				std::atomic<bool>* ParentEnabled,
				std::queue<boost::asio::ip::tcp::socket>* ParentQueue,
				std::mutex* ParentMutex,
				const std::size_t MaxOrder) :
				ParentEnabled(ParentEnabled),
				ParentQueue(ParentQueue),
				ParentMutex(ParentMutex),
				MaxOrder(MaxOrder),
				is_child(false),
				Port(__Port),
				Enabled(false),
				ThreadCreated(false)
			{
				if constexpr (!std::is_integral_v<Type>)
					throw std::invalid_argument("Invalid argument given");

				else
					enable();
			}

			~listener(void)
			{
				if (is_child)
					delete ParentEnabled;

				disable();
			}

			void enable(void)
			{
				if (!Enabled.load(std::memory_order_acquire))
				{
					Enabled.store(true, std::memory_order_release);

					launch();
				}
			}

			void disable(void)
			{
				if (Enabled.load(std::memory_order_acquire))
				{
					while (!ThreadCreated.load(std::memory_order_acquire))
						continue;

					Enabled.store(false, std::memory_order_release);

					if (Listener.joinable())
						Listener.join();
					ThreadCreated.store(false, std::memory_order_release);
				}
			}

			std::size_t port(void) const noexcept { return Port; }

			bool operator == (listener const& Another)
			{
				return Port == Another.port();
			}

		private:
			void launch(void)
			{
				ThreadCreated.store(false, std::memory_order_seq_cst);
				Listener = std::thread([&](const boost::asio::ip::tcp::endpoint EndPoint) -> void {
					std::unique_ptr<std::condition_variable> ConditionVariable = nullptr;
					boost::asio::ip::tcp::acceptor Acceptor(IO_Service, EndPoint);
					std::unique_ptr<std::mutex> UpdaterMutex = nullptr;
					std::unique_ptr<std::atomic<bool>> Flag = nullptr;
					std::unique_ptr<std::thread> Updater = nullptr;

					if (!is_child) {
						ConditionVariable = std::make_unique<std::condition_variable>();
						UpdaterMutex = std::make_unique<std::mutex>();
						Flag = std::make_unique<std::atomic<bool>>(false);

						Updater = std::make_unique<std::thread>(([&](void) -> void {
							while (Enabled.load(std::memory_order_acquire) &&
								   ParentEnabled->load(std::memory_order_acquire))
							{
								std::unique_lock<std::mutex> UpdaterMutex_UniqueLock(*UpdaterMutex);
								while (!Flag->load(std::memory_order_acquire))
									ConditionVariable->wait(UpdaterMutex_UniqueLock);

								ThreadCreated.store(true, std::memory_order_release);
								
								ParentMutex->lock();
								if (ParentQueue->size() >= MaxOrder)
									boost::asio::write(Queue.front(), boost::asio::buffer("Maximum order limit. Connection closed\0", 40));
								else
									ParentQueue->push(std::move(Queue.front()));
								ParentMutex->unlock();

								Queue.pop();
								Flag->store(false, std::memory_order_release);
							}
						}));
					}

					while (Enabled.load(std::memory_order_acquire) &&
						   ParentEnabled->load(std::memory_order_acquire))
					{
						boost::asio::ip::tcp::socket Socket(IO_Service);
						Acceptor.accept(Socket);

						UpdaterMutex->lock();
						Queue.push(std::move(Socket));
						UpdaterMutex->unlock();

						if (!is_child)
						{
							Flag->store(true, std::memory_order_release);
							ConditionVariable->notify_one();
						}
					}
					if(!is_child)
						Updater->join();
					ThreadCreated.store(false, std::memory_order_release);
				}, boost::asio::ip::tcp::endpoint{boost::asio::ip::tcp::v4(), boost::asio::ip::port_type(Port)});
			}
	};

	template<std::size_t MaxOrder = 64>
	class netqueue final : public boost::noncopyable
	{
		std::atomic<bool> Enabled;
		std::vector<std::unique_ptr<listener>> Listeners;
		std::queue<boost::asio::ip::tcp::socket> Queue;
		std::mutex GlobalMutex;

		public:
			netqueue(void) :Enabled(true)
			{ }
			template<typename... Args>
			netqueue(Args... args) :Enabled(true)
			{
				if constexpr (!(std::is_integral_v<decltype(args)> && ...))
					throw std::invalid_argument("Invalid argument given. Port must be integral");

				(add(args), ...);
			}
			~netqueue(void)
			{
				disable();
				Listeners.clear();
			}

			std::size_t size(void)
			{
				std::lock_guard<std::mutex> LockGuard(GlobalMutex);

				return Queue.size();
			}

			void enable(void)
			{
				Enabled.store(true, std::memory_order_release);
			}
			void disable(void)
			{
				Enabled.store(false, std::memory_order_release);
			}

			template<typename Type>
			void add(const Type Port)
			{
				if constexpr (!std::is_integral_v<Type>)
					throw std::invalid_argument("Port is not an integer");

				for (std::vector<std::unique_ptr<listener>>::iterator Iterator = Listeners.begin(); Iterator != Listeners.end(); Iterator += 1)
					if (Iterator->get()->port() == Port)
						return;
				
				Listeners.push_back(
					std::make_unique<listener>(Port,
						&Enabled,
						&Queue,
						&GlobalMutex,
						MaxOrder)
				);
			}

			template<typename Type>
			void remove(const Type Port)
			{
				if constexpr (!std::is_integral_v<Type>)
					throw std::invalid_argument("Port is not an integer");

				for (std::vector<std::unique_ptr<listener>>::iterator Iterator = Listeners.begin(); Iterator != Listeners.end();)
					if (Iterator->get()->port() == Port)
						Iterator = Listeners.erase(Iterator);
					else
						Iterator += 1;
			}

			boost::asio::ip::tcp::socket pull_one(void)
			{
				GlobalMutex.lock();
				boost::asio::ip::tcp::socket Socket(std::move(Queue.front()));
				Queue.pop();
				GlobalMutex.unlock();

				return Socket;
			}
	};
}