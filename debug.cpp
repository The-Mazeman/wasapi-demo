#include "include.hpp"
#include "debug.hpp"

void assert(bool result)
{
	if (result == 0)
	{
		*((char*)0) = 0;
	}
}