// file CSession.cpp
// this file contains definitions for class CSession
#include "CBackupServer.h"
#include "CSession.h"


CSession::CSession(CBackupServer* p_server, tcp::socket* p_socket) : mp_socket(p_socket), mp_server(p_server)
{
	// empty
}


void CSession::begin()
{
	receive_request();  // lock_user() happens here

	process_request();

	send_response();

	unlock_user();
}


void CSession::receive_request()
{
	get_header();

	lock_user();

	if (BACKUP_FILE == m_request.op)
	{
		get_payload();
	}
}


void CSession::get_header()
{
	// get user id, version and op
	uint8_t user_id_buffer[BYTES_IN_USER_ID] = { 0 };
	uint8_t version_buffer[BYTES_IN_VERSION] = { 0 };
	uint8_t op_buffer[BYTES_IN_OP] = { 0 };

	// received data is little endian
	boost::asio::read(*mp_socket, boost::asio::buffer(user_id_buffer, BYTES_IN_USER_ID));
	boost::asio::read(*mp_socket, boost::asio::buffer(version_buffer, BYTES_IN_VERSION));
	boost::asio::read(*mp_socket, boost::asio::buffer(op_buffer, BYTES_IN_OP));

	if (!mp_server->m_little_endian)  // convert to big endian
	{
		std::reverse(user_id_buffer, user_id_buffer + BYTES_IN_USER_ID);
		std::reverse(version_buffer, version_buffer + BYTES_IN_VERSION);
		std::reverse(op_buffer, op_buffer + BYTES_IN_OP);
	}

	memcpy(&(m_request.user_id), user_id_buffer, BYTES_IN_USER_ID);
	memcpy(&(m_request.version), version_buffer, BYTES_IN_VERSION);
	memcpy(&(m_request.op), op_buffer, BYTES_IN_OP);

	// get file name (if applicable)
	if (BACKUP_FILE == m_request.op || RESTORE_FILE == m_request.op || DELETE_FILE == m_request.op)
	{
		// get file name length
		uint8_t name_len_buffer[BYTES_IN_NAME_LEN] = { 0 };

		// recieved data is little endian
		boost::asio::read(*mp_socket, boost::asio::buffer(name_len_buffer, BYTES_IN_NAME_LEN));

		if (!mp_server->m_little_endian)  // convert to big endian
		{
			std::reverse(name_len_buffer, name_len_buffer + BYTES_IN_NAME_LEN);
		}

		memcpy(&(m_request.name_len), name_len_buffer, BYTES_IN_NAME_LEN);

		// get file name
		char* file_name_buffer = new char[static_cast<size_t>(m_request.name_len) + 1];
		memset(file_name_buffer, 0, static_cast<size_t>(m_request.name_len) + 1);

		boost::asio::read(*mp_socket, boost::asio::buffer(file_name_buffer, m_request.name_len));

		m_request.file_name.append(file_name_buffer);
		delete[] file_name_buffer;
	}
}


void CSession::get_payload()
{
	// if user has no directory, make one for him
	std::string path = SERVER_ROOT_DIR "\\" + std::to_string(m_request.user_id);
	if (!boost::filesystem::exists(path))
	{
		boost::filesystem::create_directory(path);
	}

	// get payload size
	uint8_t payload_size_buffer[BYTES_IN_PAYLOAD_SIZE] = { 0 };

	// recieved data is little endian
	boost::asio::read(*mp_socket, boost::asio::buffer(payload_size_buffer, BYTES_IN_PAYLOAD_SIZE));

	if (!mp_server->m_little_endian)  // convert to big endian
	{
		std::reverse(payload_size_buffer, payload_size_buffer + BYTES_IN_PAYLOAD_SIZE);
	}

	memcpy(&(m_request.payload_size), payload_size_buffer, BYTES_IN_PAYLOAD_SIZE);

	// write payload directly to disk
	path.append("\\" + m_request.file_name);
	std::ofstream file(path, std::ios::binary);

	// get payload in small packets
	size_t bytes_remaining = m_request.payload_size;
	while (bytes_remaining)
	{
		char file_data_buffer[BYTES_IN_PACKET] = { 0 };

		size_t buff_size = std::min(bytes_remaining, static_cast<size_t>(BYTES_IN_PACKET));
		bytes_remaining -= boost::asio::read(*mp_socket, boost::asio::buffer(file_data_buffer, buff_size));

		file.write(file_data_buffer, buff_size);
	}
	file.close();

	// check all data received
	if (0 == bytes_remaining)
	{
		m_request.payload_received = true;
	}
	else
	{
		boost::filesystem::remove(path);
	}
}


void CSession::process_request()
{
	// process request and create a response
	const std::string user_dir = SERVER_ROOT_DIR "\\" + std::to_string(m_request.user_id);
	const std::string file_path = user_dir + "\\" + m_request.file_name;

	switch (m_request.op)
	{
	case BACKUP_FILE:
		if (m_request.payload_received)
		{
			m_response.status = SUCCESS_BACKUP_FILE;
		}
		else
		{
			m_response.status = ERROR_GENERIC;
		}
		break;

	case RESTORE_FILE:
		if (!boost::filesystem::is_directory(user_dir) || boost::filesystem::is_empty(user_dir))
		{
			m_response.status = ERROR_USER_HAS_NO_FILES;
		}
		else if (!boost::filesystem::exists(file_path))
		{
			m_response.status = ERROR_FILE_DOESNT_EXIST;
		}
		else
		{
			m_response.status = SUCCESS_RESTORE_FILE;
			m_response.name_len = m_request.name_len;
			m_response.file_name = m_request.file_name;
			m_response.payload_size = static_cast<uint16_t>(boost::filesystem::file_size(file_path));
			m_response.file_path = file_path;
		}
		break;

	case DELETE_FILE:
		if (!boost::filesystem::is_directory(user_dir) || boost::filesystem::is_empty(user_dir))
		{
			m_response.status = ERROR_USER_HAS_NO_FILES;
		}
		else if (!boost::filesystem::exists(file_path))
		{
			m_response.status = ERROR_FILE_DOESNT_EXIST;
		}
		else if (boost::filesystem::remove(file_path))
		{
			m_response.status = SUCCESS_DELETE_FILE;
		}
		else
		{
			m_response.status = ERROR_GENERIC;
		}
		break;

	case GENERATE_FILE_LIST:
		if (!boost::filesystem::is_directory(user_dir) || boost::filesystem::is_empty(user_dir))
		{
			m_response.status = ERROR_USER_HAS_NO_FILES;
		}
		else if (create_files_list(user_dir))
		{
			m_response.status = SUCCESS_GENERATE_FILE_LIST;
			m_response.name_len = static_cast<uint16_t>(strlen(FILE_LIST_NAME));
			m_response.file_name = FILE_LIST_NAME;
			m_response.file_path = user_dir + "\\" FILE_LIST_NAME;
			m_response.payload_size = static_cast<uint32_t>(boost::filesystem::file_size(m_response.file_path));
		}
		else
		{
			m_response.status = ERROR_GENERIC;
		}
		break;

	default:
		m_response.status = ERROR_GENERIC;
		break;
	}


}


bool CSession::create_files_list(const std::string& user_dir)
{
	std::ofstream files_list;
	files_list.open(user_dir + "\\" FILE_LIST_NAME);

	if (!files_list.is_open())
	{
		return false;
	}

	// cycle through the directory
	boost::filesystem::directory_iterator end_itr;

	for (boost::filesystem::directory_iterator itr(user_dir.c_str()); itr != end_itr; ++itr)
	{
		// if it's not a directory, list it.
		if (boost::filesystem::is_regular_file(itr->path())) {
			// extract file name from file path
			std::string current_file_path = itr->path().string();
			std::string current_file_name = current_file_path.substr(user_dir.length() + 1);
			if (current_file_name != FILE_LIST_NAME)
			{
				files_list << current_file_name << std::endl;
			}
		}
	}

	files_list.close();
	return true;
}


void CSession::send_response()
{
	send_header();

	if (SUCCESS_RESTORE_FILE == m_response.status || SUCCESS_GENERATE_FILE_LIST == m_response.status)
	{
		send_payload();
	}
}


void CSession::send_header()
{
	// send version and status
	uint8_t version_buffer[BYTES_IN_VERSION] = { 0 };
	uint8_t status_buffer[BYTES_IN_STATUS] = { 0 };

	// data sent is little endian
	memcpy(version_buffer, &(m_response.version), BYTES_IN_VERSION);
	memcpy(status_buffer, &(m_response.status), BYTES_IN_STATUS);

	if (!mp_server->m_little_endian)  // convert from big endian
	{
		std::reverse(version_buffer, version_buffer + BYTES_IN_VERSION);
		std::reverse(status_buffer, status_buffer + BYTES_IN_STATUS);
	}

	boost::asio::write(*mp_socket, boost::asio::buffer(version_buffer, BYTES_IN_VERSION));
	boost::asio::write(*mp_socket, boost::asio::buffer(status_buffer, BYTES_IN_STATUS));

	// send file name (if applicable)
	if (SUCCESS_RESTORE_FILE == m_response.status || SUCCESS_GENERATE_FILE_LIST == m_response.status)
	{
		// send file name length
		uint8_t name_len_buffer[BYTES_IN_NAME_LEN] = { 0 };

		// data sent is little endian
		memcpy(name_len_buffer, &(m_response.name_len), BYTES_IN_NAME_LEN);

		if (!mp_server->m_little_endian)
		{
			std::reverse(name_len_buffer, name_len_buffer + BYTES_IN_NAME_LEN);
		}

		boost::asio::write(*mp_socket, boost::asio::buffer(name_len_buffer, BYTES_IN_NAME_LEN));

		// send file name
		boost::asio::write(*mp_socket, boost::asio::buffer(m_response.file_name, m_response.name_len));
	}
}


void CSession::send_payload()
{
	// send payload sizes
	uint8_t payload_size_buffer[BYTES_IN_PAYLOAD_SIZE] = { 0 };

	memcpy(payload_size_buffer, &(m_response.payload_size), BYTES_IN_PAYLOAD_SIZE);

	if (!mp_server->m_little_endian)  // convert from big endian
	{
		std::reverse(payload_size_buffer, payload_size_buffer + BYTES_IN_PAYLOAD_SIZE);
	}

	boost::asio::write(*mp_socket, boost::asio::buffer(payload_size_buffer, BYTES_IN_PAYLOAD_SIZE));

	// open file for input
	std::ifstream file(m_response.file_path, std::ios::binary);

	// read file in chunks of 1024 bytes and send
	size_t bytes_remaining = m_response.payload_size;
	while (bytes_remaining)
	{
		char file_data_buffer[BYTES_IN_PACKET] = { 0 };

		size_t buff_size = std::min(bytes_remaining, static_cast<size_t>(BYTES_IN_PACKET));
		file.read(file_data_buffer, buff_size);

		bytes_remaining -= boost::asio::write(*mp_socket, boost::asio::buffer(file_data_buffer, buff_size));
	}
	file.close();
}


void CSession::lock_user()
{
	mp_server->m_mutex_usage_map.lock();
	while (mp_server->m_usage_map[m_request.user_id])
	{
		mp_server->m_mutex_usage_map.unlock();
		std::this_thread::sleep_for(std::chrono::milliseconds(LOCK_WAIT_PERIOD_MS));
		mp_server->m_mutex_usage_map.lock();
	}
	mp_server->m_usage_map[m_request.user_id] = true;
	mp_server->m_mutex_usage_map.unlock();
}

void CSession::unlock_user()
{
	mp_server->m_mutex_usage_map.lock();
	mp_server->m_usage_map.erase(m_request.user_id);
	mp_server->m_mutex_usage_map.unlock();
}
