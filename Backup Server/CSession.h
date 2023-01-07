// file CSession.h
// this file contains declarations for class CSession

#pragma once
#include "ServerConfig.h"


class CBackupServer;  // forward declaration


class CSession
{
public:

	// ctor
	CSession(CBackupServer*, tcp::socket*);

	// begin session with client
	void begin();


private:
	CBackupServer* mp_server;  // pointer to server

	tcp::socket* mp_socket;  // pointer to socket

	SRequest m_request;  // request structure

	SResponse m_response;  // response structure

	/* RECEIVE */
	// receive request from client
	void receive_request();

	// get request header
	void get_header();

	// get request payload
	void get_payload();

	/* PROCESS */
	// generate a response for client
	void process_request();

	// create a list of files on client's directory
	bool create_files_list(const std::string&);

	/* SEND */
	// send response to client
	void send_response();

	// send response header
	void send_header();

	// send response payload
	void send_payload();

	/* HELPER */
	// locks a client by user id
	// prevent two or more clients with the same user id to simultaneously make changes
	void lock_user();

	// unlock user id - release client lock
	void unlock_user();
};