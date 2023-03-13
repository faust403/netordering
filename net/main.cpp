# include <boost/asio.hpp>

# include <iostream>

# include "net.hpp"

int main(void)
{
	net::netqueue<2> Queue(1337);
	
	while (Queue.size() == 0)
		std::this_thread::sleep_for(std::chrono::seconds(3));

	boost::asio::ip::tcp::socket Socket(std::move(Queue.pull_one()));
	Socket.write_some(boost::asio::buffer("Hello\0", 7));

	while(true) { }
}