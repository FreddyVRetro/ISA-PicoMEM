#include <conio.h>
#include <dos.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <i86.h>
#include "pm_lib.h"

void banner(void) {
    printf("PicoMEM xxx Rev 0.1 by FreddyV, 2024\n\n");
}

void usage()
    {
    // Max line length @ 80 chars:
    //              "................................................................................\n"
    fprintf(stderr, " PMWIFI SSID PASSWORD\n");
    fprintf(stderr, "Parameter:\n");
    fprintf(stderr, "    /s        - Save the SSID/Password to wifi.txt\n");
    //              "................................................................................\n"
    }

bool detect_picomem(void)
{
    pm_detectport(100);  
    return(true);
}

int main(int argc, char* argv[]) {
    int e;

    banner();
    pm_detect();

    if (detect_picomem())
    {
    if (argc<2)  
       { 
         usage();
         return 2;
       }
    int i = 1;
    while (i < argc) {
        fprintf("i:%d %s",i,argv[i]);
        if (stricmp(argv[i], "/?") == 0) {
            usage();
            return 0;
        }
        }
        ++i;
    }

    } else 
    { //PicoMEM Not Detected
      return 1;
    }

//    printf("PicoMEM Init End.\n");
    return 0;
}
