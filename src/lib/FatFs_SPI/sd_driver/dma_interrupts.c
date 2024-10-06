
#include "hw_config.h"
#include "sd_card.h"
//
#include "dma_interrupts.h"

static void dma_irq_handler(const uint DMA_IRQ_num, io_rw_32 *dma_hw_ints_p) {
    // Iterate through all of the SD cards
    for (size_t i = 0; i < sd_get_num(); ++i) {
        sd_card_t *sd_card_p = sd_get_by_num(i);
        if (!sd_card_p)
            continue;
        uint irq_num = 0, channel = 0;
        if (SD_IF_SDIO == sd_card_p->type) {
            irq_num = sd_card_p->sdio_if_p->DMA_IRQ_num;
            channel = sd_card_p->sdio_if_p->state.SDIO_DMA_CHB;
        }
        // Is this channel requesting interrupt?
        if (irq_num == DMA_IRQ_num && (*dma_hw_ints_p & (1 << channel))) {
            *dma_hw_ints_p = 1 << channel;  // Clear it.
            if (SD_IF_SDIO == sd_card_p->type) {
                sdio_irq_handler(sd_card_p);
            }
        }
    }
}
static void __not_in_flash_func(dma_irq_handler_0)() {
    dma_irq_handler(DMA_IRQ_0, &dma_hw->ints0);
}
static void __not_in_flash_func(dma_irq_handler_1)() {
    dma_irq_handler(DMA_IRQ_1, &dma_hw->ints1);
}

/* Adding the interrupt request handler 
    Only add it once.
    Otherwise, space is wasted in irq_add_shared_handler's table.
    Also, each core maintains its own table of interrupt vectors,
    and we don't want both cores to call the IRQ handler.
*/
typedef struct ih_added_rec_t {
    uint num;
    bool added;
} ih_added_rec_t;
static ih_added_rec_t ih_added_recs[] = {
    {DMA_IRQ_0, false},
    {DMA_IRQ_1, false}};
static bool is_handler_added(const uint num) {
    for (size_t i = 0; i < count_of(ih_added_recs); ++i)
        if (num == ih_added_recs[i].num)
            return ih_added_recs[i].added;
    return false;
}
static void mark_handler_added(const uint num) {
    size_t i;
    for (i = 0; i < count_of(ih_added_recs); ++i)
        if (num == ih_added_recs[i].num) {
            ih_added_recs[i].added = true;
            break;
        }
    myASSERT(i < count_of(ih_added_recs));
}
void dma_irq_add_handler(const uint num, bool exclusive) {
    if (!is_handler_added(num)) {        
        static void (*irq_handler)();
        switch (num) {
            case DMA_IRQ_0:
                irq_handler = dma_irq_handler_0;
                break;
            case DMA_IRQ_1:
                irq_handler = dma_irq_handler_1;
                break;
            default:
                myASSERT(false);
        }
        if (exclusive) {
            irq_set_exclusive_handler(num, *irq_handler);
        } else {
            irq_add_shared_handler(
                num, *irq_handler,
                PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
        }
        irq_set_enabled(num, true); // Enable IRQ in NVIC
        mark_handler_added(num);
    }
}
