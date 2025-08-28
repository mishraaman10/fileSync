# FileSync: A Tool for Efficiently Transfering Files Over a Network

It can transfer files efficiently. It can also create new file if the provided path is a directory. It works on block level so only changes are transefred to the destination

## 🛠️ Build Instructions

### Prerequisites
- C++17 or later
- CMake (version 3.10+)
- g++ / clang++
- Linux (WSL or native)



### 🔧 Build Steps

```bash
# Clone the repository
git clone (https://github.com/mishraaman10/fileSync.git)
cd filesync

mkdir build && cd build

cmake ..
make
```

### 🚀 Run Instructions

### 🖥️ Start the Server (Remote Endpoint)
- 🛰️ The server simply listens and responds to incoming client requests.
```bash
./syncApp server
# Waits for incoming connections from clients, Handles storage, file retrieval, and synchronization requests.
```

### 🖥️ Start the Client
- 🧭 The client is the controller and must initiate all operations (connect, push, pull, etc.).
```bash
./syncApp client

## use cases for client
```bash
```bash
connect <ip> <port>                            Connect to the server (supports up to 3 concurrent sessions) for same device use (connect 127.0.0.1 8080)
push <session_id> <local_path> <remote_path>   Push the local file to the server, efficiently overwriting the remote file
pull <session_id> <remote_path> <local_path>   Pull the remote file from the server, efficiently overwriting the local file
disconnect <session_id>                        Terminate the specified session with the server
list                                           View all active session IDs with their connection details
help                                           Display all supported client commands
exit                                           Exit the client application gracefully


```
<img width="1185" height="238" alt="image" src="https://github.com/user-attachments/assets/79b9ca5b-aa11-403e-9dd3-f764da8cdfdb" />



#Use Cases

For Push : 
- If both remote path and local path are files then data of the local path will be written in the remote path.
- If the remote path is a directory then a new file with same name will be created into the directory.

For Pull 
- If both remote path and local path are files then data of the local path will be written in the remote path.
- If the remote path is a directory then a new file with .temp name will be created into that directory.

Request Format

client side

<img width="1417" height="463" alt="image" src="https://github.com/user-attachments/assets/2e142ce2-3bd5-4acb-a731-49a35ce24aae" />


Server side response 
<img width="1466" height="319" alt="image" src="https://github.com/user-attachments/assets/1edc1ecd-d589-486d-b9ac-e358998d7700" />




## 🧠 Sync Strategy: Rolling Hash + Delta Encoding

FileSync minimizes data transfer using a block-based sync strategy, combining:

- **Rolling Hash (Weak)**: Fast window-based detection (O(1) update per shift).
- **Strong Hash (MD5)**: Used to confirm exact block matches and avoid collisions.

---
1. **Destination** divides its file into blocks (e.g, 128 KB) and sends `[rolling_hash, strong_hash]` for each block.
2. **Source** slides an window (of size exactly equal to block size used by destination) over its file:
   - Computes rolling hash at each offset.
   - Checks for match in destination's weak hash set.
   - If match found, verifies with strong hash (MD5).
3. Based on comparisons delta instructions are formed and they are of two type:
   - Copy `offset_of_block`
   - Insert `data`
4. Destination reconstructs file using delta instructions

---

## 🌐 Network Protocol

FileSync uses a lightweight, reliable, and clearly structured TCP protocol between the source and destination. The entire sync session flows through these phases:

### 🔗 1. Handshake & Version Check
Both sides exchange a magic identifier and protocol version to verify compatibility.

### 📢 2. Command Declaration
The source declares its intent (`PUSH` or `PULL`) so the destination can follow the correct workflow.

### 📂 3. File Path Exchange
Source sends the file path. Destination acknowledges readiness for hash or data exchange.

### 📊 4. Hash Blueprint Transfer
The side with the latest file sends a compact list of block hashes (rolling + strong).  
The other side uses this to determine which blocks are already synchronized.

### 🧩 5. Delta Instruction Stream
A stream of `CopyBlock` and `InsertData` instructions is sent to reconstruct the target file with minimal data.











