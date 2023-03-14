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
	net::listener Listener(1337, 2);
	std::this_thread::sleep_for(std::chrono::seconds(5));
}