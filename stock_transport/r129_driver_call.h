#ifndef R129_DRIVER_CALL_H
#define R129_DRIVER_CALL_H
#include <stdint.h>
#include <stddef.h>
#include "r125_module.h"
/* Original driver invocation adapter. The caller must provide independently
 * verified process identity, live readable ranges and serialized ownership.
 * No process hook, no MFi bypass, no default authorization. */
typedef struct {
 void *context;
 int (*certify)(void *,void *device,void *driver,void *handle);
 int (*read_live)(void *,uintptr_t,void *,size_t);
 int (*authorize)(void *,void *,uint32_t);
 int (*teardown)(void *,void *);
} R129Evidence;
typedef struct {
 R129Evidence proof;
 void *device,*driver,*handle;
 int (*start)(void *,char *);
 int (*receive)(void *);
 uintptr_t options;
 char opts[1024];
 int bound,started,last_input;
} R129Driver;
/* Zero initialize; caller owns lifetime. The handle must already belong to
 * the original loaded driver and must remain resident. Never dlopen OEM here. */
int r129_driver_bind(R129Driver *,void *device,void *driver_handle,const R129Evidence *,R125Owner *);
#endif
