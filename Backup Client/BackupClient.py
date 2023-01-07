from enum import IntEnum
import traceback
import random
import socket
import sys
import os


FILE_LIST_NAME = '__file_list.txt'
PACKET_SIZE = 1024
VERSION = 1


class Op(IntEnum):
    BACKUP_FILE = 100
    RESTORE_FILE = 200
    DELETE_FILE = 201
    GENERATE_FILES_LIST = 202


class Status(IntEnum):
    SUCCESS_DELETE_FILE = 208
    SUCCESS_BACKUP_FILE = 209
    SUCCESS_RESTORE_FILE = 210
    SUCCESS_GENERATE_FILE_LIST = 211
    ERROR_FILE_DOESNT_EXIST = 1001
    ERROR_USER_HAS_NO_FILES = 1002
    ERROR_GENERIC = 1003


class Size(IntEnum):
    VERSION = 1
    STATUS = 2
    NAME = 2
    PAYLOAD = 4


def print_list(lst: list, indent='         '):
    for item in lst:
        print(indent + str(item))


def generate_request_header(client_id: int, version: int, op: int, file_name='') -> bytes:
    # create byte array for request header
    request = client_id.to_bytes(4, byteorder='little', signed=False)
    request += VERSION.to_bytes(1, byteorder='little', signed=False)
    request += op.to_bytes(1, byteorder='little', signed=False)
    if file_name:
        request += len(file_name).to_bytes(2, byteorder='little', signed=False)
        request += bytes(file_name, encoding='ascii')
    return request


def get_response_header(sock: socket) -> tuple:
    # receive response header and return values of response
    server_version = int.from_bytes(sock.recv(Size.VERSION), sys.byteorder, signed=False)
    status = int.from_bytes(sock.recv(Size.STATUS), sys.byteorder, signed=False)

    if status in [Status.SUCCESS_RESTORE_FILE, Status.SUCCESS_GENERATE_FILE_LIST, Status.ERROR_FILE_DOESNT_EXIST]:
        name_len = int.from_bytes(sock.recv(Size.NAME), sys.byteorder, signed=False)
        file_name = sock.recv(name_len).decode('ascii')
        return server_version, status, name_len, file_name

    return server_version, status, 0, ''


def send_payload(sock: socket, file_name: str, payload_size: int) -> None:
    with open(file_name, 'rb') as file:  # send file in packets of PACKET_SIZE bytes
        bytes_remaining = payload_size
        while bytes_remaining: # will always send exactly payload_size bytes
            file_data = file.read(min(PACKET_SIZE, bytes_remaining))
            bytes_remaining -= len(file_data)
            sock.sendall(file_data)


def receive_payload(sock: socket, file_name: str, payload_size: int) -> None:
    # receive payload & write to disk
    with open(file_name, 'wb') as file:  # receive file in packets of PACKET_SIZE bytes
        bytes_remaining = payload_size
        while bytes_remaining: # will always read exactly payload_size bytes
            file_data = sock.recv(min(PACKET_SIZE, bytes_remaining))
            bytes_remaining -= len(file_data)
            file.write(file_data)


def backup_file(client_id: int, file_name: str, sock: socket) -> Status:
    # request server to backup a file
    request = generate_request_header(client_id, VERSION, Op.BACKUP_FILE, file_name)
    payload_size = os.path.getsize(file_name)
    request += payload_size.to_bytes(4, byteorder='little', signed=False)
    sock.sendall(request)  # send header + file size

    send_payload(sock, file_name, payload_size)

    srv_ver, status, name_len, _file_name = get_response_header(sock)
    return status


def restore_file(client_id: int, file_name: str, sock: socket) -> Status:
    # request server to restore a file
    request = generate_request_header(client_id, VERSION, Op.RESTORE_FILE, file_name)
    sock.sendall(request)  # send header

    srv_ver, status, name_len, _file_name = get_response_header(sock)

    if Status.SUCCESS_RESTORE_FILE == status:  # file exists on server
        if os.path.isfile(file_name):  # if file exists for client, rename file to tmp
            file_name = 'tmp'

        payload_size = int.from_bytes(sock.recv(Size.PAYLOAD), sys.byteorder, signed=False)
        receive_payload(sock, file_name, payload_size)

    return status


def delete_file(client_id: int, file_name: str, sock: socket) -> Status:
    # request server to delete a file 
    request = generate_request_header(client_id, VERSION, Op.DELETE_FILE, file_name)
    sock.sendall(request)  # send header

    srv_ver, status, name_len, _file_name = get_response_header(sock)
    return status


def generate_files_list(client_id: int, sock: socket) -> Status:
    # request server to generate a list of all files that belong to the client
    request = generate_request_header(client_id, VERSION, Op.GENERATE_FILES_LIST)
    sock.sendall(request)  # send header

    srv_ver, status, name_len, file_name = get_response_header(sock)

    if Status.SUCCESS_GENERATE_FILE_LIST == status:
        global FILE_LIST_NAME
        FILE_LIST_NAME = file_name  # update file list name in case its changed in server

        payload_size = int.from_bytes(sock.recv(Size.PAYLOAD), sys.byteorder, signed=False)
        receive_payload(sock, file_name, payload_size)
        assert (os.path.getsize(file_name) == payload_size)  # check all data received

    return status


if __name__ == "__main__":
    try:
        # 1. random 4 byte unsigned int
        client_id = random.randint(0, 2 ** 32)
        print(f'[Client] client_id = {client_id}')

        # 2. read server details from server.info
        with open('server.info', 'r') as server_info:
            connection_details = list(server_info)[0].split(':')
            HOST = connection_details[0]
            PORT = int(connection_details[1])
        print(f'[Client] Server info: {HOST}:{PORT}')

        # 3. read backup details from backup.info
        with open('backup.info', 'r') as backup_info:
            backup_list = [line.rstrip('\n') for line in backup_info]
        print('[Client] Backup info:')
        print_list(backup_list)

        # 4. get files list on server & display
        print('[Client] Requesting server to generate files list...')
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect((HOST, PORT))
            status = generate_files_list(client_id, sock)
            if Status.SUCCESS_GENERATE_FILE_LIST == status:
                print(f"[Client] Request succeeded, status = {Status(status).name} ({status})")
                with open(FILE_LIST_NAME) as file:
                    print('[Client] Files on server:')
                    print_list(list(file))
            else:
                print(f"[Client] Request failed, status = {Status(status).name} ({status})")

        # 5. backup first file in backup.info
        print(f'[Client] Requesting server to backup file: {backup_list[0]}')
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect((HOST, PORT))
            status = backup_file(client_id, backup_list[0], sock)
            if Status.SUCCESS_BACKUP_FILE == status:
                print(f"[Client] Request succeeded, status = {Status(status).name} ({status})")
            else:
                print(f"[Client] Request failed, status = {Status(status).name} ({status})")

        # 6. backup second file in backup.info
        print(f'[Client] Requesting server to backup file: {backup_list[1]}')
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect((HOST, PORT))
            status = backup_file(client_id, backup_list[1], sock)
            if Status.SUCCESS_BACKUP_FILE == status:
                print(f"[Client] Request succeeded, status = {Status(status).name} ({status})")
            else:
                print(f"[Client] Request failed, status = {Status(status).name} ({status})")

        # 7. get files list on server & display
        print('[Client] Requesting server to generate files list...')
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect((HOST, PORT))
            status = generate_files_list(client_id, sock)
            if Status.SUCCESS_GENERATE_FILE_LIST == status:
                print(f"[Client] Request succeeded, status = {Status(status).name} ({status})")
                with open(FILE_LIST_NAME) as file:
                    print('[Client] Files on server:')
                    print_list([line.rstrip('\n') for line in file])
            else:
                print(f"[Client] Request failed, status = {Status(status).name} ({status})")

        # 8. restore first file in backup.info
        print(f'[Client] Requesting server to restore file: {backup_list[0]}')
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect((HOST, PORT))
            status = restore_file(client_id, backup_list[0], sock)
            if Status.SUCCESS_RESTORE_FILE == status:
                print(f"[Client] Request succeeded, status = {Status(status).name} ({status})")
            else:
                print(f"[Client] Request failed, status = {Status(status).name} ({status})")

        # 9. delete first file in backup.info
        print(f'[Client] Requesting server to delete file: {backup_list[0]}')
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect((HOST, PORT))
            status = delete_file(client_id, backup_list[0], sock)
            if Status.SUCCESS_DELETE_FILE == status:
                print(f"[Client] Request succeeded, status = {Status(status).name} ({status})")
            else:
                print(f"[Client] Request failed, status = {Status(status).name} ({status})")

        # 10. restore first file in backup.info
        print(f'[Client] Requesting server to restore file: {backup_list[0]}')
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect((HOST, PORT))
            status = restore_file(client_id, backup_list[0], sock)
            if Status.SUCCESS_RESTORE_FILE == status:
                print(f"[Client] Request succeeded, status = {Status(status).name} ({status})")
            else:
                print(f"[Client] Request failed, status = {Status(status).name} ({status})")

    except Exception:
        traceback.print_exc()

    print('[Client] Exiting program...')

