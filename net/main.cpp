# include <boost/asio.hpp>

# include <iostream>

# include "net.hpp"
std::string make_string(boost::asio::streambuf& streambuf)
{
	return { boost::asio::buffers_begin(streambuf.data()),
			boost::asio::buffers_end(streambuf.data()) };
}
int main(void)
{
	net::netqueue<2> Queue(1337);
	while(true) { }
}