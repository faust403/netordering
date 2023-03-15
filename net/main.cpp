# include <boost/asio.hpp>

# include <iostream>

# include "net.hpp"

std::string make_string(boost::asio::streambuf& streambuf)
{
	return std::string{ boost::asio::buffers_begin(streambuf.data()),
						boost::asio::buffers_end(streambuf.data()) };
}

int main(void)
{
	net::queue Queue(1337);
	std::this_thread::sleep_for(std::chrono::seconds(3));
	Queue.disable();
	Queue.enable();
}