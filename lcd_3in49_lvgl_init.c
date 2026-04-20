/// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-
#include <stdio.h>
#include "pico/stdlib.h"
#include "DEV_Config.h"
#include "LCD_3in49.h"
#include "qspi_pio.h"
#include "lvgl.h"

#define DISP_HOR_RES 172
#define DISP_VER_RES 640

static lv_display_t *disp;
static void *buf1, *buf2;

static struct repeating_timer lvgl_timer;

static bool repeating_lvgl_timer_callback(struct repeating_timer *t) 
{
    lv_tick_inc(5);
    return true;
}

#if 0
static uint8_t *next_px_map;
static uint32_t line_size;
static int line_count;

static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    // Set the LVGL interface display position
    LCD_3IN49_SetWindows(area->x1, area->y1, area->x2+1 , area->y2+1);
    QSPI_Select(qspi);
    QSPI_Pixel_Write(qspi, 0x2c);

    line_size = (area->x2 + 1 - area->x1) * 2;
    next_px_map = px_map + line_size;
    line_count = area->y2 - area->y1;
    dma_channel_configure(dma_tx, &c, &qspi.pio->txf[qspi.sm], px_map, line_size, true);
}

static void dma_handler(void)
{
    if (dma_channel_get_irq0_status(dma_tx)) 
        {
            dma_channel_acknowledge_irq0(dma_tx);
            if (line_count > 0) {
                line_count--;
                uint8_t *px_map = next_px_map;
                next_px_map = px_map + line_size;
                dma_channel_configure(dma_tx, &c, &qspi.pio->txf[qspi.sm], px_map, line_size, true);
            } else {
                QSPI_Deselect(qspi);
                // Indicate you are ready with the flushing
                lv_disp_flush_ready(disp);
            }
        }
}
#else
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    // Send command in one-line mode
    // QSPI_1Wrie_Mode(&qspi);
    // printf("flush_cb: x1:%d, y1:%d, x2:%d, y2:%d\n", area->x1, area->y1, area->x2, area->y2);
    LCD_3IN49_SetWindows(area->x1, area->y1, area->x2+1 , area->y2+1);  // Set the LVGL interface display position
    QSPI_Select(qspi);
    QSPI_Pixel_Write(qspi, 0x2c);

    // Four-wire mode sends RGB data
    // QSPI_4Wrie_Mode(&qspi);
    dma_channel_configure(dma_tx,   
                          &c,
                          &qspi.pio->txf[qspi.sm], 
                          px_map, // read address
                          ((area->x2 + 1 - area->x1) * (area->y2 + 1 - area->y1))*2,
                          true);// Start DMA transfer

    // // Waiting for DMA transfer to complete
    // while(dma_channel_is_busy(dma_tx));  
    // QSPI_Deselect(qspi);   
}

static void dma_handler(void)
{
    if (dma_channel_get_irq0_status(dma_tx)) 
        {
            dma_channel_acknowledge_irq0(dma_tx);
            QSPI_Deselect(qspi);   
            lv_disp_flush_ready(disp); // Indicate you are ready with the flushing
        }
}
#endif

int lcd_3in49_lvgl_init(void)
{
    /*QSPI PIO Init*/
    QSPI_GPIO_Init(qspi);
    QSPI_PIO_Init(qspi);
    QSPI_4Wrie_Mode(&qspi);
    /*Init LCD*/
    LCD_3IN49_Init();
    DEV_SET_PWM(60);
    LCD_3IN49_Clear(WHITE);

    // Init LVGL
    add_repeating_timer_ms(5,  repeating_lvgl_timer_callback,  NULL, &lvgl_timer);
    lv_init();

    size_t buf_size_in_bytes = DISP_HOR_RES * DISP_VER_RES * sizeof(lv_color_t) / 8;
    buf1 = malloc(buf_size_in_bytes);
    buf2 = malloc(buf_size_in_bytes);
    disp = lv_display_create(DISP_HOR_RES, DISP_VER_RES);
    lv_display_set_flush_cb(disp, disp_flush_cb);
    lv_display_set_buffers(disp, buf1, buf2, buf_size_in_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_obj_clean(lv_screen_active());

    // Init DMA
    dma_channel_set_irq0_enabled(dma_tx, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    return 0;
}

