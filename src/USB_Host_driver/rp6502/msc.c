/*
 * Copyright (c) 2023 Rumbledethumps
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "main.h"
#include "tusb.h"
#include "usb/usb.h"
#include "fatfs/ff.h"
#include "fatfs/diskio.h"
#include <math.h>

// We are an 8-bit computer, confirm fatfs is too
static_assert(sizeof(TCHAR) == sizeof(char));

static FATFS msc_fatfs_volumes[FF_VOLUMES];
static volatile bool msc_volume_busy[FF_VOLUMES];
static scsi_inquiry_resp_t msc_inquiry_resp;

bool inquiry_complete_cb(uint8_t dev_addr, tuh_msc_complete_data_t const *cb_data)
{
    if (cb_data->csw->status != 0)
    {
        usb_set_status(dev_addr, "?MSC SCSI inquiry failed");
        return false;
    }
    const double block_count = tuh_msc_get_block_count(dev_addr, cb_data->cbw->lun);
    const double block_size = tuh_msc_get_block_size(dev_addr, cb_data->cbw->lun);
    const char *xb = "MB";
    double size = block_count * block_size / (1024 * 1024);
    if (size >= 1000)
    {
        xb = "GB";
        size /= 1024;
    }
    if (size >= 1000)
    {
        xb = "TB";
        size /= 1024;
    }
    size = ceil(size * 10) / 10;
    usb_set_status(dev_addr, "MSC %.1f %s %.8s %.16s rev %.4s",
                   size, xb,
                   msc_inquiry_resp.vendor_id,
                   msc_inquiry_resp.product_id,
                   msc_inquiry_resp.product_rev);
    char drive_path[3] = "0:";
    drive_path[0] += dev_addr;
    FRESULT mount_result = f_mount(&msc_fatfs_volumes[dev_addr], drive_path, 1);
    if (mount_result != FR_OK)
    {
        usb_set_status(dev_addr, "?MSC filesystem mount failed (%d)", mount_result);
        return false;
    }
    // If current directory invalid, change to root of this drive
    char s[2];
    if (FR_OK != f_getcwd(s, 2))
    {
        f_chdrive(drive_path);
        f_chdir("/");
    }
    return true;
}

void tuh_msc_mount_cb(uint8_t dev_addr)
{
    uint8_t const lun = 0;
    usb_set_status(dev_addr, "MSC mounted, inquiring");
    tuh_msc_inquiry(dev_addr, lun, &msc_inquiry_resp, inquiry_complete_cb, 0);
}

void tuh_msc_umount_cb(uint8_t dev_addr)
{
    char drive_path[3] = "0:";
    drive_path[0] += dev_addr;
    f_unmount(drive_path);
}

static void wait_for_disk_io(BYTE pdrv)
{
    while (msc_volume_busy[pdrv])
        main_task();
}

static bool disk_io_complete(uint8_t dev_addr, tuh_msc_complete_data_t const *cb_data)
{
    (void)cb_data;
    msc_volume_busy[dev_addr] = false;
    return true;
}

DSTATUS disk_status(BYTE pdrv)
{
    uint8_t dev_addr = pdrv;
    return tuh_msc_mounted(dev_addr) ? 0 : STA_NODISK;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    (void)(pdrv);
    return 0;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    uint8_t const dev_addr = pdrv;
    uint8_t const lun = 0;
    msc_volume_busy[pdrv] = true;
    tuh_msc_read10(dev_addr, lun, buff, sector, (uint16_t)count, disk_io_complete, 0);
    wait_for_disk_io(pdrv);
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    uint8_t const dev_addr = pdrv;
    uint8_t const lun = 0;
    msc_volume_busy[pdrv] = true;
    tuh_msc_write10(dev_addr, lun, buff, sector, (uint16_t)count, disk_io_complete, 0);
    wait_for_disk_io(pdrv);
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    uint8_t const dev_addr = pdrv;
    uint8_t const lun = 0;
    switch (cmd)
    {
    case CTRL_SYNC:
        return RES_OK;
    case GET_SECTOR_COUNT:
        *((DWORD *)buff) = (WORD)tuh_msc_get_block_count(dev_addr, lun);
        return RES_OK;
    case GET_SECTOR_SIZE:
        *((WORD *)buff) = (WORD)tuh_msc_get_block_size(dev_addr, lun);
        return RES_OK;
    case GET_BLOCK_SIZE:
        *((DWORD *)buff) = 1; // 1 sector
        return RES_OK;
    default:
        return RES_PARERR;
    }
}
