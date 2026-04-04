#!/usr/bin/env bash
set -euo pipefail

gcc -std=c99 -Wall -Wextra -O2 -o /tmp/level1_exec /home/aaditya-shah/hackrush26-embedded-memory-manager/level1/level1.c
gcc -std=c99 -Wall -Wextra -O2 -o /tmp/level2_exec /home/aaditya-shah/hackrush26-embedded-memory-manager/level2/level2.c
gcc -std=c99 -Wall -Wextra -O2 -o /tmp/level3_exec /home/aaditya-shah/hackrush26-embedded-memory-manager/level3/level3.c
gcc -std=c99 -Wall -Wextra -O2 -o /tmp/level4_exec /home/aaditya-shah/hackrush26-embedded-memory-manager/level4/level4.c
gcc -std=c99 -Wall -Wextra -O2 -o /tmp/bonus_exec  /home/aaditya-shah/hackrush26-embedded-memory-manager/bonus/bonus.c

/tmp/level1_exec
/tmp/level2_exec
/tmp/level3_exec
/tmp/level4_exec
/tmp/bonus_exec
