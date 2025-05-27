#include <conio.h>
#include <dos.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <i86.h>
#include "pm_lib.h"

bool SaveCFG=false;

void banner(void) {
    printf("PicoMEM Wifi Rev 0.1 by FreddyV, 2025\n\n");
}

void usage()
    {
    // Max line length @ 80 chars:
    //              "................................................................................\n"
    fprintf(stderr, "PMWIFI SSID PASSWORD\n");
    fprintf(stderr, " Usage: PMWIFI /id SSID /pw PASSWORD\n");
    fprintf(stderr, " Parameter:\n");
    fprintf(stderr, "     /id xxx   - Set the SSID\n");
    fprintf(stderr, "     /pw xxx   - Set the password\n");
    fprintf(stderr, "     /s        - Save the SSID/Password to wifi.txt\n");
    fprintf(stderr, "     /off      - Disable the ne2000\n");
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
