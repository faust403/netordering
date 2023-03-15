# include <boost/asio.hpp>

# include <openssl/evp.h>

# include <iostream>

# include <netordering/net.hpp>

std::string make_string(boost::asio::streambuf& streambuf)
{
	return std::string{ boost::asio::buffers_begin(streambuf.data()),
						boost::asio::buffers_end(streambuf.data()) };
}

class A final
{
	public:
		void service80(std::unique_ptr<net::connection>&& Connection)
		{
		
		}
		void operator()(std::unique_ptr<net::connection> Connection)
		{
			if (Connection->port == 80)
				service80(std::move(Connection));
		}
} a;

int main(void)
{
	net::server Server(a, 80);
	while(true) { }
}