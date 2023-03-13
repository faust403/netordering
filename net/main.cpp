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
	Queue.add(80);
	
	while (Queue.size() == 0)
		std::this_thread::sleep_for(std::chrono::seconds(3));

	std::cout << "Ok" << std::endl;
	boost::asio::ip::tcp::socket Socket(std::move(Queue.pull_one()));
	std::cout << "Socket moved" << std::endl;
	boost::asio::streambuf StreamBuffer;
	std::cout << "SS created" << std::endl;
	boost::asio::read_until(Socket, StreamBuffer, "\0");
	std::cout << "read" << std::endl;
	std::cout << make_string(StreamBuffer) << std::endl;
	Socket.write_some(boost::asio::buffer("HTTP/1.0 200 OK\n\n<p>Hello</p>\n", 31));

	while(true) { }
}