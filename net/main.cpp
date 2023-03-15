# include <boost/asio.hpp>

# include <iostream>

# include "net.hpp"

std::string make_string(boost::asio::streambuf& streambuf)
{
	return std::string{ boost::asio::buffers_begin(streambuf.data()),
						boost::asio::buffers_end(streambuf.data()) };
}

class A final
{
	public:
		void operator()(const std::unique_ptr<net::connection> Connection)
		{
			boost::asio::write(*Connection->socket, boost::asio::buffer("Hello", 6));
		}
} a;

int main(void)
{
	net::server Server(a, 1337);
	while(true) { }
}