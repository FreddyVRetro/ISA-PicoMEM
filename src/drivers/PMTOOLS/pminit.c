#include <conio.h>
#include <dos.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <i86.h>
#include "pm_lib.h"

/*

1.0.1 : Add TDY Port 300, Add SB Port 250

*/

#define PICOMEM2 1

// To detect also PicoGUS
#define PG_CONTROL_PORT 0x1D0
#define DATA_PORT_LOW  0x1D1
#define DATA_PORT_HIGH 0x1D2
#define PICOGUS_PROTOCOL_VER 2

// List of authorized port for validation.
#define CMS_PORTS 8
#define TDY_PORTS 6
#define SB_PORTS 6
#define MMB_PORTS 7
#define CVX_PORTS 4
#define MPU_PORTS 7
uint16_t CMSPortList[] = {0,1,0x210,0x220,0x230,0x240,0x250,0x260};
uint16_t TDYPortList[] = {0,1,0x1E0,0x2C0,0x0C0,0x300};
uint16_t SBPortList[]  = {0,1,0x220,0x230,0x240,0x250};
uint16_t MMBPortList[] = {0,1,0x220,0x2F0,0x300,0x310,0x320};
uint16_t CVXPortList[] = {0,1,2,0x300};
uint16_t STEPortList[] = {0,1,2};                                   // Covox Stereo
uint16_t MPUPortList[] = {0,1,0x300,0x320,0x330,0x340,0x360};
// DOTT : 330 332 334 336
// DOOM : 220 230 240 250 300 320 330 332 334 336 340 360 

typedef struct sb_conf {
  uint16_t port;
  uint8_t irq;
  uint8_t dma;
  uint8_t type;
} sb_conf_t;

sb_conf_t sbconf;

typedef struct gus_conf {
  uint16_t port;
  uint8_t irq;
  uint8_t irq2;
  uint8_t dma;
  uint8_t dma2;
} gus_conf_t;

gus_conf_t gusconf;

void banner(void) {
    printf("PicoMEM Init Rev 1.0.2 by FreddyV, 06/2026\n");
}

void usage()
    {
    // Max line length @ 80 chars:
    //              "................................................................................\n"
    fprintf(stderr, "Parameters:\n");
//    fprintf(stderr, "    /?       - Show this message\n");
    fprintf(stderr, "    /k       - Reinstall the Key Shortcut interrupt\n");
    fprintf(stderr, "    /j x     - Joystick          (0:Off 1:On)\n");
    fprintf(stderr, "    /adlib x - Adlib sound       (0:Off 1:On)\n"); 
    fprintf(stderr, "    /cms x   - CMS sound         (0:Off 1:220 or 210,230,240,250)\n"); 
    fprintf(stderr, "    /tdy x   - Tandy sound       (0:Off 1:2C0 or C0,1E0,300)\n");
    fprintf(stderr, "    /cvx x   - Covox DAC sound   (0:Off 1:LPT1 2:LPT2 or 300)\n");
//    fprintf(stderr, "    /ste x   - Stereo in One     (0:Off 1:LPT1 2:LPT2)\n");
    fprintf(stderr, "    /mmb x   - Mindscape sound   (0:Off 1:300 or 220,2F0,310,320)\n");
    fprintf(stderr, "    /sb x    - Sound Blaster     (0:Off 1:On) use the BLASTER values\n");
#if PICOMEM2
    fprintf(stderr, "    /gus x   - Gravis UltraSound (0:Off 1:On) use the ULTRSND values\n");
    fprintf(stderr, "    /mpu x   - General MIDI font (0:Off 1:330 or 300,320,340,360)\n");
#endif
    fprintf(stderr, "    /diag    - Start in Diagnostic Mode\n");
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

void err_sb(void) {
    //              "................................................................................\n"
    fprintf(stderr, "The BLASTER variable is not present or incorrect:\n");
    fprintf(stderr, "\tset BLASTER=Axxx Iy Dz T3\n");
    fprintf(stderr, "Where xxx = port, y = IRQ, z = DMA. T3 indicates an SB 2.0 compatible card.\n");
}

void err_gus(void) {
    //              "................................................................................\n"
    fprintf(stderr, "The ULTRASND variable is not present or incorrect:\n");
    fprintf(stderr, "\tset ULTRASND=xxx,d,d,i,i\n");
    fprintf(stderr, "Where xxx = port, d = DMA, i = IRQ.\n");
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

#define CMDERR_PORTUSED   0x10
#define CMDERR_MEM_ERR    0x11  // Not enaugh memory
#define CMDERR_AMIX_FAIL  0x12  // Audio mixer init fail

void display_error(uint8_t err)
{
 switch(err)
    {
      case CMDERR_PORTUSED: printf(" IO Port already used\n");
                break;
      case CMDERR_MEM_ERR: printf(" Not enaugh Pico RAM\n");
                break;
      case CMDERR_AMIX_FAIL: printf(" Failed to initialize the mixer\n");
                break;
      case CMDERR_NOIRQ: printf(" No IRQ detected/Available\n");
                break;
      default:   printf(" Code %d\n",err); break;
    }    
}

void display_valid_ports(uint16_t *table, uint8_t size)
{
 for (int i=0;i<size-2;i++)
     printf("%x ",table[i+2]);
 printf("\n");
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

static int configure_sb(void) {
    char* blaster = getenv("BLASTER");
    if (blaster == NULL) {
        err_sb();
        return 1;
    } //else printf(" - BLASTER variable found: %s\n", blaster);

    // Check the BLASTER variable
    int e;
    e = sscanf(blaster, "A%hx I%hhu D%hhu T3", &sbconf.port, &sbconf.irq, &sbconf.dma);

    //printf(" * BLASTER: A%x I%d D%d\n", sbconf.port, sbconf.irq, sbconf.dma);
    if (e != 3) {
        printf(" * BLASTER: A%x I%d D%d\n", sbconf.port, sbconf.irq, sbconf.dma);
        err_sb();
        return 2;
    }
    if ((sbconf.irq!=3)&&(sbconf.irq!=5)&&(sbconf.irq!=7)) {
        printf("Invalid IRQ %d (Must be 3,5 or 7 and not PicoMEM IRQ)\n", sbconf.irq);
        return 3;
    }
    if ((sbconf.dma!=1)&&(sbconf.dma!=3)) {
        printf("Invalid DMA %d (Must be 1 or 3)\n", sbconf.dma);
        return 4;
    }
    e=pm_io_cmd(CMD_SetSBIRQDMA,sbconf.irq|(sbconf.dma<<8));
    if (e) {
        printf("SB Set IRQ DMA Error:");
        display_error(e);
        return 5;
       }
    e=pm_io_cmd(CMD_SBOnOff,sbconf.port);
    if (e) {
        printf("SB Enable Error:",e);
        display_error(e);
        return 6;
       }
    printf(" - Sound Blaster enabled on port %x IRQ %d DMA %d\n", sbconf.port, sbconf.irq, sbconf.dma);
    return 0;
}

static int configure_gus(void) {
    char* ultrsnd = getenv("ULTRASND");
    if (ultrsnd == NULL) {
        err_gus();
        return 1;
    } //else printf(" - BLASTER variable found: %s\n", blaster);

    // Check the ULTRASND variable
    int e;
    e = sscanf(ultrsnd, "%hx,%hhu,%hhu,%hhu,%hhu", &gusconf.port, &gusconf.dma, &gusconf.dma2, &gusconf.irq, &gusconf.irq2);
    if (e != 5) {
        printf(" * ULTRASND: A%x I%d D%d  e:%d\n", gusconf.port, gusconf.irq, gusconf.dma,e);
        err_gus();
        return 2;
    }
    if ((gusconf.irq!=3)&&(gusconf.irq!=5)&&(gusconf.irq!=7)) {
        printf("Invalid IRQ %d (Must be 3,5 or 7 and not PicoMEM IRQ)\n", gusconf.irq);
        return 3;
    }
    if ((gusconf.dma!=1)&&(gusconf.dma!=3)) {
        printf("Invalid DMA %d (Must be 1 or 3)\n", gusconf.dma);
        return 4;
    }
    e=pm_io_cmd(CMD_GUSOnOff,gusconf.port);
    if (e) {
        printf("GUS Enable Error:");
        display_error(e);
        return 6;
       }
    e=pm_io_cmd(CMD_SetGUSIRQDMA,gusconf.irq|(gusconf.dma<<8));
    if (e) {
        printf("GUS Set IRQ DMA Error:");
        display_error(e);
        return 5;
       }       
    printf(" - Gravis UltraSound enabled on port %x IRQ %d DMA %d\n", gusconf.port, gusconf.irq, gusconf.dma);
    return 0;
}


// ! Must be called only if LPT1 or/and LPT2 need to be added
void add_lpt(uint8_t nb)
{
 if (BIOSVAR->Equip>>14==0) { //Add LPT1 to the BIOS if no parallel port
     BIOSVAR->Equip=(BIOSVAR->Equip & 0x3FFF) + 0x4000;
     BIOSVAR->LPT1=0x378;
    }
 if (nb==2) { //Add LPT2 to the BIOS if need to add LPT2
     if ((BIOSVAR->Equip>>14)<2) BIOSVAR->Equip=(BIOSVAR->Equip & 0x3FFF) + 0x8000;
     BIOSVAR->LPT2=0x278;
    }
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
            if (PM_BIOS_IRQ_Present) {
               printf(" - Install the key shortcut. (Ctrl Shift F1/F2)\n");
               pm_irq_cmd(0x12);
              }
              else {
                printf(" > Error: PicoMEM BIOS Needed to enable the shortcut.\n");
                return 0;
              }
        } else if (stricmp(argv[i], "/d") == 0) {   // Shortcut
            if (PM_BIOS_IRQ_Present)
              {
               printf(" - Install the DOS int 21h/2Fh Spy\n");
               pm_irq_cmd(0x0F);
              }
              else {
                printf(" > Error: PicoMEM BIOS Needed to enable the shortcut.\n");
                return 0;
              }
        } else if (stricmp(argv[i], "/diag") == 0) {    // Diagnostic
            printf(" - Diagnostic mode:\n");
            pm_diag();
            return 1;
            
        } else if (stricmp(argv[i], "/dd") == 0) {    // Diagnostic
            printf(" - DMA Diag Mode:\n");
            pm_dmadiag();
            return 1;
            
        } else if (stricmp(argv[i], "/adlib") == 0) { // Adlib
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
        } else if (stricmp(argv[i], "/j") == 0) {    // Joystick
            if (i + 1 >= argc) {
                usage();
                return 255;
            }
            process_port_opt(tmp_uint16);
            printf(" - Joystick : ");
            if (tmp_uint16==0) printf("Disable\n");
                  else printf("Enable (%X)\n",tmp_uint16);
            pm_io_cmd(CMD_Joy_OnOff,tmp_uint16);

        } else if (stricmp(argv[i], "/cms") == 0) {    // CMS
            if (i + 1 >= argc) {
                usage();
                return 255;
            }
            process_port_opt(tmp_uint16);
            printf(" - CMS : ");
            if (check_valid_w(&CMSPortList[0],tmp_uint16,CMS_PORTS))
               {
                if (tmp_uint16==0) 
                   {
                    printf("Disable\n");
                    pm_io_cmd(CMD_CMSOnOff,0);
                   }
                  else 
                   {
                    tmp_uint8=pm_io_cmd(CMD_CMSOnOff,tmp_uint16);
                    if (tmp_uint8) 
                       {
                        printf(" Enable Error: ",tmp_uint8);
                        display_error(tmp_uint8);
                        return 1;
                       } else printf("Enable (%X)\n",(tmp_uint16 == 1) ? 0x220 : tmp_uint16);
                   }
                 
                 pm_io_cmd(CMD_CMSOnOff,tmp_uint16);
               } else 
                 {
                  printf("Invalid port (%X)\n",tmp_uint16);
                 }
        } else if (stricmp(argv[i], "/tdy") == 0) {    // CMS
            if (i + 1 >= argc) {
                usage();
                return 255;
            }
            process_port_opt(tmp_uint16);
            printf(" - Tandy : ");
            if (check_valid_w(&TDYPortList[0],tmp_uint16,TDY_PORTS))
               {
                if (tmp_uint16==0) 
                   {
                    printf("Disable\n");
                    pm_io_cmd(CMD_TDYOnOff,0);
                   }
                  else 
                   {
                    tmp_uint8=pm_io_cmd(CMD_TDYOnOff,tmp_uint16);
                    if (tmp_uint8) 
                       {
                        printf(" Enable Error: ");
                        display_error(tmp_uint8);
                        return 1;
                       } else printf("Enable (%X)\n",(tmp_uint16 == 1) ? 0x2C0 : tmp_uint16);
                   }
               } else                 
                 {
                  printf("Invalid port (%X)\n",tmp_uint16);
                 }
        } else if (stricmp(argv[i], "/cvx") == 0) {    // Covox
            if (i + 1 >= argc) {
                usage();
                return 255;
            }
            process_port_opt(tmp_uint16);
            printf(" - Covox : ");
            if (check_valid_w(&CVXPortList[0],tmp_uint16,CVX_PORTS))
               {
                if (tmp_uint16==0) 
                  {
                    pm_io_cmd(CMD_CVXOnOff,tmp_uint16);
                    printf("Disable\n");
                  }
                  else 
                  { 
                    switch(tmp_uint16) {
                        case 1: //printf("Nb port: %d LPT1: %x",BIOSVAR->Equip>>14,BIOSVAR->LPT1);
                                if ((BIOSVAR->Equip>>14>=1)&&(BIOSVAR->LPT1!=0)) { tmp_uint16=BIOSVAR->LPT1; } // Use the BIOS the LPT1 port
                                   else {
                                     add_lpt(1);
                                     tmp_uint16=0x378;  // Default LPT1
                                    }
                                break;
                        case 2: //printf("Nb port: %d LPT2: %x",BIOSVAR->Equip>>14,BIOSVAR->LPT2);
                                 if (BIOSVAR->Equip>>14>=2&&(BIOSVAR->LPT2!=0)) { tmp_uint16=BIOSVAR->LPT2; } // Use the BIOS the LPT2 port
                                   else {
                                     add_lpt(2);
                                     tmp_uint16=0x278;  // Default LPT2
                                    }
                                break;
                        }
                   tmp_uint16+=CVX_TYPE_DAC<<10;   // Set the type in the upper bits
                   tmp_uint8=pm_io_cmd(CMD_CVXOnOff,tmp_uint16);
                   if (tmp_uint8) 
                       {
                        printf(" Enable Error: ",tmp_uint8);
                        display_error(tmp_uint8);
                        return 1;
                       } else printf("Enable (%X)\n",(tmp_uint16 == 1) ? 0x2C0 : tmp_uint16);
                  }                    
               } else                 
                 {
                  printf("Invalid port (%X)\n",tmp_uint16);
                 } 
        } else if (stricmp(argv[i], "/ste") == 0) {    // Covox
            if (i + 1 >= argc) {
                usage();
                return 255;
            }
            process_port_opt(tmp_uint16);
            printf(" - Stereo in One : ");
            if (check_valid_w(&CVXPortList[0],tmp_uint16,CVX_PORTS))
               {
                if (tmp_uint16==0) 
                  {
                    pm_io_cmd(CMD_CVXOnOff,tmp_uint16);
                    printf("Disable\n");
                  }
                  else 
                  { 
                    switch(tmp_uint16) {
                        case 1: //printf("Nb port: %d LPT1: %x",BIOSVAR->Equip>>14,BIOSVAR->LPT1);
                                if ((BIOSVAR->Equip>>14>=1)&&(BIOSVAR->LPT1!=0)) { tmp_uint16=BIOSVAR->LPT1; } // Use the BIOS the LPT1 port
                                   else {
                                     add_lpt(1);
                                     tmp_uint16=0x378;  // Default LPT1
                                    }
                                break;
                        case 2: //printf("Nb port: %d LPT2: %x",BIOSVAR->Equip>>14,BIOSVAR->LPT2);
                                 if (BIOSVAR->Equip>>14>=2&&(BIOSVAR->LPT2!=0)) { tmp_uint16=BIOSVAR->LPT2; } // Use the BIOS the LPT2 port
                                   else {
                                     add_lpt(2);
                                     tmp_uint16=0x278;  // Default LPT2
                                    }
                                break;
                        }
                   tmp_uint16+=CVX_TYPE_DAC_STEREO<<10;   // Set the type in the upper bits
                   tmp_uint8=pm_io_cmd(CMD_CVXOnOff,tmp_uint16);
                   if (tmp_uint8) 
                       {
                        printf(" Enable Error: ",tmp_uint8);
                        display_error(tmp_uint8);
                        return 1;
                       } else printf("Enable (%X)\n",(tmp_uint16 == 1) ? 0x2C0 : tmp_uint16);
                  }                    
               } else                 
                 {
                  printf("Invalid port (%X)\n",tmp_uint16);
                 } 
        } else if (stricmp(argv[i], "/mpu") == 0) {    // Mindscape
            if (i + 1 >= argc) {
                usage();
                return 255;
            }
            process_port_opt(tmp_uint16);
            printf(" - MPU/General MIDI : ");
            if (check_valid_w(&MPUPortList[0],tmp_uint16,MPU_PORTS))
               {
                if (tmp_uint16==0) printf("Disable\n");
                  else {
                      tmp_uint8=pm_io_cmd(CMD_MPUOnOff,tmp_uint16);
                      if (tmp_uint8) 
                          {
                           printf(" Enable Error: ",tmp_uint8);
                           display_error(tmp_uint8);
                           return 1;
                          } else printf("Enable (%x)\n",(tmp_uint16 == 1) ? 0x300 : tmp_uint16);                  
                     }
               } else 
                 {
                  printf("Invalid port (%X)\n",tmp_uint16);
                 }
        }else if (stricmp(argv[i], "/mmb") == 0) {    // Mindscape
            if (i + 1 >= argc) {
                usage();
                return 255;
            }
            process_port_opt(tmp_uint16);
            printf(" - Mindscape Music Board : ");
            if (check_valid_w(&MMBPortList[0],tmp_uint16,MMB_PORTS))
               {
                if (tmp_uint16==0) printf("Disable\n");
                  else {
                      tmp_uint8=pm_io_cmd(CMD_MMBOnOff,tmp_uint16);
                      if (tmp_uint8) 
                          {
                           printf(" Enable Error: ",tmp_uint8);
                           display_error(tmp_uint8);
                           return 1;
                          } else printf("Enable (%x)\n",(tmp_uint16 == 1) ? 0x300 : tmp_uint16);                  
                     }
               } else 
                 {
                  printf("Invalid port (%X)\n",tmp_uint16);
                 }
        } else if (stricmp(argv[i], "/sb") == 0) {    // Sound Blaster
            if (i + 1 >= argc) {
                usage();
                return 255;
            }
            process_port_opt(tmp_uint16);
            if (tmp_uint16==0) 
                   {
                    pm_io_cmd(CMD_SBOnOff,0);
                    printf(" - Sound Blaster : Disable\n");
                   }
                  else 
                   {
                    configure_sb();
                   }                      
        }
         else if (stricmp(argv[i], "/gus") == 0) {    // Gravis UltraSound
            if (i + 1 >= argc) {
                usage();
                return 255;
            }
            process_port_opt(tmp_uint16);
            if (tmp_uint16==0) 
                   {
                    pm_io_cmd(CMD_GUSOnOff,0);
                    printf(" - Gravis UltraSound : Disable\n");
                   }
                  else 
                   {
                    configure_gus();
                   }                      
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
