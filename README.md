# Reverse UDP Tunnel

## Overview
Running a WireGuard instance behind a Carrier-Grade NAT (CG-NAT) makes it difficult for external clients to connect to the instance. This is because there is no **preconfigured** route between the **public endpoint** of the home network and the WireGuard server.

This issue can be solved using **NAT hole punching**, where the server behind the NAT periodically sends packets to a **publicly accessible relay server**. The relay server then learns the **IP/PORT** mapping of the incoming connection and can use it to forward traffic back to the device behind NAT.

*This implementation is inspired by [prof7bit's](https://github.com/prof7bit/udp-reverse-tunnel) similar approach.*

## Security Considerations

### 1. State Exhaustion Attack
#### Problem:
For every incoming connection to the public server, a new tunnel is allocated, meaning the inside server opens a socket. If incoming traffic is not verified, an attacker could launch a **state-exhaustion attack** by repeatedly connecting from different IP addresses, forcing the inside server to create excessive tunnels.

#### Mitigation:
Use the `-n` option on the **outside server** to set a **maximum limit** on simultaneous open tunnels. However, note that an attacker could still consume all available slots and block legitimate users.

### 2. Session Hijacking
#### Problem:
An attacker could **spoof** or **replay** keepalive signals to the outside server, causing the server to allocate a tunnel for the attacker's IP. If successful, traffic meant for the legitimate endpoint would be forwarded to the attacker.

#### Mitigation:
Use the `-k` option to set a **shared secret** for authenticating keepalive messages. Each keepalive message includes:
- A **HMAC** (hash-based message authentication code) over the secret key
- A **strictly increasing nonce** to prevent replay attacks

### 3. Encryption
The tunnel itself **does not encrypt** traffic. It assumes that the application using the tunnel (e.g., WireGuard) provides its own encryption.

## Setup

### 1. Clone and Build
```bash
# Clone the repository
git clone git@github.com:svensglinz/reverse_udp_tunnel.git
cd reverse_udp_tunnel

# Compile the binary
make
```

**Dependencies:** The project requires OpenSSL for HMAC authentication.

### 2. Running the Tunnel
#### Inside Agent (Behind NAT)
```bash
./reverse-udp-tunnel -s localhost:51820 -o outside.server.com:1234 -k "mysecret" -n 100
```
This command sets up the **inside agent** that:
- Listens on `localhost:51820`
- Periodically pings the outside relay at `outside.server.com:1234`
- Uses `"mysecret"` to authenticate keepalive messages
- allows up to `100` simultaneous connections (`-n 100`)
- 
#### Outside Relay (Publicly Accessible Server)
```bash
./reverse-udp-tunnel -l 1234 -n 100 -k "mysecret"
```
This command starts the **outside relay** that:
- Listens for incoming connections on port `1234`
- Allows up to `100` simultaneous connections (`-n 100`)
- Uses `"mysecret"` for authentication

## Command-Line Options

All options labeled Inside/Outside should have the same values on both the inside and outside agents.

| Option | Description                                                           |
|--------|-----------------------------------------------------------------------|
| `-s HOST:PORT` | Inside: Address of the internal service (e.g., `localhost:51820`)     |
| `-o HOST:PORT` | Inside: Public relay server address (e.g., `outside.server.com:1234`) |
| `-l PORT` | Outside: Port to listen for connections (e.g., `-l 1234`)             |
| `-n NUMBER` | Inside/Outside: Max open tunnels (default: 10)                        |
| `-k SECRET` | Inside/Outside: Secret key for keepalive authentication               |
| `--keepaliveInterval SECONDS` | Interval for keepalive packets (default: 25s)                         |
| `--connectionTimeout SECONDS` | Inside: Timeout for UDP sockets (default: 60s)                        |
| `--logLevel LEVEL` | Logging verbosity (default: 2)                                        |

## Implementation Details

## Implementation Details

- **At startup**, the inside client opens a socket and sends a keepalive message to the outside client.  
  The outside client registers this connection as a spare tunnel.

- **When an external client connects** to the outside client, it is mapped to this spare tunnel, which will relay all further traffic from the client to the inside service.

- **Upon receiving the first packet** through the tunnel, the inside service opens another socket and sends a keepalive message to the outside service.  
  This new connection is registered as the next spare tunnel to be assigned to future clients.

- To enable fast lookups of client-to-tunnel mappings, even with many active tunnels, a **hashmap** is used.
  
## Tunnel Lifecycle & Timeout Handling

- If the inside client does not receive any traffic over a tunnel for at least `connectionTimeout * (±20%)` seconds, it will close the socket.
- If the outside server does not receive a keepalive message from an inside tunnel for `2 × keepaliveInterval` seconds, it will remove the client-to-tunnel mapping.

### ⚠ Potential Issue:
> A tunnel may be closed by the inside client up to `2 × keepaliveInterval` seconds before it is removed on the outside client.  
> If a client reconnects before the outside client removes the mapping, traffic may be sent to a closed tunnel, causing a connection failure.

To prevent this, **avoid reconnecting** with the same IP/Port combination immediately after a timeout.

For wireguard, use `persistentKeepalive =` to ensure that a connection does not timeout (more information [here](https://www.wireguard.com/quickstart/#nat-and-firewall-traversal-persistence)) 
...

## Notes
- Adjust the `--keepaliveInterval` as needed to prevent NAT mappings from expiring.
- This tunnel does **not** provide encryption—use WireGuard or another encrypted protocol over it.
- NAT hole punching relies on routers keeping NAT mappings alive. If your router aggressively removes them, lower the keepalive interval.

---
