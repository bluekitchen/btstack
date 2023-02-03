
#include "bootutil/bootutil.h"
#include "bootutil/image.h"
#include "bootutil/bootutil_log.h"
#include "port/boot_startup_port.h"

typedef void (*pFunction)(void);
pFunction JumpToApplication;

static void start_app(uint32_t pc, uint32_t sp) {
    if ((sp & 0x2FFE0000 ) == 0x20020000)
    {
        printf("before system deinit\n");
        void system_deinit(void);
        system_deinit();
        printf("after system deinit\n");
#if 0
        uint32_t JumpAddress;
        printf("before get jumpaddr\n");
        /* Jump to user application */
        JumpAddress = *(volatile uint32_t*) (sp + 4);
        printf("jump_addr:0x%x\n", JumpAddress);
        JumpToApplication = (pFunction) JumpAddress;
        /* Initialize user application's Stack Pointer */
        __asm volatile ("MSR msp, %0" : : "r" (sp) : );
        JumpToApplication();
#else
        __asm volatile ("MSR msp, %0" : : "r" (sp) : );
        void (*application_reset_handler)(void) = (void *)pc;
        application_reset_handler();
#endif
    }

}

static void do_boot(struct boot_rsp *rsp) {
  MCUBOOT_LOG_INF("Starting Main Application");
  MCUBOOT_LOG_INF("Image Start Offset: 0x%x", (int)rsp->br_image_off);

  // We run from internal flash. Base address of this medium is 0x0
  uint32_t vector_table = 0x0 + rsp->br_image_off + rsp->br_hdr->ih_hdr_size;

  uint32_t *app_code = (uint32_t *)vector_table;
  uint32_t app_sp = app_code[0];
  uint32_t app_start = app_code[1];

  MCUBOOT_LOG_INF("Vector Table Start Address: 0x%x. PC=0x%x, SP=0x%x",
    (int)vector_table, app_start, app_sp);

  // We need to move the vector table to reflect the location of the main application
  volatile uint32_t *vtor = (uint32_t *)0xE000ED08; //0xE000ED08:SCB->VTOR
  *vtor = (vector_table & 0xFFFFFFF8);
  start_app(app_start, app_sp);
  while (1) {
      printf("after jump to app, should never get to there\n");
  }
}

void boot_port_init( void )
{
//    bootloader_init();
}

/** Boots firmware at specific address */
void boot_port_startup( struct boot_rsp *rsp )
{
    MCUBOOT_LOG_INF("br_image_off = 0x%x", rsp->br_image_off);
    MCUBOOT_LOG_INF("ih_hdr_size = 0x%x", rsp->br_hdr->ih_hdr_size);
//    int slot = (rsp->br_image_off == CONFIG_ESP_APPLICATION_PRIMARY_START_ADDRESS) ? 0 : 1;
//    esp_app_image_load(slot, rsp->br_hdr->ih_hdr_size);
    do_boot(rsp);
}
