# Backup-server-and-client

My solution for exercise 14 in Defensive Systems Programming (20937) course.<br />
This is an implementation for a simple file backup server.<br />

The operations supported are:
1. Backup file
2. Restore file
3. Delete file
4. Get list of files on backup

## Backup Server:

The server is based on this example: https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/example/cpp11/echo/blocking_tcp_echo_server.cpp<br />

1. Dependencies: Boost library (available at https://www.boost.org)

2. Compilation instructions (in VS):<br />
  a. Include boost library (Configuration properties -> C/C++ -> General -> Additional include directories -> add boost dir)<br />
  b. Add boost lib folder to linker (Configuration properties -> Linker -> Additional library directories -> add boost's  stage/lib dir)<br />
  c. Define macro _WIN32_WINNT=0x0A00 (Configuration properties -> C/C++ -> Preprocessor)

## Backup Client:

No special instructions. Run with 'run_client.bat'.

## Message to the exersice checker

The protocol did not state a status code for a successful backup or deletion of a file. Hence, I added the following status codes:
1. 208 - Successful deletion of a file
2. 209 - Successful backup of a file
