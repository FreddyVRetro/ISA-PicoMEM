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

// List of authorized port for validation.
#define CMS_PORTS 6
#define TDY_PORTS 5
#define SB_PORTS 5
uint16_t CMSPortList[] = {0,1,0x220,0x230,0x240,0x250};
uint16_t TDYPortList[] = {0,1,0x1E0,0x2C0,0x0C0};
uint16_t SBPortList[] = {0,1,0x220,0x230,0x240,0x250};

void banner(void) {
    printf("PicoMEM Init Rev 0.3 by FreddyV, 12/2024\n");
}

void usage()
    {
    // Max line length @ 80 chars:
    //              "................................................................................\n"
    fprintf(stderr, "Parameters:\n");
    fprintf(stderr, "    /?        - Show this message\n");
    fprintf(stderr, "    /k        - Reinstall the Key Shortcut interrupt\n");
    fprintf(stderr, "    /adlib x  - Enable/Disable the Adlib output (0:Off 1:On)\n"); 
    fprintf(stderr, "    /cms x    - Enable/Disable the CMS output   (0:Off 1:220 x:Port)\n");    
    fprintf(stderr, "    /tdy x    - Enable/Disable the Tandy output (0:Off 1:2C0 x:Port)\n");
    fprintf(stderr, "    /j x      - Enable/Disable the Joystick (0:Off 1:On)\n");
    fprintf(stderr, "    /diag     - Start in Diagnostic Mode\n");
    //              "................................................................................\n"
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

bool check_valid_w(uint16_t *table, uint16_t value, uint8_t size)
{
 for (int i=0;i<size;i++)
     if (table[i]==value) return true;
return false;
}

#define process_port_opt(option) \
if (i + 1 >= argc) { \
    usage();    \
    return 255; \
} \
e = sscanf(argv[++i], "%hx", &option); \
if (e != 1 || option > 0x3ffu) { \
    usage();  \
    return 4; \
}

int main(int argc, char* argv[]) {
    int e;

    banner();
    if (argc<=1)
      {
        usage();
        return 1;
      }

    pm_detect();

// Read the arguments
    uint16_t tmp_uint16;
    uint8_t tmp_uint8;
    int i = 1;
    while (i < argc) 
     {
        if (stricmp(argv[i], "/?") == 0) {          // Help
            usage();
            return 0;
        } else if (stricmp(argv[i], "/k") == 0) {   // Shortcut
            if (PM_BIOS_IRQ_Present)
              {
               printf(" - Install the key shortcut. (Ctrl Shift F1/F2)\n");
               pm_irq_cmd(0x12);
              }
              else {printf(" > Error: PicoMEM BIOS Needed to enable the shortcut.\n");}
            return 0;
        } else if (stricmp(argv[i], "/diag") == 0) {    // Diagnostic
            printf(" - Diagnostic mode:\n");
            pm_diag();
            return 1;
        }  else if (stricmp(argv[i], "/adlib") == 0) {    // Tandy
            if (i + 1 >= argc) {
                usage();
                return 255;
            }
            process_port_opt(tmp_uint16);
            printf(" - Adlib : ");
            if ((tmp_uint16==0)||(tmp_uint16==1))
               {
                if (tmp_uint16==0) printf("Disable\n");
                  else printf("Enable (388)\n");

                pm_io_cmd(CMD_AdlibOnOff,tmp_uint16);
               } else printf("Use only 0 or 1\n");               
        } else if (stricmp(argv[i], "/j") == 0) {    // Tandy
            if (i + 1 >= argc) {
                usage();
                return 255;
            }
            process_port_opt(tmp_uint16);
            printf(" - Joystick : ");
            if (tmp_uint16==0) printf("Disable\n");
                  else printf("Enable (%x)\n",tmp_uint16);
            pm_io_cmd(CMD_Joy_OnOff,tmp_uint16);

        }else if (stricmp(argv[i], "/cms") == 0) {    // CMS
            if (i + 1 >= argc) {
                usage();
                return 255;
            }
            process_port_opt(tmp_uint16);
            printf(" - CMS : ");
            if (check_valid_w(&CMSPortList[0],tmp_uint16,CMS_PORTS))
               {
                if (tmp_uint16==0) printf("Disable\n");
                  else printf("Enable (%x)\n",tmp_uint16);
                 
                 pm_io_cmd(CMD_CMSOnOff,tmp_uint16);
               } else printf("Invalid port (%x)\n",tmp_uint16);
        }else if (stricmp(argv[i], "/tdy") == 0) {    // CMS
            if (i + 1 >= argc) {
                usage();
                return 255;
            }
            process_port_opt(tmp_uint16);
            printf(" - Tandy : ");
            if (check_valid_w(&TDYPortList[0],tmp_uint16,TDY_PORTS))
               {
                if (tmp_uint16==0) printf("Disable\n");
                  else printf("Enable (%x)\n",tmp_uint16);
                 
                 pm_io_cmd(CMD_TDYOnOff,tmp_uint16);
               } else printf("Invalid port (%x)\n",tmp_uint16);
        }
        ++i;
     }
//    printf("PicoMEM Init End.\n");
    return 0;
}

 /*           e = sscanf(argv[++i], "%hu", &buffer_size); 
            if (e != 1 || buffer_size < 1 || buffer_size > 256) {
                usage(); 
                return 3;
            } */
