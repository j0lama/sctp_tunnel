# SCTP Tunneling
SCTP is not a common protocol; therefore, many cloud services and middleboxes do not support this protocol and delete all the packets. A solution for that is to create a tunnel between both SCTP endpoints with an accepted Internet protocol such as TCP or UDP. The tunnel inserts SCPT bidirectional messages in the data field of the tunnel protocol. Also, this implementation can be interpreted as two connected proxies that forward all SCTP traffic to the other proxy and then forward it to the SCTP server/client.

```
 ______              _____                      _____              ______
|      |    SCTP    |     |     TCP Tunnel     |     |    SCTP    |      |
|      | <--------> |     | <================> |     | <--------> |      |
|      |            |     |                    |     |            |      |
 ¯¯¯¯¯¯              ¯¯¯¯¯                      ¯¯¯¯¯              ¯¯¯¯¯¯
  SCTP               Tunnel                     Tunnel              SCTP
 Client              Client                     Server             Server
```

## TCP tunnel for SCTP Protocol
This repository contains a client-server light-weight application implemented in C that uses a TCP flow as a tunnel for SCTP traffic. To keep the coherence in SCTP connection outside the tunnel, the *struct sctp_sndrcvinfo* parameters are sent through the tunnel in the first 32 bytes of every TCP message to reconstruct the SCTP original tunnel in the other tunnel side.


### Build
To build the project use:
```
./build
```

### How to use

Before run SCTP Client, both parts of the tunnel have to be running.

#### Run Tunnel Client
```
./client <TUNNEL_SERVER_IP> <TUNNEL_SERVER_PORT> <SCTP_IP> <SCTP_PORT>
```
Where:
- *TUNNEL_SERVER_IP*: Tunnel Server IP
- *TUNNEL_SERVER_PORT*: Port in which Tunnel Server is listening for TCP connections.
- *SCTP_IP*: Is the IP where the Tunnel Client is listening upcomming SCTP client connections.
- *SCTP_PORT*: Port in which Tunnel Client receives SCTP messages from the SCTP Client (Same as SCTP Server Port).

#### Run Tunnel Server
```
./server <TUNNEL_IP> <TUNNEL_PORT> <SCTP_SERVER_IP> <SCTP_SERVER_PORT>
```
Where:
- *TUNNEL_IP*: Tunnel Server IP
- *TUNNEL_PORT*: Port in which Tunnel Server is listening for TCP connections.
- *SCTP_SERVER_IP*: SCTP Server IP
- *SCTP_SERVER_PORT*: Port in which SCTP Server receives SCTP messages.

#### Problems
List of known problems:
- Tunnel Server does not close properly when a SIGINT signal is received because it is blocked in *sctp_recvmsg* function.
- Buffer length is 1024 by default and long packets could be truncated.
