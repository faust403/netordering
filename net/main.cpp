# include <iostream>

# include "net.hpp"

int main(void)
{
	net::netqueue<2> Queue(1337);
	Queue.enable();
	Queue.disable();
	Queue.enable();
	while(true) { }
}