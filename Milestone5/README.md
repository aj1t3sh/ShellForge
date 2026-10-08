# ShellForge — Milestone 5

This milestone extends the Milestone 4 shell with the handout's CO5 job-control requirements:

- `jobs` — list active/stopped jobs
- `fg` / `fg %N` — bring a job to the foreground
- `bg` / `bg %N` — resume a stopped job in the background
- background jobs with `&`
- process groups using `setpgid()`
- terminal ownership using `tcsetpgrp()`
- `SIGCHLD` handling through `sigaction()`
- stopped jobs using `SIGTSTP` / Ctrl-Z
- resumed jobs using `SIGCONT`
- foreground waiting with `waitpid(..., WUNTRACED)`
- pipelines remain supported from Milestone 4
- redirection remains supported from Milestone 4

## Build on Ubuntu

```bash
sudo apt update
sudo apt install build-essential libreadline-dev
make
./shellforge
```

## Test Milestone 5

```text
sleep 20 &
jobs
fg %1
```

Press **Ctrl-Z** while the foreground `sleep` is running, then:

```text
jobs
bg %1
jobs
```

Also test:

```text
sleep 5 &
echo hello | tr a-z A-Z
ls > out.txt
cat out.txt
```

## Notes

Job-control behavior is intended for an interactive terminal. The shell ignores interactive terminal signals itself and places child processes into their own process groups so Ctrl-C/Ctrl-Z affect the foreground job rather than the shell.
