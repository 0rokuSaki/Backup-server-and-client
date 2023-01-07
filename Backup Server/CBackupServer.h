// file CBackupServer.h
// this file contains declarations and definitions for class CBackupServer
#pragma once
#include "CSession.h"


class CBackupServer
{
	friend class CSession;

public:
	// ctor
	CBackupServer(bool is_little_endian) : m_little_endian(is_little_endian)
	{
		std::cout << "[BackupServer] Server is starting." << std::endl;

		// create a root directory for the server
		if (!boost::filesystem::exists(SERVER_ROOT_DIR))
		{
			boost::filesystem::create_directory(SERVER_ROOT_DIR);
		}
	}

	// run server
	void run()
	{
		std::cout << "[BackupServer] Server is listening on port: " << PORT << std::endl;

		boost::asio::io_context io_context;
		tcp::acceptor a(io_context, tcp::endpoint(tcp::v4(), PORT));

		try
		{
			while (true)
			{
				std::thread(handle_client, this, a.accept()).detach();
			}
		}
		catch (std::exception& e)
		{
			std::cerr << "Exception: " << e.what() << std::endl;
		}
	}

private:
	// handles a client connection
	static void handle_client(CBackupServer* p_server, tcp::socket socket)
	{
		std::cout << "[BackupServer] connection accepted from: " << socket.remote_endpoint().address().to_string() << std::endl;
		CSession* p_session = new CSession(p_server, &socket);

		try
		{
			p_session->begin();
		}
		catch (std::exception& e)
		{
			std::cerr << "[BackupServer] exception: " << e.what() << std::endl;
		}
		
		delete p_session;
	}

	bool m_little_endian;  // is machine little endian

	std::map<uint32_t, bool> m_usage_map;  // usage map (by user id)
};