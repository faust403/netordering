# include <boost/asio.hpp>

# include <openssl/evp.h>

# include <iostream>

# include <netordering/net.hpp>

std::string make_string(boost::asio::streambuf& streambuf)
{
	return std::string{ boost::asio::buffers_begin(streambuf.data()),
						boost::asio::buffers_end(streambuf.data()) };
}

std::string hash(std::string const& String, const EVP_MD* HashFunction)
{
	if (String.size() == 0)
		return "";

	std::pair<std::array<unsigned char, EVP_MAX_MD_SIZE>, unsigned int> Bytes;
	EVP_MD_CTX* Context = EVP_MD_CTX_new();

	EVP_DigestInit(Context, HashFunction);
	EVP_DigestUpdate(Context, String.data(), String.size());
	EVP_DigestFinal(Context, Bytes.first.data(), &Bytes.second);

	std::stringstream MainSS;
	for (std::size_t Iter = 0; Iter < Bytes.second; Iter += 1)
	{
		std::stringstream ss;
		ss << std::hex << (int)Bytes.first[Iter];
		std::string Byte = ss.str();
		MainSS << (Byte.size() == 1 ? '0' + Byte : Byte);
	}
	return MainSS.str();
}

class A final
{
	public:
		// Type in your browser localhost
		void service80(const std::unique_ptr<net::connection>&& Connection)
		{
			boost::asio::streambuf StreamBuffer;
			std::string Result = "Hello from server";
			std::string Hash = "HTTP/1.0 200 OK\n\n<p>(SHA256)(" + Result + ") = " + hash(Result, EVP_sha256()) + "</p>";
			std::cout << "Write: " << Hash << std::endl;

			boost::asio::read_until(*Connection->socket, StreamBuffer, "\0");
			Connection->socket->write_some(boost::asio::buffer(Hash.c_str(), Hash.size()));
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}

		/* # Make o program on Python
		*  import socket
		*
        *  s = socket.socket()
        *  s.connect(("localhost", 13456))
		*
        *  print("Input a message:", end=' ')
        *  message = str(input())
		* 
		*  if len(message) == 0:
		*	exit(1)
		*
        *  print(message, f"({len(message)})")
        *  s.send(message.encode())
        *  print(s.recv(1024).decode('utf-8'))
		*/
		void service13456(const std::unique_ptr<net::connection>&& Connection)
		{
			boost::asio::streambuf StreamBuffer;
			boost::asio::read_until(*Connection->socket, StreamBuffer, "\0");

			std::string Read = make_string(StreamBuffer);
			std::string Hash = "(MD5)(" + Read + ") = " + hash(Read, EVP_md5());

			std::cout << "Write: " << Hash << std::endl;

			Connection->socket->write_some(boost::asio::buffer(Hash.c_str(), Hash.size()));
		}

		// This overloaded operator will be called in Server
		void operator()(const std::unique_ptr<net::connection> Connection)
		{
			if (Connection->port == 80)
				service80(std::move(Connection));

			else
				if (Connection->port == 13456)
					service13456(std::move(Connection));
		}
} a;

int main(void)
{
	net::server Server(a, 80, 13456);
	Server.set_limit_executor(3); // Only 3 clients will be executed in parallel. Others will waiting for they order
	Server.set_limit_order(2); // Order can be size of 2. Not more. If there will be new connection it will send error-message and close connection
	while (true) { } // There is while(true) because will be called distructor and Server will listen last connections. For demonstration purposes only
}