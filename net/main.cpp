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
		void operator()(const std::unique_ptr<net::connection> Connection)
		{
			boost::asio::streambuf StreamBuffer;
			std::string Read = "Evgeny pidor";
			std::string Hash;
			if (Connection->port == 1337)
				Hash = "HTTP/1.0 200 OK\n\n<p>(MD5)(" + Read + ") = " + hash(Read, EVP_md5()) + "</p>\n";
			else
				Hash = "HTTP/1.0 200 OK\n\n<p>(SHA256)(" + Read + ") = " + hash(Read, EVP_sha256()) + "</p>\n";
			std::cout << "Write: " << Hash << std::endl;

			boost::asio::read_until(*Connection->socket, StreamBuffer, "\0");
			Connection->socket->write_some(boost::asio::buffer(Hash.c_str(), Hash.size()));
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
} a;

int main(void)
{
	net::server Server(a, 80);
	while(true) { }
}