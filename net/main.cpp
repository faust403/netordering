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
			std::cout << "Connection from " << Connection->port << std::endl;
			boost::asio::streambuf StreamBuffer;
			boost::asio::read_until(*Connection->socket, StreamBuffer, "\0");

			std::string Read = make_string(StreamBuffer);
			std::cout << "Read: " << Read << std::endl;

			std::string Hash;
			if(Connection -> port == 1337)
				Hash = "(MD5)(" + Read + ") = " + hash(Read, EVP_md5());
			else
				Hash = "(SHA256)(" + Read + ") = " + hash(Read, EVP_sha256());
			std::cout << "Write: " << Hash << std::endl;

			boost::asio::write(*Connection->socket, boost::asio::buffer(Hash.c_str(), Hash.size()));
		}
} a;

int main(void)
{
	net::server Server(a, 1337, 228);
	while(true) { }
}