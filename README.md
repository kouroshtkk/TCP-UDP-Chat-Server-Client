# TCP/UDP Chat Server

A multithreaded chat server and client written in C. Communication is split across two transports: TCP handles commands and session control, while UDP delivers lightweight real-time notifications. Up to 100 clients can be connected simultaneously.

## How it works

When a client connects, it authenticates over TCP and binds a local UDP port for receiving notifications. From that point on, all commands (listing users, sending messages, managing friends) go over TCP. Whenever an event is queued for a user — a friend request, a message, a flood — the server sends a small 3-byte UDP packet to alert them. The client then polls its stream with `CONSU` to retrieve the actual payload over TCP.

The server maintains a global client table protected by a single mutex. Each client has a linked list of pending stream events (`Flow`) and an unread counter. The UDP notification encodes the event type and the current unread count in hex.

## Features

- Register and log in with an 8-character ID and a numeric password
- List currently online users
- Send friend requests and accept or reject them interactively
- Direct messages between friends
- Flood messages that propagate recursively through the friend graph
- UDP notifications with event type and unread stream count
- Clean disconnect with `IQUIT`

## Protocol

All TCP packets are terminated with `+++`. Commands are fixed 5-character codes:

| Command | Direction | Description |
|---------|-----------|-------------|
| `REGIS` | client -> server | Register a new account |
| `CONNE` | client -> server | Log in to an existing account |
| `LIST?` | client -> server | Request list of online users |
| `FRIE?` | client -> server | Send a friend request |
| `MESS?` | client -> server | Send a direct message to a friend |
| `FLOO?` | client -> server | Send a flood message across friend graph |
| `CONSU` | client -> server | Consume the next pending stream event |
| `IQUIT` | client -> server | Disconnect gracefully |

Server responses include `WELCO`, `HELLO`, `GOBYE`, `RLIST`, `LINUM`, `FRIE>`, `FRIE<`, `MESS>`, `MESS<`, `FLOO>`, `EIRF>`, `FRIEN`, `NOFRI`, `SSEM>`, `OOLF>`, `NOCON`.

UDP packets are 3 bytes: `[type][count_low][count_high]`, where type is an ASCII digit (0-4) indicating the event category and count is the unread stream count encoded as a 2-digit hex value.

## Building

Requires GCC and pthreads. Binaries are placed in `bin/`.

```sh
make
```

To clean and rebuild:

```sh
make rebuild
```

## Running

Start the server on a port (must be 4 digits or fewer):

```sh
./bin/server 8080
```

Connect a client by passing the server IP and TCP port:

```sh
./bin/client 127.0.0.1 8080
```

On startup the client prompts for `REGIS` or `CONNE`. Registration requires an 8-character ID, a UDP port to bind locally, and a numeric password. Login requires the ID, password, and the same UDP port used at registration.

## Limitations

- User data is held entirely in memory. Registered accounts do not survive a server restart.
- The server port is capped at 9999.
- Messages are limited to 200 characters.
- The server supports a maximum of 100 simultaneous clients.
- There is no encryption on any transport.
