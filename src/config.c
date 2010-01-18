/* vim: ts=4 sw=4 sts=4 sta tw=80 list
 *
 * Copyright (C) 2009, Tobias Kieslich. All rights reseved.
 * See Copyright Notice in parcle.h
 *
 * read the configuration file and command line arguments; then
 * set global variables
 *
 */
#include <pthread.h>            /* mutexes, conditions */

#include "parcle.h"


const char * const   _Server_version = "testserver/poc";

pthread_mutex_t wake_worker_mutex  = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pull_job_mutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  wake_worker_cond   = PTHREAD_COND_INITIALIZER;

