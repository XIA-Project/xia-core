#include "Xkeys.hh"

#include <iostream>
#include <memory>	// make_unique

int main()
{
	SIDKey tempkey;
	std::cout << tempkey.to_string() << std::endl;

	auto newkey = std::make_unique<SIDKey>();
	std::cout << newkey->to_string() << std::endl;
	return 0;
}
