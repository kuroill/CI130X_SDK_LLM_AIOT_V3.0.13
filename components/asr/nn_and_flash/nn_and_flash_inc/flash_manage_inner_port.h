#ifndef __FLASH_MANAGE_INNER_PORT_H
#define __FLASH_MANAGE_INNER_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void flash_manage_nuclear_com_inner_port_init(void);
void post_read_flash_int_op_done(void);
void post_erase_flash_int_op_done(void);
void post_write_flash_int_op_done(void);
void post_read_unique_id_int_op_done(void);


#ifdef __cplusplus
}
#endif

#endif 




