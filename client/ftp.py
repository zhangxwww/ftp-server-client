# ftp.py

import socket
import re

BUF_SIZE = 8192
ENCODING = 'latin-1'
SUCCESS_CODE = ['1', '2', '3']
ERROR_CODE = ['4', '5']


class FTPError(Exception):
    def __init__(self, info):
        self.info = info

    def __str__(self):
        return str(self.info)


class FTP:
    def __init__(self):
        self.ctrl_sock = None
        self.data_listen_sock = None
        self.data_sock = None
        self.data_host = None
        self.data_port = None
        self.is_pasv = None
        self.file = None

    def connect(self, host, port):
        self.ctrl_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_TCP)
        self.ctrl_sock.connect((host, port))
        self.file = self.ctrl_sock.makefile('r', encoding=ENCODING)
        res = self.get_response()
        return None, res

    def exit(self):
        if self.file is not None:
            self.file.close()
            self.file = None
        if self.ctrl_sock is not None:
            self.ctrl_sock.close()
            self.ctrl_sock = None

    def USER(self, username):
        cmd = 'USER {}'.format(username)
        return cmd, self.send_command(cmd)

    def PASS(self, password):
        cmd = 'PASS {}'.format(password)
        return cmd, self.send_command(cmd)

    def RETR(self, filename):
        if self.is_pasv:
            if not self.data_sock:
                raise FTPError('No data connection')
        cmd = 'RETR {}'.format(filename)
        res150 = self.send_command(cmd)
        if not self.is_pasv:
            self.data_sock = self.data_listen_sock.accept()[0]
            self.data_listen_sock.close()
            self.data_listen_sock = None
        with open(filename, 'wb') as f:
            while True:
                data = self.data_sock.recv(BUF_SIZE)
                if not data:
                    break
                f.write(data)
        res226 = self.get_response()
        self.is_pasv = None
        self.data_sock.close()
        self.data_sock = None
        return cmd, (res150, res226)

    def STOR(self, filename):
        if self.is_pasv:
            if not self.data_sock:
                raise FTPError('No data connection')
        cmd = 'STOR {}'.format(filename)
        res150 = self.send_command(cmd)
        if not self.is_pasv:
            self.data_sock = self.data_listen_sock.accept()[0]
            self.data_listen_sock.close()
            self.data_listen_sock = None
        with open(filename, 'wb') as f:
            while True:
                block = f.read(BUF_SIZE)
                if not block:
                    break
                self.data_sock.sendall(block)
        res226 = self.get_response()
        self.is_pasv = None
        self.data_sock.close()
        self.data_sock = None
        return cmd, (res150, res226)

    def REST(self, rest):
        cmd = 'REST {}'.format(rest)
        return cmd, self.send_command(cmd)

    def QUIT(self):
        cmd = 'QUIT'
        res = self.send_command(cmd)
        self.exit()
        return cmd, res

    def SYST(self):
        cmd = 'SYST'
        return cmd, self.send_command(cmd)

    def TYPE(self, t):
        cmd = 'TYPE {}'.format(t)
        return cmd, self.send_command(cmd)

    def PORT(self):
        sock = None
        for af, socktype, proto, _, saddr in socket.getaddrinfo(None, 0, socket.AF_INET, socket.SOCK_STREAM, 0, socket.AI_PASSIVE):
            try:
                sock = socket.socket(af, socktype, proto)
                sock.bind(saddr)
            except OSError:
                if sock:
                    sock.close()
                    sock = None
                continue
            break
        sock.listen(1)
        host = self.ctrl_sock.getsockname()[0]
        port = sock.getsockname()[1]
        h = host.split('.')
        p = [port // 256, port % 256]
        cmd = 'PORT {}'.format(','.join(map(str, h + p)))
        res = self.send_command(cmd)
        self.is_pasv = False
        self.data_listen_sock = sock
        return cmd, res

    def PASV(self):
        cmd = 'PASV'
        res = self.send_command(cmd)
        reg = re.compile(r'(\d+),(\d+),(\d+),(\d+),(\d+),(\d+)')
        match = reg.search(res)
        if not match:
            raise FTPError('Invalid response: {}'.format(res))
        nums = match.groups()
        host = '.'.join(nums[:4])
        port = int(nums[4]) * 256 + int(nums[5])
        print(host, port)
        self.data_sock = socket.create_connection((host, port))
        self.is_pasv = True
        return cmd, res

    def MKD(self, dir_name):
        cmd = 'MKD {}'.format(dir_name)
        return cmd, self.send_command(cmd)

    def CWD(self, dir_name):
        cmd = 'CWD {}'.format(dir_name)
        return cmd, self.send_command(cmd)

    def PWD(self):
        cmd = 'PWD'
        return cmd, self.send_command(cmd)

    def LIST(self, name):
        if self.is_pasv:
            if not self.data_sock:
                raise FTPError('No data connection')
        cmd = 'LIST'
        if isinstance(name, list) or isinstance(name, tuple):
            cmd = cmd + ' '.join(name)
        elif isinstance(name, str):
            cmd = cmd + ' ' + name
        res150 = self.send_command(cmd)
        if not self.is_pasv:
            self.data_sock = self.data_listen_sock.accept()[0]
            self.data_listen_sock.close()
            self.data_listen_sock = None
        lines = []
        with self.data_sock.makefile('r', encoding=ENCODING) as f:
            while True:
                line = f.readline(BUF_SIZE)
                if not line:
                    break
                if line[-2:] == '\r\n':
                    line = line[:-2]
                elif line[-1:] in ['\r', '\n']:
                    line = line[:-1]
                lines.append(line)
        res226 = self.get_response()
        self.is_pasv = None
        self.data_sock.close()
        self.data_sock = None
        return cmd, (res150, res226), lines

    def RMD(self, dir_name):
        cmd = 'RWD {}'.format(dir_name)
        return cmd, self.send_command(cmd)

    def RNFR(self, old_name):
        cmd = 'RNFR {}'.format(old_name)
        return cmd, self.send_command(cmd)

    def RNTO(self, new_name):
        cmd = 'RNTO {}'.format(new_name)
        return cmd, self.send_command(cmd)

    def send_command(self, cmd):
        self.send_request(cmd)
        return self.get_response()

    def get_response(self):
        line = self.parse_single_line()
        if line[3:4] == '-':
            code = line[:3]
            while True:
                nextline = self.parse_single_line()
                line = line + '\n' + nextline
                if nextline[:3] == code:
                    if nextline[3:4] == ' ':
                        break
                else:
                    raise FTPError('Wrong response format')
        return line

    def parse_single_line(self):
        line = self.file.readline(BUF_SIZE + 1)
        if not line:
            raise EOFError
        if line[-2:] == '\r\n':
            line = line[:-2]
        elif line[-1:] in ['\r', '\n']:
            line = line[:-1]
        return line

    def send_request(self, req):
        req = req.strip() + '\r\n'
        self.ctrl_sock.sendall(req.encode(ENCODING))
