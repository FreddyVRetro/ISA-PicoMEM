/*
 * part of ethersrv-linux
 *
 * Copyright (C) 2017 Mateusz Viste
 */

#ifndef LOCK_H_SENTINEL
#define LOCK_H_SENTINEL

/* acquire the lock file */
int lockme(char *lockfile);

/* release the lock file */
void unlockme(char *lockfile);

#endif
