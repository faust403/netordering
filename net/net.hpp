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
	
	/*
	*								  / -- Make mini order on port P1 --- (*)
	*								 / --- Make mini order on port P2 --- (*)
	*								/ ---- Make mini order on port P3 --- (*)
	*	   *---- One big order ----*
	*								\ ---- Make mini order on port P4 --- (*)
	*								 \ --- Make mini order on port P5 --- (*)
	*								 ...
	*								  \ -- Make mini order on port N  --- (*)
	*/
	template<std::size_t MaxOrder = 64>
	class netqueue final : public boost::noncopyable
	{
		std::mutex GlobalOrder;
		std::mutex MThreadSupport;
		std::atomic<bool> Enabled;
		boost::asio::io_service IO_Service;
		std::vector<std::pair<std::thread, std::size_t>> Listeners;
		std::queue<boost::asio::ip::tcp::socket> Queue;
		std::vector<boost::asio::ip::tcp::endpoint> EndPoints;

		public:
			netqueue(void) = default;
			template<typename... Args>
			explicit netqueue(Args... args) :Enabled(false)
			{
				if constexpr(!(std::is_integral_v<decltype(args)> && ...))
					throw std::invalid_argument("1 or more args are not an integers");
				else
					(EndPoints.push_back(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), args)), ...);
				
				(add(args), ...);
			}
			~netqueue(void)
			{
				if(Enabled.load(std::memory_order::acquire))
					disable();
			}

			std::size_t size(void) { return Queue.size(); }
			boost::asio::ip::tcp::socket pull_one(void)
			{
				std::lock_guard<std::mutex> MThreadSupportGuard(MThreadSupport);
				GlobalOrder.lock();
				boost::asio::ip::tcp::socket Socket = std::move(Queue.front());
				Queue.pop();
				GlobalOrder.unlock();

				return Socket;
			}
			
			void enable(void)
			{
				std::lock_guard<std::mutex> MThreadSupportGuard(MThreadSupport);
				Enabled.store(true, std::memory_order::relaxed);
				launch_all();
			}
			void disable(void)
			{
				std::lock_guard<std::mutex> MThreadSupportGuard(MThreadSupport);
				Enabled.store(false, std::memory_order::relaxed);

				for (auto& Listener : Listeners)
					Listener.first.join();

				Listeners.clear();
			}

			void add(boost::asio::ip::tcp::endpoint __EndPoint)
			{
				std::lock_guard<std::mutex> MThreadSupportGuard(MThreadSupport);
				if (!Enabled.load(std::memory_order::acquire))
				{
					if (std::find(EndPoints.begin(), EndPoints.end(), __EndPoint) == EndPoints.end())
						EndPoints.push_back(std::move(__EndPoint));
					return;
				}

				launch(__EndPoint);
			}

			template<typename Type>
			void add(const Type Port)
			{
				if constexpr (!std::is_integral_v<Type>)
					throw std::invalid_argument("invalid argument for port given");
				else
					add(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), Port));
			}

		private:
			
			void launch(const boost::asio::ip::tcp::endpoint __EndPoint)
			{
				std::thread Thread([&](const boost::asio::ip::tcp::endpoint EndPoint) -> void {
					boost::asio::ip::tcp::acceptor Acceptor(IO_Service, EndPoint);
					std::queue<boost::asio::ip::tcp::socket> Sockets;
					std::condition_variable Adding_cv;
					std::atomic<bool> Flag(false);
					std::mutex Adding;

					std::thread Adder([&](void) -> void {
						while (Enabled.load(std::memory_order::acquire))
						{
							std::unique_lock<std::mutex> UniqueLock(Adding);

							while (!Flag.load(std::memory_order::acquire))
								Adding_cv.wait(UniqueLock);

							GlobalOrder.lock();
							if (Queue.size() >= MaxOrder)
								boost::asio::write(Sockets.front(), boost::asio::buffer("Maximum order limit. Connection closed\0", 40));
							else
								Queue.push(std::move(Sockets.front()));
							GlobalOrder.unlock();
							Sockets.pop();
							Flag.store(false, std::memory_order::release);
						}
					});
					while (Enabled.load(std::memory_order::acquire))
					{
						boost::asio::ip::tcp::socket Socket(IO_Service);
						Acceptor.accept(Socket);

						std::lock_guard<std::mutex> LockGuard(Adding);
						Sockets.push(std::move(Socket));
						Flag.store(true, std::memory_order::seq_cst);
						Adding_cv.notify_one();
					}
					Adder.join();
				}, __EndPoint);
				Listeners.push_back(std::make_pair(std::move(Thread), __EndPoint.port()));
			}

			void launch_all(void)
			{
				for (auto const& EndPoint : EndPoints)
					launch(EndPoint);
			}
	};
}