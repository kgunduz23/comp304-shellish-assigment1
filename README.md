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
