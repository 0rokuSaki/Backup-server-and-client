/*******************************************************************************
 * Author: Aaron Barkan
 * GitHub repository: https://github.com/0rokuSaki/Backup-server-and-client/
 * Solution for exersice 14 in Defensive Systems Programming course
 * Implementation for a simple file backup server
 * Dependencies: Boost library (available at https://www.boost.org/)
 * 
 * Instructions for compilation (in VS):
 * 1. Include boost library (Configuration properties -> C/C++ -> General -> 
 *    Additional include directories -> add boost dir)
 * 2. Add boost lib folder to linker (Configuration properties -> Linker ->
 *    Additional library directories -> add boost's  stage/lib dir)
 * 3. Define macro _WIN32_WINNT=0x0A00 (Configuration properties ->
 *    C/C++ -> Preprocessor)
 * 
 ******************************************************************************/

#include "CBackupServer.h"
#include <iostream>


bool is_little_endian()
{
	int n = 1;
	return (*(char*)&n == 1) ? true : false;
}


int main(int argc, char** argv)
{
	CBackupServer server(is_little_endian());

	try
	{
		server.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << std::endl;
	}

	return 0;
}