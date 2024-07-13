#include <conio.h>
#include <dos.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <i86.h>
#include "pm_lib.h"

#define PG_CONTROL_PORT 0x1D0
#define DATA_PORT_LOW  0x1D1
#define DATA_PORT_HIGH 0x1D2
#define PICOGUS_PROTOCOL_VER 2

void banner(void) {
    printf("PicoMEM Init Rev 0.1 by FreddyV, 2024\n\n");
}

void usage()
    {
    // Max line length @ 80 chars:
    //              "................................................................................\n"
    fprintf(stderr, "Parameters:\n");
    fprintf(stderr, "    /?        - Show this message\n");
    fprintf(stderr, "    /k        - Reinstall the Key Shortcut interrupt\n");    
    fprintf(stderr, "    /diag     - Start in Diagnostic Mode\n");
    fprintf(stderr, "    /j        - Enable USB joystick support\n");
    //              "................................................................................\n"
    }

void err_ultrasnd(char *argv0) {
    fprintf(stderr, "ERROR: no ULTRASND variable set or is malformed!\n");
}

void print_firmware_string(void) {
    outp(PG_CONTROL_PORT, 0xCC); // Knock on the door...
    outp(PG_CONTROL_PORT, 0x02); // Select firmware string register

    char firmware_string[256] = {0};
    for (int i = 0; i < 255; ++i) {
        firmware_string[i] = inp(DATA_PORT_HIGH);
        if (!firmware_string[i]) {
            break;
        }
    }
    printf("Firmware version: %s\n", firmware_string);
}

bool wait_for_read(uint8_t value) {
    for (uint32_t i = 0; i < 1000000; ++i) {
        if (inp(DATA_PORT_HIGH) == value) {
            return true;
        }
    }
    return false;
}

bool detect_picogus(void)
{
 // Get magic value from port on PicoGUS that is not on real GUS
 outp(PG_CONTROL_PORT, 0xCC); // Knock on the door...
 outp(PG_CONTROL_PORT, 0x00); // Select magic string register
 if (inp(DATA_PORT_HIGH) != 0xDD) {
        return false;
    };
 printf("PicoGUS detected: ");
 print_firmware_string();
 return true;
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

// Read the arguments
    int i = 1;
    while (i < argc) 
     {
        if (stricmp(argv[i], "/?") == 0) {
            usage();
            return 0;
        } else if (stricmp(argv[i], "/k") == 0) {
            printf(">Install the key shortcut. (Ctrl Shift F1/F2)\n");
            pm_irq_cmd(0x12);
            return 0;
        } else if (stricmp(argv[i], "/diag") == 0) {
            printf(">Diagnostic mode:\n");
            pm_diag();
            return 1;
        }  else if (stricmp(argv[i], "/a") == 0) {
            if (i + 1 >= argc) {
                usage();
                return 255;
            }
 /*           e = sscanf(argv[++i], "%hu", &buffer_size); 
            if (e != 1 || buffer_size < 1 || buffer_size > 256) {
                usage(); 
                return 3;
            } */
        }
        ++i;
     }
//    printf("PicoMEM Init End.\n");
    return 0;
}
