/*
SB2CD I/O MAP (From SoundBlaster Programmers Guide Page 119)

Base 0x250h     (250h to 255h)
Base 0x260h     (260h to 265h)

Base + 0x0h     CD-ROM Command or Data Register (Read/Write)
Base + 0x1h     CD-ROM Status Register          (Read Only)
Base + 0x2h     CD-ROM Reset Register           (Write Only)
Base + 0x3h     CD-ROM Enable Register          (Write Only)
Base + 0x4h     Mixer Chip Register Address     (Write Only)
Base + 0x5h     Mixer Chip Data                 (Read/Write)

    CDo_command=sbpcd_ioaddr;       250
	CDi_info=sbpcd_ioaddr;          250
	CDi_status=sbpcd_ioaddr+1;      251
	CDo_sel_i_d=sbpcd_ioaddr+1;     251
	CDo_reset=sbpcd_ioaddr+2;       252
	CDo_enable=sbpcd_ioaddr+3;      253

Addresses:
Command (output)    = base
Select  (output)    = base + 1
Reset   (output)    = base + 2
Enable  (output)    = base + 3

Info    (input)     = base
Status  (input)     = base + 1
Data    (input)     = base + 2  (base)





*/

#include "mke.h"
#include "../cdrom/cdrom.h"

#include <stdio.h>


#define MKE_STATUS_FIFO_SIZE    256
#define MKE_DATA_FIFO_SIZE    16

typedef struct mke_t {
    bool    tray_open;
    
    uint8_t enable_register;

    uint8_t command_buffer[7];
    uint8_t command_buffer_pending; 

    uint8_t data_select;
    uint8_t errors[8];    

    uint8_t media_selected;//temporary hack


} mke_t;
mke_t mke;


void media_update();

void MKE_COMMAND(uint8_t value) {  
    uint16_t i,len;
    uint8_t x[12];//this is wasteful handling of buffers for compatibility, but will optimize later.
    subchannel_t subc;

    if(mke.command_buffer_pending) {                 
        mke.command_buffer[6-mke.command_buffer_pending+1]=value;
        mke.command_buffer_pending--;        
    }

    if(mke.command_buffer[0] == CMD1_ABORT) {
        printf("CMD_ABORT\n");
        cdrom_fifo_clear(&cdrom->info_fifo);
        mke.command_buffer[0]=0;
        mke.command_buffer_pending=7;
        cdrom_output_status(cdrom);
    }

    if(!mke.command_buffer_pending && mke.command_buffer[0]) { // We are done and we have a command        
        switch(mke.command_buffer[0]) { 
            case 06: //TRAY OUT
                cdrom_fifo_clear(&cdrom->info_fifo);
                printf("TRAY OUT\n");
                cdrom_output_status(cdrom);
                mke.tray_open=true;
                break;
            case 07: //TRAY IN
                cdrom_fifo_clear(&cdrom->info_fifo);                
                printf("TRAY IN\n");
                cdrom_output_status(cdrom);                                
                mke.tray_open=false;
                break;
            case CMD1_READ:                
                cdrom->req_m=mke.command_buffer[1];
                cdrom->req_s=mke.command_buffer[2];
                cdrom->req_f=mke.command_buffer[3];
                cdrom->req_cur=0;            
                cdrom->req_total=mke.command_buffer[6];                                                
                cdrom_fifo_clear(&cdrom->info_fifo);                                                                                            
                break;
            case CMD1_READSUBQ:                
                cdrom_get_subq(cdrom,(uint8_t *)&x);    
                cdrom_fifo_clear(&cdrom->info_fifo);                                                            
                /* cdrom_fifo_write_multiple(&cdrom->info_fifo, x, 11); */
                for(i=0;i<11;i++) {                
                    cdrom_fifo_write(&cdrom->info_fifo,x[i]);                                
                }
                cdrom_output_status(cdrom);
                break;
            case CMD1_SETMODE: //Returns 1
                cdrom_fifo_clear(&cdrom->info_fifo);                                                                            
                printf("CMD: SET MODE:");
                for(i=0;i<6;i++) {
                    printf("%02x ",mke.command_buffer[i+1]);
                }
                printf("\n");                
                cdrom_output_status(cdrom);
                break;
            case CMD1_GETMODE://6
                printf("GET MODE\n");
                /*
                uint8_t mode[5] = {[1] = 0x08};
                cdrom_fifo_write_multiple(&cdrom->info_fifo, mode, 5);
                */
                cdrom_fifo_write(&cdrom->info_fifo,0x00);
                cdrom_fifo_write(&cdrom->info_fifo,0x08);
                cdrom_fifo_write(&cdrom->info_fifo,0x00);
                cdrom_fifo_write(&cdrom->info_fifo,0x00);
                cdrom_fifo_write(&cdrom->info_fifo,0x00);
                cdrom_output_status(cdrom);
                break;
            case CMD1_PAUSERESUME:                
                cdrom_audio_pause_resume(cdrom,mke.command_buffer[1] >> 7);
                cdrom_output_status(cdrom);
                break;
            case CMD1_CAPACITY://6
                printf("DISK CAPACITY\n");
                cdrom_disc_capacity(cdrom, (uint8_t *)&x);
                /* cdrom_fifo_write_multiple(&cdrom->info_fifo, x, 5); */
                for(i=0;i<5;i++) {
                    printf("%02x ",x[i]);
                    cdrom_fifo_write(&cdrom->info_fifo,x[i]);
                }
                printf("\n");
                cdrom_output_status(cdrom);
                break;
            case CMD1_DISKINFO: //7
                printf("DISK INFO\n");
                cdrom_disc_info(cdrom, (uint8_t *)&x);
                /* cdrom_fifo_write_multiple(&cdrom->info_fifo, x, 6); */
                for(i=0;i<6;i++) {
                    printf("%02x ",x[i]);
                    cdrom_fifo_write(&cdrom->info_fifo,x[i]);
                }
                printf("\n");
                cdrom_output_status(cdrom);
                break;
            case CMD1_READTOC:
                cdrom_fifo_clear(&cdrom->info_fifo);                                                                            
                printf("READ TOC:");  
                for(i=0;i<6;i++) {
                    printf("%02x ",mke.command_buffer[i+1]);
                }
                printf(" | ");                                
                cdrom_read_toc(cdrom,(uint8_t *)&x,mke.command_buffer[2]);                
                /* cdrom_fifo_write_multiple(&cdrom->info_fifo, x, 8); */
                for(i=0;i<8;i++) {
                    printf("%02x ",x[i]);
                    cdrom_fifo_write(&cdrom->info_fifo,x[i]);
                }
                printf("\n");
                cdrom_output_status(cdrom);
                break;
            case CMD1_PLAY_MSF:
                cdrom_fifo_clear(&cdrom->info_fifo);                                                                                            
                printf("PLAY MSF:");  
                for(i=0;i<6;i++) {
                    printf("%02x ",mke.command_buffer[i+1]);
                }
                printf("\n");                
                cdrom_audio_playmsf(cdrom,
                    mke.command_buffer[1],
                    mke.command_buffer[2],
                    mke.command_buffer[3],
                    mke.command_buffer[4],
                    mke.command_buffer[5],
                    mke.command_buffer[6]
                );
                cdrom_output_status(cdrom);
                break;            
            case CMD1_SEEK :
                cdrom_fifo_clear(&cdrom->info_fifo);                                                                                                
                printf("SEEK MSF:");  //TODO: DOES THIS IMPACT CURRENT PLAY LENGTH?
                for(i=0;i<6;i++) {
                    printf("%02x ",mke.command_buffer[i+1]);
                }
                cdrom_seek(cdrom,mke.command_buffer[1],mke.command_buffer[2],mke.command_buffer[3]);
                cdrom_output_status(cdrom);
                break;            
            case CMD1_SESSINFO:
                cdrom_fifo_clear(&cdrom->info_fifo);                                                                                                                
                printf("CMD: READ SESSION INFO\n");
                char session_info[6] = {0};
                /* cdrom_fifo_write_multiple(&cdrom->info_fifo, session_info, 6); */
                cdrom_fifo_write(&cdrom->info_fifo,0);
                cdrom_fifo_write(&cdrom->info_fifo,0);
                cdrom_fifo_write(&cdrom->info_fifo,0);
                cdrom_fifo_write(&cdrom->info_fifo,0);
                cdrom_fifo_write(&cdrom->info_fifo,0);
                cdrom_fifo_write(&cdrom->info_fifo,0);
                cdrom_output_status(cdrom);
                break;
            case CMD1_READ_UPC:
                cdrom_fifo_clear(&cdrom->info_fifo);
                printf("CMD: READ UPC\n");
                char upc[8] = {[0] = 80};
                /* cdrom_fifo_write_multiple(&cdrom->info_fifo, upc, 8); */
                cdrom_fifo_write(&cdrom->info_fifo,0x80);
                cdrom_fifo_write(&cdrom->info_fifo,0);
                cdrom_fifo_write(&cdrom->info_fifo,0);
                cdrom_fifo_write(&cdrom->info_fifo,0);
                cdrom_fifo_write(&cdrom->info_fifo,0);
                cdrom_fifo_write(&cdrom->info_fifo,0);
                cdrom_fifo_write(&cdrom->info_fifo,0);
                cdrom_fifo_write(&cdrom->info_fifo,0);
                cdrom_output_status(cdrom);
                break;
            case CMD1_READ_ERR:
                cdrom_fifo_clear(&cdrom->info_fifo);                
                printf("CMD: READ ERR\n");                
                cdrom_read_errors(cdrom,(uint8_t *)x);
                /* cdrom_fifo_write_multiple(&cdrom->info_fifo, x, 8); */
                for(i=0;i<8;i++) {
                    cdrom_fifo_write(&cdrom->info_fifo,x[i]);
                }
                cdrom_output_status(cdrom);
                break;        
            case CMD1_READ_VER:
                /*
                SB2CD Expects 12 bytes, but drive only returns 11.
                */
                cdrom_fifo_clear(&cdrom->info_fifo);                
                printf("CMD: READ VER\n");                                
                /* char ver[] = "CR-5630.75"; */
                /* cdrom_fifo_write_multiple(&cdrom->info_fifo, ver, 10); */
                cdrom_fifo_write(&cdrom->info_fifo,'C');
                cdrom_fifo_write(&cdrom->info_fifo,'R');
                cdrom_fifo_write(&cdrom->info_fifo,'-');
                cdrom_fifo_write(&cdrom->info_fifo,'5');
                cdrom_fifo_write(&cdrom->info_fifo,'6');
                cdrom_fifo_write(&cdrom->info_fifo,'3');
                cdrom_fifo_write(&cdrom->info_fifo,'0');
                cdrom_fifo_write(&cdrom->info_fifo,'.');
                cdrom_fifo_write(&cdrom->info_fifo,'7');
                cdrom_fifo_write(&cdrom->info_fifo,'5');
                cdrom_output_status(cdrom);
                break;
            case CMD1_STATUS:
                cdrom_fifo_clear(&cdrom->info_fifo);                                
                cdrom_output_status(cdrom);                
                break;
            default:
                printf("MKE: Unknown Commnad [%02x]\n",mke.command_buffer[0]);
        }
        mke.command_buffer[0]=0;
    }
    else if(!mke.command_buffer_pending) {//we are done byt not in a command.  should we make sure it is a valid command here?        
        mke.command_buffer[0]=value;
        mke.command_buffer_pending=6;
    }        
}


void MKE_WRITE(uint16_t address, uint8_t value) {    
    if(mke.enable_register && ((address & 0xF) != 3)) {
        //printf("Ignore Write Unit %u\n",mke.enable_register);
        return;
    }
    //printf("MKE WRITE: %02x => %03x\n",value,address);
    switch(address & 0xF) {
        case 0:
            MKE_COMMAND(value);
            break;
        case 1:            
            mke.data_select=value;
            break;
        case 2:
            switch(value) {
                case 1:
                    sprintf(cdrom->image_path,"MECH2_16B.CUE");
                    break;
                case 2:
                    sprintf(cdrom->image_path,"MOC1.cue");
                    break;
                /*
                case 3:
                    sprintf(cdrom->image_path,"war2.cue");
                    break;
                case 4:
                    sprintf(cdrom->image_path,"myst.iso");
                    break;
                case 5:
                    sprintf(cdrom->image_path,"megarace.iso");
                    break;
                */
                default:
                    sprintf(cdrom->image_path,"");
                    break;
            }                        
            cdrom->req_image_load=1;
                       printf("loaded %s\n", cdrom->image_path); 
            break;
        case 3:
            mke.enable_register=value;
            break;
        default:
            break;
    }    
}

uint8_t MKE_READ(uint16_t address) {
    uint8_t x;        
    if(mke.enable_register ) {
      //  printf("Ignore Read Unit %u\n",mke.enable_register);
        return 0;
    }    
    switch(address & 0xF) {        
        case 0://Info            
            if(mke.data_select) {                                       
                   return cdrom_fifo_read(&cdrom->data_fifo);
            }
            else {                    
                
                return cdrom_fifo_read(&cdrom->info_fifo);
            }            
            break;
            case 1://Status 
            /*
            1 = Status Change
            2 = Data Ready
            4 = Response Ready
            8 = Attention / Issue ?
            */
            x=0xFF;                        
            //if(cdrom->media_changed) x ^= 1;
            if(cdrom_fifo_level(&cdrom->data_fifo)) x ^= 2;//DATA FIFO
            if(cdrom_fifo_level(&cdrom->info_fifo)) x ^= 4;//STATUS FIFO
            if(cdrom_has_errors(cdrom)) x ^=8;            
            return x;      
            break;
        case 2://Data
            return cdrom_fifo_read(&cdrom->data_fifo);    
            case 3:
                return mke.enable_register;
                break;
        default:
            printf("MKE Unknown Read Address: %03x\n",address);
            break;
    }
}

      


void mke_init() {        
    cdrom_fifo_init(&cdrom->data_fifo,2048+2048+32);
    cdrom_fifo_init(&cdrom->info_fifo,32);
    cdrom_audio_fifo_init(cdrom);
    printf("FIFOS, INFO=%p   DATA=%p\n",cdrom->info_fifocdrom->.data_fifo);

    mke.command_buffer_pending=7;
    cdrom->sound_on=1;

}

