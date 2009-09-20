//
//  SpringBoardAccess-test.c
//
//  Created by Matthias Ringwald on 9/15/09.
//
#include "SpringBoardAccess.h"

#include <string.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
	int usage = 1;
	
	if (argc == 3) {
		if (strcmp("add", argv[1]) == 0) {
			SBA_addStatusBarImage(argv[2]);
			usage = 0;
		} else if (strcmp("remove", argv[1]) == 0) {
			SBA_removeStatusBarImage(argv[2]);
			usage = 0;
		}
	}

	if (usage) {
		printf("Usage: %s add/remove StatuBarImageName", argv[0]);
		return -1;
	} 
}

