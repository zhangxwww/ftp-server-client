# ftp.py

import socket
import re
import math

BUF_SIZE = 8192
PORT_PORT = 23333
PORT_HOST = '0.0.0.0'

def catchError(error):
    def catchErrorDecorator(f):
        def g(*args, **kwargs):
            try:
                return f(*args, **kwargs)
            except error as e:
                return None, str(e)
        return g
    return catchErrorDecorator

class FTP:
    def __init__(self):
        self.ctrl_sock = None
        self.data_listen_sock = None
        self.data_sock = None
        self.data_host = None
        self.data_port = None
        self.is_pasv = None
        self.file = None

    @catchError(IOError)
    def connect(self, host, port):
        self.ctrl_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_TCP)
        self.ctrl_sock.connect((host, port))
        self.file = self.ctrl_sock.makefile('r')
        res = self.getResponse()
        return None, res

    @catchError(IOError)
    def exit(self):
        if self.file is not None:
            self.file.close()
            self.file = None
        if self.ctrl_sock is not None:
            self.ctrl_sock.close()
            self.ctrl_sock = None

    @catchError(IOError)
    def shutdownDataConnection(self):
        if self.data_sock is not None:
            self.data_sock.shutdown(socket.SHUT_RDWR)
            self.data_sock.close()
            self.data_sock = None

    @catchError(IOError)
    def USER(self, username):
        cmd = 'USER {}'.format(username)
        return cmd, self.sendCommand(cmd)

    @catchError(IOError)
    def PASS(self, password):
        cmd = 'PASS {}'.format(password)
        return cmd, self.sendCommand(cmd)

    @catchError(IOError)
    def RETR(self, filename, f, callback=None, rest=0):
        if self.is_pasv:
            if not self.data_sock:
                return None, 'No data connection'
        cmd = 'RETR {}'.format(filename)
        res150 = self.sendCommand(cmd)
        if not self.is_pasv:
            self.data_sock = self.data_listen_sock.accept()[0]
            self.data_listen_sock.close()
            self.data_listen_sock = None
        if res150[:1] in ['4', '5']:
            return cmd, res150
        reg = re.compile(r'\((\d*) bytes\).$')
        match = reg.search(res150)
        total = None
        if match:
            total = int(math.floor(float(match.groups()[0])))
        cur = rest
        while True:
            if self.data_sock is None:
                break
            data = self.data_sock.recv(BUF_SIZE)
            if not data:
                break
            f.write(data)
            cur += len(data)
            if callback is not None:
                callback(total, cur)
        res226 = self.getResponse()
        self.is_pasv = None
        if self.data_sock is not None:
            self.data_sock.close()
            self.data_sock = None
        return cmd, (res150, res226)

    @catchError(IOError)
    def STOR(self, filename, f, callback=None):
        if self.is_pasv:
            if not self.data_sock:
                return None, 'No data connection'
        cmd = 'STOR {}'.format(filename)
        res150 = self.sendCommand(cmd)
        if not self.is_pasv:
            self.data_sock = self.data_listen_sock.accept()[0]
            self.data_listen_sock.close()
            self.data_listen_sock = None
        if res150[:1] in ['4', '5']:
            return cmd, res150
        n_byte = 0
        while True:
            block = f.read(BUF_SIZE)
            if not block:
                break
            self.data_sock.sendall(block)
            n_byte += len(block)
            if callback is not None:
                callback(n_byte)
        if self.data_sock is not None:
            self.data_sock.close()
            self.data_sock = None
        res226 = self.getResponse()
        self.is_pasv = None
        return cmd, (res150, res226)

    @catchError(IOError)
    def REST(self, rest):
        cmd = 'REST {}'.format(rest)
        return cmd, self.sendCommand(cmd)

    @catchError(IOError)
    def QUIT(self):
        cmd = 'QUIT'
        res = self.sendCommand(cmd)
        self.exit()
        return cmd, res

    @catchError(IOError)
    def SYST(self):
        cmd = 'SYST'
        return cmd, self.sendCommand(cmd)

    @catchError(IOError)
    def TYPE(self, t):
        cmd = 'TYPE {}'.format(t)
        return cmd, self.sendCommand(cmd)

    @catchError(IOError)
    def PORT(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_TCP)
        host = PORT_HOST
        port = PORT_PORT
        while True:
            try:
                sock.bind((host, port))
                break
            except:
                port += 1
                continue
        sock.listen(1)
        host = self.ctrl_sock.getsockname()[0]
        port = sock.getsockname()[1]
        h = host.split('.')
        p = [port // 256, port % 256]
        cmd = 'PORT {}'.format(','.join(map(str, h + p)))
        res = self.sendCommand(cmd)
        self.is_pasv = False
        self.data_listen_sock = sock
        return cmd, res

    @catchError(IOError)
    def PASV(self):
        cmd = 'PASV'
        res = self.sendCommand(cmd)
        reg = re.compile(r'(\d+),(\d+),(\d+),(\d+),(\d+),(\d+)')
        match = reg.search(res)
        if not match:
            return None, 'Invalid response: {}'.format(res)
        nums = match.groups()
        host = '.'.join(nums[:4])
        port = int(nums[4]) * 256 + int(nums[5])
        self.data_sock = socket.create_connection((host, port))
        self.is_pasv = True
        return cmd, res

    @catchError(IOError)
    def MKD(self, dir_name):
        cmd = 'MKD {}'.format(dir_name)
        return cmd, self.sendCommand(cmd)

    @catchError(IOError)
    def CWD(self, dir_name):
        cmd = 'CWD {}'.format(dir_name)
        return cmd, self.sendCommand(cmd)

    @catchError(IOError)
    def CDUP(self):
        cmd = 'CDUP'
        return cmd, self.sendCommand(cmd)

    @catchError(IOError)
    def PWD(self):
        cmd = 'PWD'
        return cmd, self.sendCommand(cmd)

    @catchError(IOError)
    def LIST(self, name):
        if self.is_pasv:
            if not self.data_sock:
                return None, 'No data connection'
        cmd = 'LIST'
        if isinstance(name, list) or isinstance(name, tuple):
            cmd = cmd + ' '.join(name)
        elif isinstance(name, str):
            cmd = cmd + ' ' + name
        res150 = self.sendCommand(cmd)
        if not self.is_pasv:
            self.data_sock = self.data_listen_sock.accept()[0]
            self.data_listen_sock.close()
            self.data_listen_sock = None
        lines = []
        with self.data_sock.makefile('r', encoding='UTF-8') as f:
            while True:
                line = f.readline(BUF_SIZE)
                if not line:
                    break
                if line[-2:] == '\r\n':
                    line = line[:-2]
                elif line[-1:] in ['\r', '\n']:
                    line = line[:-1]
                lines.append(line)
        res226 = self.getResponse()
        self.is_pasv = None
        self.data_sock.close()
        self.data_sock = None
        return cmd, (res150, res226), lines

    @catchError(IOError)
    def RMD(self, dir_name):
        cmd = 'RMD {}'.format(dir_name)
        return cmd, self.sendCommand(cmd)

    @catchError(IOError)
    def RNFR(self, old_name):
        cmd = 'RNFR {}'.format(old_name)
        return cmd, self.sendCommand(cmd)

    @catchError(IOError)
    def RNTO(self, new_name):
        cmd = 'RNTO {}'.format(new_name)
        return cmd, self.sendCommand(cmd)

    # 发送指令并返回response
    def sendCommand(self, cmd):
        self.sendRequest(cmd)
        return self.getResponse()

    # 获取多行response
    def getResponse(self):
        line = self.parseSingleLine()
        if line[3:4] == '-':
            code = line[:3]
            while True:
                nextline = self.parseSingleLine()
                line = line + '\n' + nextline
                if nextline[:3] == code:
                    if nextline[3:4] == ' ':
                        break
        return line

    # 接受一行response并取出换行符
    def parseSingleLine(self):
        line = self.file.readline(BUF_SIZE + 1)
        if not line:
            raise EOFError
        if line[-2:] == '\r\n':
            line = line[:-2]
        elif line[-1:] in ['\r', '\n']:
            line = line[:-1]
        return line

    def sendRequest(self, req):
        req = req.strip() + '\r\n'
        self.ctrl_sock.sendall(req.encode())
