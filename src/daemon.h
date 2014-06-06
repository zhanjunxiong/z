#ifndef DAEMON_H_
#define DAEMON_H_

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int daemonize(int nochdir, int noclose);

#endif
