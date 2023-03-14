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

	while(Listener.size() == 0)
	{ }
	Listener.disable();
	std::unique_ptr<net::connection> Con = Listener.pull_one();
	boost::asio::write(*Con -> Socket, boost::asio::buffer("Hello", 6));
}