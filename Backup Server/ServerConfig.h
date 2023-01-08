// file ServerConfig.h
// this file contains various configurations for the backup server

#pragma once
#include <algorithm>
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <utility>
#include <thread>
#include <chrono>
#include <vector>
#include <mutex>
#include <map>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>

// server related macros
#define SERVER_ROOT_DIR     "C:\\backupsvr"
#define FILE_LIST_NAME      "__file_list.txt"
#define PORT                5468
#define LOCK_WAIT_PERIOD_MS 3000
#define SERVER_VERSION      1

// byte-size macros
#define BYTES_IN_PACKET      1024
#define BYTES_IN_USER_ID        4
#define BYTES_IN_VERSION        1
#define BYTES_IN_OP             1
#define BYTES_IN_NAME_LEN       2
#define BYTES_IN_PAYLOAD_SIZE   4
#define BYTES_IN_STATUS         2


using boost::asio::ip::tcp;


enum EOp
{
	BACKUP_FILE = 100,
	RESTORE_FILE = 200,
	DELETE_FILE = 201,
	GENERATE_FILE_LIST = 202
};


enum EStatus
{
	SUCCESS_DELETE_FILE = 208,
	SUCCESS_BACKUP_FILE = 209,
	SUCCESS_RESTORE_FILE = 210,
	SUCCESS_GENERATE_FILE_LIST = 211,
	ERROR_FILE_DOESNT_EXIST = 1001,
	ERROR_USER_HAS_NO_FILES = 1002,
	ERROR_GENERIC = 1003
};


struct SRequest
{
	// header fields
	uint32_t user_id;
	uint8_t version;
	uint8_t op;
	uint16_t name_len;
	std::string file_name;

	// payload fields
	uint32_t payload_size;
	bool payload_received;

	// ctor
	SRequest() : user_id(0), version(0), op(0), name_len(0), file_name(""), payload_size(0), payload_received(false) {}
};


struct SResponse
{
	// header fields
	uint8_t version;
	uint16_t status;
	uint16_t name_len;
	std::string file_name;

	// payload fields
	uint32_t payload_size;
	std::string file_path;

	// ctor
	SResponse() : version(SERVER_VERSION), status(0), name_len(0), file_name(""), payload_size(0), file_path("") {}
};
