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
        self.data_sock = None
        self.data_host = None
        self.data_port = None
        self.is_pasv = None
        self.file = None

    def connect(self, host='127.0.0.1', port=21):
        self.ctrl_sock = socket.create_connection((host, port))
        self.file = self.ctrl_sock.makefile('r', encoding=ENCODING)
        res = self.get_response()
        return res

    def exit(self):
        if self.file is not None:
            self.file.close()
            self.file = None
        if self.ctrl_sock is not None:
            self.ctrl_sock.close()
            self.ctrl_sock = None

    '''
    def login(self, username='anonymous', password='anonymous@'):
        res = self.USER(username)
        if res[0] == '3':
            res = self.PASS(password)
        if res[0] == '3':
            res = self.ACCT('')
        if res[0] != '2':
            raise FTPError('Error code: {}'.format(res[:3]))
        return res
    '''

    def USER(self, username):
        return self.send_command('USER {}'.format(username))

    def PASS(self, password):
        return self.send_command('PASS {}'.format(password))

    '''
    def ACCT(self, acct):
        cmd = 'ACCT {}'.format(acct)
        return self.send_command(cmd)
    '''

    def RETR(self, filename):
        res150 = self.send_command('RETR {}'.format(filename))
        if not self.data_sock:
            raise FTPError('No data connection')
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
        return res150, res226

    def STOR(self, filename):
        res150 = self.send_command('STOR {}'.format(filename))
        if not self.data_sock:
            raise FTPError('No data connection')
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
        return res150, res226

    def REST(self, rest):
        return self.send_command('REST {}'.format(rest))

    def QUIT(self):
        res = self.send_command('QUIT')
        self.exit()
        return res

    def SYST(self):
        return self.send_command('SYST')

    def TYPE(self, t):
        return self.send_command('TYPE {}'.format(t))

    def PORT(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_TCP)
        host, port = self.ctrl_sock.getsockname()
        while True:
            port += 1
            try:
                sock.bind((host, port))
                break
            except IOError:
                pass
        sock.listen(1)
        h = host.split('.')
        p = [port // 256, port % 256]
        res = self.send_command('PORT {}'.format(','.join(h + p)))
        self.data_sock = sock.accept()[0]
        self.is_pasv = False
        return res

    def PASV(self):
        res = self.send_command('PASV')
        reg = re.compile(r'(\d+),(\d+),(\d+),(\d+),(\d+),(\d+)')
        match = reg.search(res)
        if not match:
            raise FTPError('Invalid response: {}'.format(res))
        nums = match.groups()
        host = '.'.join(nums[:4])
        port = int(nums[4]) * 256 + int(nums[5])
        self.data_sock = socket.create_connection((host, port))
        self.is_pasv = True
        return res

    def MKD(self, dir_name):
        return self.send_command('MKD {}'.format(dir_name))

    def CWD(self, dir_name):
        return self.send_command('CWD {}'.format(dir_name))

    def PWD(self):
        return self.send_command('PWD')

    def LIST(self, name):
        cmd = 'LIST'
        if isinstance(name, list) or isinstance(name, tuple):
            cmd = cmd + ' '.join(name)
        elif isinstance(name, str):
            cmd = cmd + ' ' + name
        res150 = self.send_command(cmd)
        if not self.data_sock:
            raise FTPError('No data connection')
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
        return res150, res226

    def RMD(self, dir_name):
        return self.send_command('RWD {}'.format(dir_name))

    def RNFR(self, old_name):
        return self.send_command('RNFR {}'.format(old_name))

    def RNTO(self, new_name):
        return self.send_command('RNTO {}'.format(new_name))

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
            line = line[-1:]
        return line

    def send_request(self, req):
        req = req.strip() + '\r\n'
        self.ctrl_sock.sendall(req.encode(ENCODING))
