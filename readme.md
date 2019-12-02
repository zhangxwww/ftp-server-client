# FTP Server/Client

## Functionalities

- Support FTP commands including USER, PASS, PASV, PORT, RETR, STOR, REST, SYST, TYPE, QUIT, MKD, CWD, PWD, RMD, CDUP, LIST, REFR, RETO.
- Multiple clients with a single server.
- Transfer large files without blocking the server.
- Resume transmitting after connection terminated.

## Build

```bash
cd server
make
```

## How to run

### Server

```bash
cd server
./server [-port] [-root]
```

### Client

```bash
cd client
py client.py
```

