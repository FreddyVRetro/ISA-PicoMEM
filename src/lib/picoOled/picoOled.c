/*

MIT License

Parts of this file are:
    Copyright (c) 2021 David Schramm


Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// From https://github.com/jburrell7/RPi-Pico-OLED-DRIVER/tree/main

#include "pico.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"
#include "picoOled.h"

static const uint8_t oled_font6x8 [] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // sp
    0x00, 0x00, 0x00, 0x2f, 0x00, 0x00, // !
    0x00, 0x00, 0x07, 0x00, 0x07, 0x00, // "
    0x00, 0x14, 0x7f, 0x14, 0x7f, 0x14, // #
    0x00, 0x24, 0x2a, 0x7f, 0x2a, 0x12, // $
    0x00, 0x62, 0x64, 0x08, 0x13, 0x23, // %
    0x00, 0x36, 0x49, 0x55, 0x22, 0x50, // &
    0x00, 0x00, 0x05, 0x03, 0x00, 0x00, // '
    0x00, 0x00, 0x1c, 0x22, 0x41, 0x00, // (
    0x00, 0x00, 0x41, 0x22, 0x1c, 0x00, // )
    0x00, 0x14, 0x08, 0x3E, 0x08, 0x14, // *
    0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, // +
    0x00, 0x00, 0x00, 0xA0, 0x60, 0x00, // ,
    0x00, 0x08, 0x08, 0x08, 0x08, 0x08, // -
    0x00, 0x00, 0x60, 0x60, 0x00, 0x00, // .
    0x00, 0x20, 0x10, 0x08, 0x04, 0x02, // /
    0x00, 0x3E, 0x51, 0x49, 0x45, 0x3E, // 0
    0x00, 0x00, 0x42, 0x7F, 0x40, 0x00, // 1
    0x00, 0x42, 0x61, 0x51, 0x49, 0x46, // 2
    0x00, 0x21, 0x41, 0x45, 0x4B, 0x31, // 3
    0x00, 0x18, 0x14, 0x12, 0x7F, 0x10, // 4
    0x00, 0x27, 0x45, 0x45, 0x45, 0x39, // 5
    0x00, 0x3C, 0x4A, 0x49, 0x49, 0x30, // 6
    0x00, 0x01, 0x71, 0x09, 0x05, 0x03, // 7
    0x00, 0x36, 0x49, 0x49, 0x49, 0x36, // 8
    0x00, 0x06, 0x49, 0x49, 0x29, 0x1E, // 9
    0x00, 0x00, 0x36, 0x36, 0x00, 0x00, // :
    0x00, 0x00, 0x56, 0x36, 0x00, 0x00, // ;
    0x00, 0x08, 0x14, 0x22, 0x41, 0x00, // <
    0x00, 0x14, 0x14, 0x14, 0x14, 0x14, // =
    0x00, 0x00, 0x41, 0x22, 0x14, 0x08, // >
    0x00, 0x02, 0x01, 0x51, 0x09, 0x06, // ?
    0x00, 0x32, 0x49, 0x59, 0x51, 0x3E, // @
    0x00, 0x7C, 0x12, 0x11, 0x12, 0x7C, // A
    0x00, 0x7F, 0x49, 0x49, 0x49, 0x36, // B
    0x00, 0x3E, 0x41, 0x41, 0x41, 0x22, // C
    0x00, 0x7F, 0x41, 0x41, 0x22, 0x1C, // D
    0x00, 0x7F, 0x49, 0x49, 0x49, 0x41, // E
    0x00, 0x7F, 0x09, 0x09, 0x09, 0x01, // F
    0x00, 0x3E, 0x41, 0x49, 0x49, 0x7A, // G
    0x00, 0x7F, 0x08, 0x08, 0x08, 0x7F, // H
    0x00, 0x00, 0x41, 0x7F, 0x41, 0x00, // I
    0x00, 0x20, 0x40, 0x41, 0x3F, 0x01, // J
    0x00, 0x7F, 0x08, 0x14, 0x22, 0x41, // K
    0x00, 0x7F, 0x40, 0x40, 0x40, 0x40, // L
    0x00, 0x7F, 0x02, 0x0C, 0x02, 0x7F, // M
    0x00, 0x7F, 0x04, 0x08, 0x10, 0x7F, // N
    0x00, 0x3E, 0x41, 0x41, 0x41, 0x3E, // O
    0x00, 0x7F, 0x09, 0x09, 0x09, 0x06, // P
    0x00, 0x3E, 0x41, 0x51, 0x21, 0x5E, // Q
    0x00, 0x7F, 0x09, 0x19, 0x29, 0x46, // R
    0x00, 0x46, 0x49, 0x49, 0x49, 0x31, // S
    0x00, 0x01, 0x01, 0x7F, 0x01, 0x01, // T
    0x00, 0x3F, 0x40, 0x40, 0x40, 0x3F, // U
    0x00, 0x1F, 0x20, 0x40, 0x20, 0x1F, // V
    0x00, 0x3F, 0x40, 0x38, 0x40, 0x3F, // W
    0x00, 0x63, 0x14, 0x08, 0x14, 0x63, // X
    0x00, 0x07, 0x08, 0x70, 0x08, 0x07, // Y
    0x00, 0x61, 0x51, 0x49, 0x45, 0x43, // Z
    0x00, 0x00, 0x7F, 0x41, 0x41, 0x00, // [
    0x02, 0x04, 0x08, 0x10, 0x20, 0x00, // backslash
    0x00, 0x00, 0x41, 0x41, 0x7F, 0x00, // ]
    0x00, 0x04, 0x02, 0x01, 0x02, 0x04, // ^
    0x00, 0x40, 0x40, 0x40, 0x40, 0x40, // _
    0x00, 0x00, 0x01, 0x02, 0x04, 0x00, // '
    0x00, 0x20, 0x54, 0x54, 0x54, 0x78, // a
    0x00, 0x7F, 0x48, 0x44, 0x44, 0x38, // b
    0x00, 0x38, 0x44, 0x44, 0x44, 0x00, // c was 0x20
    0x00, 0x38, 0x44, 0x44, 0x48, 0x7F, // d
    0x00, 0x38, 0x54, 0x54, 0x54, 0x18, // e
    0x00, 0x08, 0x7E, 0x09, 0x01, 0x02, // f
    0x00, 0x18, 0xA4, 0xA4, 0xA4, 0x7C, // g
    0x00, 0x7F, 0x08, 0x04, 0x04, 0x78, // h
    0x00, 0x00, 0x44, 0x7D, 0x40, 0x00, // i
    0x00, 0x40, 0x80, 0x84, 0x7D, 0x00, // j
    0x00, 0x7F, 0x10, 0x28, 0x44, 0x00, // k
    0x00, 0x00, 0x41, 0x7F, 0x40, 0x00, // l
    0x00, 0x7C, 0x04, 0x18, 0x04, 0x78, // m
    0x00, 0x7C, 0x08, 0x04, 0x04, 0x78, // n
    0x00, 0x38, 0x44, 0x44, 0x44, 0x38, // o
    0x00, 0xFC, 0x24, 0x24, 0x24, 0x18, // p
    0x00, 0x18, 0x24, 0x24, 0x18, 0xFC, // q
    0x00, 0x7C, 0x08, 0x04, 0x04, 0x08, // r
    0x00, 0x48, 0x54, 0x54, 0x54, 0x20, // s
    0x00, 0x04, 0x3F, 0x44, 0x40, 0x20, // t
    0x00, 0x3C, 0x40, 0x40, 0x20, 0x7C, // u
    0x00, 0x1C, 0x20, 0x40, 0x20, 0x1C, // v
    0x00, 0x3C, 0x40, 0x30, 0x40, 0x3C, // w
    0x00, 0x44, 0x28, 0x10, 0x28, 0x44, // x
    0x00, 0x1C, 0xA0, 0xA0, 0xA0, 0x7C, // y
    0x00, 0x44, 0x64, 0x54, 0x4C, 0x44, // z
    0x00, 0x00, 0x08, 0x77, 0x41, 0x00, // {
    0x00, 0x00, 0x00, 0x63, 0x00, 0x00, // ¦
    0x00, 0x00, 0x41, 0x77, 0x08, 0x00, // }
    0x00, 0x08, 0x04, 0x08, 0x08, 0x04, // ~
    0x00, 0x3D, 0x40, 0x40, 0x20, 0x7D, // ü
    0x00, 0x3D, 0x40, 0x40, 0x40, 0x3D, // Ü
    0x00, 0x21, 0x54, 0x54, 0x54, 0x79, // ä
    0x00, 0x7D, 0x12, 0x11, 0x12, 0x7D, // Ä
    0x00, 0x39, 0x44, 0x44, 0x44, 0x39, // ö
    0x00, 0x3D, 0x42, 0x42, 0x42, 0x3D, // Ö
    0x00, 0x02, 0x05, 0x02, 0x00, 0x00, // °
    0x00, 0x7E, 0x01, 0x49, 0x55, 0x73, // ß
};

void oledLogI2cResult(int rtnCode, char *msg){

    if (strlen(msg) > 0) printf("user msg: %s\n", msg);

    switch (rtnCode) {
    case PICO_ERROR_GENERIC:
        printf("Adr not acknowledged!\n");
        break;
    case PICO_ERROR_TIMEOUT:
        printf("Timeout!\n");
        break;
    default:
        printf("Wrote successfully %lu bytes!\n", (uint32_t)rtnCode);
        break;
    }
}

void  oledLogI2cData(uint8_t *buffer, int bufSize){
int lines, cols;
int bufIndex, i, j;

    bufIndex = 0;
    printf("************************************************\n");
    printf("I2C data: \n");
    if (bufSize & 0x0F){
        cols = bufSize & 0x0F;
        for (i = 0; i < cols; i++){
            printf("%02x ", buffer[i]);
        }
        printf("\n");
        bufSize -= cols;
        bufIndex = i;
    }

    if (bufSize > 0){
        cols = 0;
        for(i = 0; i < bufSize; i++){
            printf("%02x ", buffer[bufIndex]);
            bufIndex++;
            cols++;
            if ((cols & 15) == 0) printf("\n");
        }
    }
    printf("************************************************\n");

}
void myI2cWriteBlocking(t_OledParams *oled, uint8_t *buf, uint16_t bufSize, char *msg){
int rtnCode;

    rtnCode = i2c_write_blocking(oled->i2c, oled->i2c_address, buf, bufSize, false);
    
    if (DO_I2CERRLOGGING) oledLogI2cResult(rtnCode, msg);
    if (DO_I2CDATLOGGING) oledLogI2cData(buf, bufSize);
}

uint8_t oledI2cConfig(t_OledParams *oled){
// set up the i2c port hardware
//    i2c_init(oled->i2c, 200000);
    i2c_init(oled->i2c, 400000);    
    gpio_set_function(oled->SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(oled->SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(oled->SDA_PIN);
    gpio_pull_up(oled->SCL_PIN);

// set up the power on defaults for the OLED display
    oled->fontInverted  = false;            // true if the font is inverted
    oled->color         = WHITE;            // text color
    oled->scaling       = NORMAL_SIZE;      // font size (DOUBLE or SINGLE)
    oled->ttyMode       = false;            // true if display in tty text mode
    oled->scroll_type   = NO_SCROLLING;
    oled->X             = 0;                // current print position column
    oled->Y             = 0;                // current print position line
    oled->pages = ((oled->height + 7) / 8);

    oled->usingOffset = false;
    if (oled->width == W_132){
        oled->width = W_128;
        oled->usingOffset = true;
    }

    oled->bufsize = (int16_t)(oled->pages * oled->width);

// fire it up
    oledInit(oled);
}


void oledInit(t_OledParams *oled)
{
    int rtnCode;
    uint8_t comPin0, comPin1;
    uint8_t i;

    if ((oled->width == 128) && (oled->height == 32))
    {
        // i2c_send(0xDA); // com pins hardware configuration
        // i2c_send(0x02); 
        comPin0 = 0xDA; // com pins hardware configuration
        comPin1 = 0x02; 

    }
    else if ((oled->width == 128) && (oled->height == 64))
    {
        // i2c_send(0xDA); // com pins hardware configuration
        // i2c_send(0x12); 
        comPin0 = 0xDA; // com pins hardware configuration
        comPin1 = 0x12; 
    }
    else if ((oled->width == 96) && (oled->height == 16))
    {
        // i2c_send(0xDA); // com pins hardware configuration
        // i2c_send(0x02); 
        comPin0 = 0xDA; // com pins hardware configuration
        comPin1 = 0x02; 
    }

    uint8_t params[] = {
    0x00,                           // command
    0xAE,                           // display off
    0xD5,                           // clock divider
    0x80, 
    0xA8,                           // multiplex ratio
    oled->height - 1, 
    0xD3,                           // no display offset
    0x00, 
    0x40,                           // start line address=0
    0x8D,                           // enable charge pump
    0x14, 
    0x20,                           // memory adressing mode=horizontal
    0x00, 
    0xA1,                           // segment remapping mode
    0xC8,                           // COM output scan direction
    comPin0,
    comPin1,
    0x81,                           // contrast control
    0x7F,
    0xD9,                           // pre-charge period
    0x22, 
    0xDB,                           // set vcomh deselect level
    0x20, 
    0xA4,                           // output RAM to display
    0xA6,                           // display mode A6=normal, A7=inverse
    0x2E};                          // stop scrolling

//    rtnCode = i2c_write_blocking(oled->i2c, oled->i2c_address, params, sizeof(params), false);

    myI2cWriteBlocking(oled, params, sizeof(params), "oledInit()");

    // Switch display on
    oledSet_power(oled, true);

    // Clear and send buffer
    oledClear(oled, BLACK);
    oledDisplay(oled);


}

void oledSet_power(t_OledParams *oled, bool enable)
{

    if (enable)
    {
        uint8_t cmd[] = {
            0x00,
            0x8D,
            0x14,
            0xAF};
//        i2c_write_blocking(oled->i2c, oled->i2c_address, cmd, sizeof(cmd), false);
        myI2cWriteBlocking(oled, cmd, sizeof(cmd), "oledSet_power()");
        // i2c_send(0x8D); // enable charge pump
        // i2c_send(0x14);
        // i2c_send(0xAF); // display on
    } else {
        uint8_t cmd[] = {
            0x00,
            0xAE,
            0x8D,
            0x10};
//        i2c_write_blocking(oled->i2c, oled->i2c_address, cmd, sizeof(cmd), false);
        myI2cWriteBlocking(oled, cmd, sizeof(cmd), "oledSet_power()");
        // i2c_send(0xAE); // display off
        // i2c_send(0x8D); // disable charge pump
        // i2c_send(0x10);
    }

}

void oledSet_invert(t_OledParams *oled, bool enable)
{
    if (enable){
        uint8_t cmd[] = {
            0x00,
            0xA7};
//        i2c_write_blocking(oled->i2c, oled->i2c_address, cmd, sizeof(cmd), false);
        myI2cWriteBlocking(oled, cmd, sizeof(cmd), "oledSet_invert()");
//        i2c_send(0xA7); // invert display
    }  else {
        uint8_t cmd[] = {
            0x00,
            0xA6};
//        i2c_write_blocking(oled->i2c, oled->i2c_address, cmd, sizeof(cmd), false);
        myI2cWriteBlocking(oled, cmd, sizeof(cmd), "oledSet_invert()");
//        i2c_send(0xA6); // normal display
    }

}

void oledSet_scrolling(t_OledParams *oled, tScrollEffect scroll_type, uint8_t first_page, uint8_t last_page, uint8_t speed)
{
    // i2c_start();
    // i2c_send(i2c_address << 1); // address + write
    // i2c_send(0x00); // command
    // i2c_send(0x2E); // deativate scroll

    if (scroll_type == NO_SCROLLING){
        uint8_t cmd[] = {
            0x00,
            0X2E};
//        i2c_write_blocking(oled->i2c, oled->i2c_address, cmd, sizeof(cmd), false);
        myI2cWriteBlocking(oled, cmd, sizeof(cmd), "oledSet_scrolling");
        oled->scroll_type = scroll_type;
        return;
    }

    if ((scroll_type == DIAGONAL_LEFT) || (scroll_type == DIAGONAL_RIGHT)){
        uint8_t cmd[] = {
            0x00,
            0X2E,       // deactivate current scroll mode
            0xA3,       // vertical scroll area 
            0x00,
            (oled->height - 1),
            scroll_type,
            0x00,
            first_page,
            0x00,
            last_page,
            0x01,
            0x2F};
//            i2c_write_blocking(oled->i2c, oled->i2c_address, cmd, sizeof(cmd), false);
            myI2cWriteBlocking(oled, cmd, sizeof(cmd), "oledSet_scrolling");
            oled->scroll_type = scroll_type;
        return;
    }

    if ((scroll_type == HORIZONTAL_RIGHT) || (scroll_type == HORIZONTAL_LEFT)){
        if (oled->ctlrType == CTRL_SSD1309) {
            uint8_t cmd[] = {
                0x00,
                0X2E,           // deactivate current scroll mode
                scroll_type,
                0x00,
                first_page,
                speed,          // speed
                last_page,
                0x00,           // dummy byte // added by Rodrik for SSD1309
                0x00,           // first column
                0x7F,           // last column
                0x2F};
                myI2cWriteBlocking(oled, cmd, sizeof(cmd), "oledSet_scrolling");
        } else {
            uint8_t cmd[] = {
                0x00,
                0X2E,           // deactivate current scroll mode
                scroll_type,
                0x00,
                first_page,
                speed,          // speed
                last_page,
                0x00,           // first column
                0x7F,           // last column
                0x2F};
                myI2cWriteBlocking(oled, cmd, sizeof(cmd), "oledSet_scrolling");
        }
        oled->scroll_type = scroll_type;
        return;
    }
}


void oledSet_contrast(t_OledParams *oled, uint8_t contrast)
{
uint8_t cmd[] = {
        0x00,
        0x81,
        contrast
    };
//    i2c_write_blocking(oled->i2c, oled->i2c_address, cmd, sizeof(cmd), false);
    myI2cWriteBlocking(oled, cmd, sizeof(cmd), "oledSet_contrast");

    // i2c_start();
    // i2c_send(i2c_address << 1); // address + write
    // i2c_send(0x00); // command
    // i2c_send(0x81); // deativate scrol
    // i2c_send(contrast); 
    // i2c_stop();
}

void oledClear(t_OledParams *oled, tColor color)
{
    if (color == WHITE)
    {
        memset(&oled->buffer, 0xFF, oled->bufsize);
    }
    else
    {
        memset(&oled->buffer, 0x00, oled->bufsize);
    }
    oled->X=0;
    oled->Y=0;
}

void oledDisplay(t_OledParams *oled)
{
    uint8_t     txBuf[144];
    uint16_t    bufIndex;
    uint16_t    index = 0;
    uint16_t    bytesToSend = 0;

    for (uint8_t page = 0; page < oled->pages; page++)
    {
        if (oled->ctlrType == CTRL_SH1106)
        {
            uint8_t cmd[] = {
                0x00,           // command
                0xB0 + page,    // set page
                0x00,           // lower columns address =0
                0x10};          // upper columns address =0
            myI2cWriteBlocking(oled, cmd, sizeof(cmd), "oledDisplay");
        } else {
            uint8_t cmd[] = {
                0x00,               // command
                0xB0 + page,        // set page
                0x21,               // column address
                0x00,               // first column =0
                oled->width - 1};   // last column
            myI2cWriteBlocking(oled, cmd, sizeof(cmd), "oledDisplay");
        }

        // send one page of buffer to the display
        // i2c_start();
        // i2c_send(i2c_address << 1); // address + write
        // i2c_send(0x40); // data

        txBuf[0]    = 0x40;
        bufIndex    = 1;
        bytesToSend = oled->width + 1;

        if(oled->usingOffset){
        // send two dummy bytes if the width of the display
        //  is > 128 pixels
            txBuf[1]    = 0x00;
            txBuf[2]    = 0x00;
            bufIndex    = 3;
            bytesToSend += 2;
        }
    // copy the bit map from the structure buffer to our local buffer
        memcpy(&txBuf[bufIndex], &oled->buffer[index], oled->width);
        //i2c_write_blocking(oled->i2c, oled->i2c_address, txBuf, bytesToSend, false);
        myI2cWriteBlocking(oled, txBuf, bytesToSend, "oledDisplay");
        index += oled->width;
    }
}

void oledDraw_byte(t_OledParams *oled, uint8_t x, uint8_t y, uint8_t b, tColor color)
{
    // Invalid position
    if (x >= oled->width || y >= oled->height)
    {
        return;
    }

    uint_fast16_t buffer_index = y / 8 * oled->width + x;

    if (oled->fontInverted) {
        b^=255;
    }

    if (oled->color == WHITE)
    {
        // If the y position matches a page, then it goes quicker
        if (y % 8 == 0)
        {
            if (buffer_index < oled->bufsize)
            {
                oled->buffer[buffer_index] |= b;
            }
        }
        else
        {
            uint16_t w = (uint16_t) b << (y % 8);
            if (buffer_index < oled->bufsize)
            {
                oled->buffer[buffer_index] |= (w & 0xFF);
            }
            uint16_t buffer_index2 = buffer_index + oled->width;
            if (buffer_index2 < oled->bufsize)
            {
                oled->buffer[buffer_index2] |= (w >> 8);
            }
        }
    } else {
        // If the y position matches a page, then it goes quicker
        if (y % 8 == 0)
        {
            if (buffer_index < oled->bufsize)
            {
                oled->buffer[buffer_index] &= ~b;
            }
        } else {
            uint16_t w = (uint16_t) b << (y % 8);
            if (buffer_index < oled->bufsize)
            {
                oled->buffer[buffer_index] &= ~(w & 0xFF);
            }
            uint16_t buffer_index2 = buffer_index + oled->width;
            if (buffer_index2 < oled->bufsize)
            {
                oled->buffer[buffer_index2] &= ~(w >> 8);
            }
        }
    }
    return;
}

void oledDraw_bytes(t_OledParams *oled, uint8_t x, uint8_t y, const uint8_t* data, uint8_t size, tFontScaling scaling, tColor color, bool useProgmem)
{
    for (uint8_t column = 0; column < size; column++)
    {
        uint8_t b = *data;
        data++;

        if (scaling == DOUBLE_SIZE)
        {
            // Stretch vertically
            uint16_t w = 0;
            for (uint8_t bit = 0; bit < 8; bit++)
            {
                if (b & (1 << bit))
                {
                    uint8_t pos = bit << 1;
                    w |= ((1 << pos) | (1 << (pos + 1)));
                }
            }

            // Output 2 times to strech hozizontally            
            oledDraw_byte(oled, x, y, w & 0xFF, color);
            oledDraw_byte(oled, x, y + 8, (w >> 8), color);
            x++;
            oledDraw_byte(oled, x, y, w & 0xFF, color);
            oledDraw_byte(oled, x, y + 8, (w >> 8), color);
            x++;
        }
        else // NORMAL_SIZE
        {
            oledDraw_byte(oled, x++, y, b, color);
        }
    }
}

size_t oledDraw_character(t_OledParams *oled, uint8_t x, uint8_t y, char c, tFontScaling scaling, tColor color)
{
    // Invalid position
    if (x >= oled->width || y >= oled->height || c < 32)
    {
        return 0;
    }

    // Remap extended Latin1 character codes
    switch ((unsigned char) c)
    {
        case 252 /* u umlaut */:
            c = 127;
            break;
        case 220 /* U umlaut */:
            c = 128;
            break;
        case 228 /* a umlaut */:
            c = 129;
            break;
        case 196 /* A umlaut */:
            c = 130;
            break;
        case 246 /* o umlaut */:
            c = 131;
            break;
        case 214 /* O umlaut */:
            c = 132;
            break;
        case 176 /* degree   */:
            c = 133;
            break;
        case 223 /* szlig    */:
            c = 134;
            break;
    }

    uint16_t font_index = (c - 32)*6;

    // Invalid character code/font index
    if (font_index >= sizeof (oled_font6x8))
    {
        return 0;
    }

    oledDraw_bytes(oled, x, y, &oled_font6x8[font_index], 6, scaling, color, true);
    return 1;
}

void oledDraw_string(t_OledParams *oled, uint8_t x, uint8_t y, const char* s, tFontScaling scaling, tColor color)
{
    while (*s)
    {
        oledDraw_character(oled, x, y, *s, scaling, color);
        if (scaling == DOUBLE_SIZE)
        {
            x += 12;
        }
        else // NORMAL_SIZE
        {
            x += 6;
        }
        s++;
    }
}

// Use https://www.dcode.fr/binary-image to have a binary output with 0 and 1, then the python to group each 8 pixels in a byte
void oledDraw_simple_picture(t_OledParams *oled, uint8_t x0, uint8_t y0, uint8_t size_x,  uint8_t size_y, const uint8_t *data, bool invert)
{
    uint8_t x, y = 0;

    for (uint8_t start_y=0; start_y < size_y; start_y++) {
        for (uint8_t start_x=0; start_x < size_x; start_x+=8) {
            uint8_t d = *data;
            for (uint8_t x=0; x<8; x++) {
                if ((x0 + start_x + x) < size_x) {
                    oledDraw_pixel(oled, x0 + start_x + x, y0 + start_y + y, ((d>>(7-x))& 0x01) ? (invert ? BLACK:WHITE) : (invert ? WHITE:BLACK));
                }
            }
            data++;
        }
    }
}

void oledDraw_bitmap(t_OledParams *oled, uint8_t x, uint8_t y, uint8_t bitmap_width, uint8_t bitmap_height, const uint8_t* data, tColor color)
{
    uint8_t num_pages = (bitmap_height + 7) / 8;
    for (uint8_t page = 0; page < num_pages; page++)
    {
        oledDraw_bytes(oled, x, y, data, bitmap_width, NORMAL_SIZE, color, false);
        data += bitmap_width;
        y += 8;
    }
}

void oledDraw_bitmap_P(t_OledParams *oled, uint8_t x, uint8_t y, uint8_t bitmap_width, uint8_t bitmap_height, const uint8_t* data, tColor color)
{
    uint8_t num_pages = (bitmap_height + 7) / 8;
    for (uint8_t page = 0; page < num_pages; page++)
    {
        oledDraw_bytes(oled, x, y, data, bitmap_width, NORMAL_SIZE, color, true);
        data += bitmap_width;
        y += 8;
    }
}

void oledDraw_pixel(t_OledParams *oled, uint8_t x, uint8_t y, tColor color)
{
    if (x >= oled->width || y >= oled->height)
    {
        return;
    }

    if (color == WHITE)
    {
        oled->buffer[x + (y / 8) *oled-> width] |= (1 << (y & 7)); // set bit 
    }
    else
    {
        oled->buffer[x + (y / 8) * oled->width] &= ~(1 << (y & 7)); // clear bit
    }
}

void oledDraw_line(t_OledParams *oled, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, tColor color)
{        
    // Algorithm copied from Wikipedia
    int16_t dx = abs((int16_t)(x1) - (int16_t)(x0));
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t dy = -abs((int16_t)(y1) - (int16_t)(y0));
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy;
    int16_t e2;


    while (1)
    {
        oledDraw_pixel(oled, x0, y0, color);
        if (x0 == x1 && y0 == y1)
        {
            break;
        }
        e2 = 2 * err;
        if (e2 > dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

void oledDraw_circle(t_OledParams *oled, uint8_t x0, uint8_t y0, uint8_t radius, tFillmode fillMode, tColor color)
{
    // Algorithm copied from Wikipedia
    int_fast16_t f = 1 - radius;
    int_fast16_t ddF_x = 0;
    int_fast16_t ddF_y = -2 * radius;
    int_fast16_t x = 0;
    int_fast16_t y = radius;

    if (fillMode == SOLID)
    {
        oledDraw_pixel(oled, x0, y0 + radius, color);
        oledDraw_pixel(oled, x0, y0 - radius, color);
        oledDraw_line(oled, x0 - radius, y0, x0 + radius, y0, color);
    }
    else
    {
        oledDraw_pixel(oled, x0, y0 + radius, color);
        oledDraw_pixel(oled, x0, y0 - radius, color);
        oledDraw_pixel(oled, x0 + radius, y0, color);
        oledDraw_pixel(oled, x0 - radius, y0, color);
    }

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x + 1;

        if (fillMode == SOLID)
        {
            oledDraw_line(oled, x0 - x, y0 + y, x0 + x, y0 + y, color);
            oledDraw_line(oled, x0 - x, y0 - y, x0 + x, y0 - y, color);
            oledDraw_line(oled, x0 - y, y0 + x, x0 + y, y0 + x, color);
            oledDraw_line(oled, x0 - y, y0 - x, x0 + y, y0 - x, color);
        }
        else
        {
            oledDraw_pixel(oled, x0 + x, y0 + y, color);
            oledDraw_pixel(oled, x0 - x, y0 + y, color);
            oledDraw_pixel(oled, x0 + x, y0 - y, color);
            oledDraw_pixel(oled, x0 - x, y0 - y, color);
            oledDraw_pixel(oled, x0 + y, y0 + x, color);
            oledDraw_pixel(oled, x0 - y, y0 + x, color);
            oledDraw_pixel(oled, x0 + y, y0 - x, color);
            oledDraw_pixel(oled, x0 - y, y0 - x, color);
        }
    }
}

void oledDraw_rectangle(t_OledParams *oled, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, tFillmode fillMode, tColor color)
{
    // Swap x0 and x1 if in wrong order
    if (x0 > x1)
    {
        uint8_t tmp = x0;
        x0 = x1;
        x1 = tmp;
    }
    // Swap y0 and y1 if in wrong order
    if (y0 > y1)
    {
        uint8_t tmp = y0;
        y0 = y1;
        y1 = tmp;
    }
    if (fillMode == SOLID)
    {        
        for (uint8_t y = y0; y <= y1; y++)
        {
            oledDraw_line(oled, x0, y, x1, y, color);
        }
    }
    else
    {
        oledDraw_line(oled, x0, y0, x1, y0, color);
        oledDraw_line(oled, x0, y1, x1, y1, color);
        oledDraw_line(oled, x0, y0, x0, y1, color);
        oledDraw_line(oled, x1, y0, x1, y1, color);
    }
}

void oledScroll_up(t_OledParams *oled, uint8_t num_lines, uint8_t delay_ms)
{
    if (delay_ms == 0)
    {
        // Scroll full pages, fast algorithm
        uint8_t scroll_pages = num_lines / 8;
        for (uint8_t i = 0; i < oled->pages; i++)
        {
            for (uint8_t x = 0; x < oled->width; x++)
            {
                uint16_t index = i * oled->width + x;
                uint16_t index2 = (i + scroll_pages) * oled->width + x;
                if (index2 < oled->bufsize)
                {
                    oled->buffer[index] = oled->buffer[index2];
                }
                else
                {
                    oled->buffer[index] = 0;
                }
            }
        }
        num_lines -= scroll_pages * 8;
    }

    // Scroll the remainder line by line 
    bool need_refresh=true;
    if (num_lines > 0)
    {
        uint32_t start = to_ms_since_boot(get_absolute_time());
        uint16_t target_time = 0;
        
        for (uint8_t i = 0; i < num_lines; i++)
        {
            // Scroll everything 1 line up            
            for (uint8_t j = 0; j < oled->pages; j++)
            {
                uint16_t index = j*oled->width;
                uint16_t index2 = index + oled->width;
                for (uint8_t x = 0; x < oled->width; x++)
                {
                    uint8_t carry = 0;
                    if (index2 < oled->bufsize)
                    {
                        if (oled->buffer[index2] & 1)
                        {
                            carry = 128;
                        }
                    }
                    oled->buffer[index] = (oled->buffer[index] >> 1) | carry;
                    index++;
                    index2++;
                }
            }
            need_refresh    = true;
            target_time     += delay_ms;
            
            // Refresh the display only if we have some time
            uint32_t now = to_ms_since_boot(get_absolute_time());
            if ((now - start) < target_time)
            {
                oledDisplay(oled);
                need_refresh=false;
            }
                       
        }
    }
    
    if (need_refresh)
    {
        oledDisplay(oled);
    }
}


void oledSetCursor(t_OledParams *oled, uint8_t x, uint8_t y)
{
	if (oled->ttyMode) return; // in TTY mode position the cursor has no effect
	oled->X = x;
	oled->Y = y;
}

size_t oledPrintfXy(t_OledParams *oled, uint8_t x, uint8_t y, const char *format, ...) {
    va_list arg;
    va_start(arg, format);
    char temp[64];
    char* buffer = temp;
    size_t len = vsnprintf(temp, sizeof(temp), format, arg);
    va_end(arg);
    if (len > sizeof(temp) - 1) {
//        buffer = new char[len + 1];
        buffer = malloc(len + 1);
        if (!buffer) {
            return 0;
        }
        va_start(arg, format);
        vsnprintf(buffer, len + 1, format, arg);
        va_end(arg);
    }
    oled->X = x;
    oled->Y = y;

    len = oledWriteStr(oled, (const uint8_t*) buffer, len);
    if (buffer != temp) {
//        delete[] buffer;
        free(buffer);
    }

    return len;
}

size_t oledPrintf(t_OledParams *oled, const char *format, ...) {
    va_list arg;
    va_start(arg, format);
    char temp[64];
    char* buffer = temp;
    size_t len = vsnprintf(temp, sizeof(temp), format, arg);
    va_end(arg);
    if (len > sizeof(temp) - 1) {
        buffer = malloc(len + 1);
//        buffer = new char[len + 1];
        if (!buffer) {
            return 0;
        }
        va_start(arg, format);
        vsnprintf(buffer, len + 1, format, arg);
        va_end(arg);
    }
    len = oledWriteStr(oled, (const uint8_t*) buffer, len);
    if (buffer != temp) {
        free(buffer);
//        delete[] buffer;
    }

    return len;
}

size_t oledWriteStr(t_OledParams *oled, const uint8_t *buffer, size_t len)
{
    //size_t n = 0;
    for (size_t ix=0; ix<len; ix++)
    {
		// If CR and LF are found consecutive, only one is processed
    	// If two or more CR are consecutive, both are processed
    	// If two or more LF are consecutive, both are processed
    	if (buffer[ix] == '\r')
		{
        // carriage return found
    		oled->X = 0;
			oled->Y += (OLED_FONT_HEIGHT);
			if (buffer[ix+1] == '\n') {
				ix++; // skip char
			}
    		oled->X = 0;
		}
		else if (buffer[ix] == '\n')
		{
        // 'C' style CRLF
			oled->X = 0;                        // carriage return
			oled->Y += (OLED_FONT_HEIGHT);      // line feed
			if (buffer[ix+1] == '\r') {
				ix++; // skip char
			}
		}
		else if (*(buffer+ix) == '\f')
		{
			// FORM FEED
			oledScroll_up(oled, oled->height, 0);
			oled->X = 0;
			oled->Y = 0;
		}
		else
		{
			oledWriteChar(oled, buffer[ix]);
		}

		if (oled->ttyMode)
		{
			// Scroll up if cursor position is out of screen
			if (oled->Y >= oled->height)
			{
				oledScroll_up(oled, OLED_FONT_HEIGHT, 0);
				oled->Y = oled->height-OLED_FONT_HEIGHT;
			}
		}
    }
    if (oled->ttyMode) oledDisplay(oled);
    return len;
}

size_t oledWriteChar(t_OledParams *oled, uint8_t c)
{
	int n=1;
	n= oledDraw_character(oled, oled->X, oled->Y, c, oled->scaling, oled->color);
	oled->X+= OLED_FONT_WIDTH;
	return n;
}


void oledSetTTYMode(t_OledParams *oled, bool enabled)
{
	oled->ttyMode = enabled;
}

void oledUseOffset(t_OledParams *oled, bool enabled)
{
    if(oled->ctlrType == CTRL_SH1106){
        oled->usingOffset = enabled;
    }
}

/* oledBmp_get_val, oledBmp_show_image_with_offset and oledBmp_show_image_with_offset
    are reworked from ssd1306.c Copyright (c) 2021 David Schramm.
*/

uint32_t oledBmp_get_val(const uint8_t *data, const size_t offset, uint8_t size) {
    switch(size) {
    case 1:
        return data[offset];
    case 2:
        return data[offset]|(data[offset+1]<<8);
    case 4:
        return data[offset]|(data[offset+1]<<8)|(data[offset+2]<<16)|(data[offset+3]<<24);
    default:
        __builtin_unreachable();
    }
    __builtin_unreachable();
}

void oledBmp_show_image_with_offset(t_OledParams *oled, const uint8_t *data, const long size, uint32_t x_offset, uint32_t y_offset) {
    if(size<54) // data smaller than header
        return;

    const uint32_t bfOffBits        = oledBmp_get_val(data, 10, 4);
    const uint32_t biSize           = oledBmp_get_val(data, 14, 4);
    const int32_t biWidth           = (int32_t) oledBmp_get_val(data, 18, 4);
    const int32_t biHeight          = (int32_t) oledBmp_get_val(data, 22, 4);
    const uint16_t biBitCount       = (uint16_t) oledBmp_get_val(data, 28, 2);
    const uint32_t biCompression    = oledBmp_get_val(data, 30, 4);

    if(biBitCount!=1) // image not monochrome
        return;

    if(biCompression!=0) // image compressed
        return;

    const int table_start=14+biSize;
    uint8_t color_val;

    for(uint8_t i=0; i<2; ++i) {
        if(!((data[table_start+i*4]<<16)|(data[table_start+i*4+1]<<8)|data[table_start+i*4+2])) {
            color_val=i;
            break;
        }
    }

    uint32_t bytes_per_line=(biWidth/8)+(biWidth&7?1:0);
    if(bytes_per_line&3)
        bytes_per_line=(bytes_per_line^(bytes_per_line&3))+4;

    const uint8_t *img_data=data+bfOffBits;

    int step=biHeight>0?-1:1;
    int border=biHeight>0?-1:biHeight;
    for(uint32_t y=biHeight>0?biHeight-1:0; y!=border; y+=step) {
        for(uint32_t x=0; x<biWidth; ++x) {
            if(((img_data[x>>3]>>(7-(x&7)))&1)==color_val)
                oledDraw_pixel(oled, x_offset+x, y_offset+y, oled->color);
        }
        img_data+=bytes_per_line;
    }
}

void oledBmp_show_image(t_OledParams *oled, const uint8_t *data, const long size) {
    oledBmp_show_image_with_offset(oled, data, size, 0, 0);
}


// ************************ U8g2 wrappers ************************
void oledDrawString(t_OledParams *oled, uint8_t col, uint8_t row, const char* s, tFontScaling scaling, tColor color)
{
    oledDraw_string(oled, ToX(col),ToY(row),s,scaling,color);
}

void oledInverse(t_OledParams *oled)
{
    oledSet_font_inverted(oled, true);
}

void oledNoInverse(t_OledParams *oled)
{
    oledSet_font_inverted(oled, false);
}

// ************************ Tools func ************************

void oledSet_font_inverted(t_OledParams *oled, bool enabled)
{
    oled->fontInverted=enabled;
}

uint8_t oledToCol(uint8_t x)
{
    return(x/OLED_FONT_WIDTH);
}

uint8_t oledToRow(uint8_t y)
{
    return(y/OLED_FONT_HEIGHT);
}

uint8_t oledToX(uint8_t col)
{
    return(col*OLED_FONT_WIDTH);
}

uint8_t oledToY(uint8_t row)
{
    return(row*OLED_FONT_HEIGHT);
}

