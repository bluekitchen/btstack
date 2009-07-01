/*
 *  main.c
 * 
 *  Simple tests
 * 
 *  Created by Matthias Ringwald on 4/29/09.
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "daemon.h"

int main (int argc, const char * argv[]) {

    // start daemon
    daemon_main(argc, argv);

    // daemon does not returs so far
    return 0;
}
