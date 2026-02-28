# Shell-ish
PART 1:
-Execute commands using PATH
- Built-in commands : cd, exit
- BAckground execution &

Usage
Compile:
gcc -o shellish shellish-skeleton.c

Run:
./shellish

## Part 2 – I/O Redirection and Piping

### I/O Redirection
Supported redirections (no space between symbol and filename):
- `<inputfile`  : redirect stdin from file
- `>outputfile` : redirect stdout to file (create if not exists, truncate if exists)
- `>>outputfile`: redirect stdout to file (create if not exists, append if exists)

Example tests:
- `echo hello >out.txt` then `cat <out.txt`
- `echo line2 >>a.txt` then `cat <a.txt`
- `wc -w <a.txt`

### Piping
Supports a single pipe between two commands:
- `cmd1 | cmd2`

Example tests:
- `ls -la | wc -l`
- `echo hello | wc -c`
- Combined: `ls -la | wc -l >count.txt`


## Part 3A – Built-in `cut`

Implemented a built-in version of `cut` that works in pipelines (reads from `stdin`, writes to `stdout`).

### Supported options
- `-f <list>` : required, comma-separated 1-based field indices (e.g. `1,3,6`)
- `-d <char>` : optional delimiter (single character). Default delimiter is TAB (`\t`)

Notes:
- Reads input line-by-line from `stdin`.
- Preserves empty fields (e.g. `a::b` with `-d : -f 2` prints an empty field).
- Works with the existing single-pipe implementation (`cmd1 | cut ...`).
- Output redirection still works after piping (e.g. `cmd1 | cut ... >out.txt`).

### Example tests
- Tab-delimited:
  - `printf "a\tb\tc\n1\t2\t3\n" | cut -f 1,3`
- Custom delimiter:
  - `printf "root:x:0:0:root:/root:/bin/bash\n" | cut -d : -f 1,6`
  - `printf "a::b\n" | cut -d : -f 1,2,3`
- With redirection:
  - `printf "a:b:c\n1:2:3\n" | cut -d : -f 2 >out.txt`
  - `printf "X:Y:Z\n" | cut -d : -f 1 >>out.txt`


## Part 3(b) – Chatroom (Named Pipes)

Implemented a simple multi-user chat command using named pipes (FIFO).

Usage:
./chatroom <roomname> <username>

Behavior:
- Each chat room is represented as a directory under:
  /tmp/chatroom-<roomname>/
- Each user is represented by a named pipe inside the room directory:
  /tmp/chatroom-<roomname>/<username>

Features:
- If the room does not exist, it is created automatically
- If the user pipe does not exist, it is created automatically
- A user continuously reads from their own named pipe
- Messages typed by a user are broadcast to all other users in the room
- Messages are displayed in the format:
  [roomname] username: message

Example:
Terminal 1:
./chatroom comp304 ali

Terminal 2:
./chatroom comp304 mehmet

ali types:
hello

mehmet sees:
[comp304] ali: hello
