# Milestone 5 Verification Checklist

Based on Handout(2).pdf sessions 17–20.

| Requirement | Implementation |
|---|---|
| jobs built-in | `jobs.c` / `executor.c` |
| List active jobs and display status | `jobs_print()` |
| Job IDs | job table in `jobs.c` |
| Foreground control with fg | `jobs_fg()` |
| Transfer terminal control | `tcsetpgrp()` |
| Wait for foreground job | `waitpid()` with process group |
| Background resumption with bg | `jobs_bg()` + `SIGCONT` |
| Process groups | `setpgid()` |
| SIGCHLD | `sigaction()` handler + `jobs_reap()` |
| Ctrl-Z / stopped jobs | `SIGTSTP`, `WUNTRACED` |
| Continue stopped jobs | `SIGCONT` |
| Maintain job states | RUNNING / STOPPED / DONE |

The handout places sessions 17–20 under CO5: `jobs`/`fg`, `bg` and signal fundamentals, `sigaction()`, and Ctrl-Z/process suspension. Session 21 onward is CO6 integration and is outside this Milestone 5 package.
