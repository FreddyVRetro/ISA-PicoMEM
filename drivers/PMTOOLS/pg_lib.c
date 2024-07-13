#include <conio.h>
#include <dos.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <i86.h>


int init_gus(char *argv0) {
    char* ultrasnd = getenv("ULTRASND");
    if (ultrasnd == NULL) {
        err_ultrasnd(argv0);
        return 1;
    }

    // Parse ULTRASND
    uint16_t port;
    uint8_t irq;
    uint8_t dma;
    int e;
    e = sscanf(ultrasnd, "%hx,%hhu,%*hhu,%hhu,%*hhu", &port, &irq, &dma);
    if (e != 3) {
        err_ultrasnd(argv0);
        return 2;
    }

    outp(PG_CONTROL_PORT, 0x04); // Select port register
    outpw(DATA_PORT_LOW, port);  // Write port

    // Detect if there's something GUS-like...
    // Set memory address to 0
    outp(port + 0x103, 0x43);
    outpw(port + 0x104, 0x0);
    outp(port + 0x103, 0x44);
    outpw(port + 0x104, 0x0);
    // Write something
    outp(port + 0x107, 0xDD);
    // Read it and see if it's the same
    if (inp(port + 0x107) != 0xDD) {
        fprintf(stderr, "ERROR: Card not responding to GUS commands on port %x\n", port); 
        return 98;
    }
    printf("GUS-like card detected on port %x...\n", port);

    // Enable IRQ latches
    outp(port, 0x8);
    // Select reset register
    outp(port + 0x103, 0x4C);
    // Master reset to run. DAC enable and IRQ enable will be done by the application.
    outp(port + 0x105, 0x1);
    return 0;
}
