/*
 * part of ethersrv-linux
 * http://etherdfs.sourceforge.net
 *
 * Copyright (C) 2017 Mateusz Viste
 */

#include <stdio.h>
#include <unistd.h>

#include "lock.h"

/* acquire the lock file */
int lockme(char *lockfile) {
  FILE *fd;
  /* does the file exist? */
  fd = fopen(lockfile, "rb");
  if (fd != NULL) {
    fclose(fd);
    return(-1);
  }
  /* file doesn't seem to exist - create it then */
  fd = fopen(lockfile, "wb");
  if (fd == NULL) return(-1);
  fclose(fd);
  return(0);
}

/* release the lock file */
void unlockme(char *lockfile) {
  unlink(lockfile);
}
