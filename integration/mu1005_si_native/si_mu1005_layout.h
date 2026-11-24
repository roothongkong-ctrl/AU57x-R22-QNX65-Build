#ifndef AU57X_SI_MU1005_LAYOUT_H
#define AU57X_SI_MU1005_LAYOUT_H
#include <stddef.h>
#include <stdint.h>
#define MU_RECORD_ALLOCATION 168u
#define MU_RECORD_STRING_COUNT 3u
#define MU_RECORD_ARRAY_COUNT_OFF 464u
#define MU_RECORD_ARRAY_PTR_OFF 472u
#define MU_RECORD_LIMIT 100u
#define MU_SI_OK 0
#define MU_SI_INVALID -1
#define MU_SI_CHANGED -2
#define MU_SI_NOT_FOUND -3
struct mu_si_reader { void *context; int (*read)(void*,uint32_t,void*,size_t); };
struct mu_si_record_ref { uint32_t address,id; };
int mu_si_find_record(const struct mu_si_reader*,uint32_t proxy,uint32_t id,struct mu_si_record_ref*);
int mu_si_read_record_ids(const struct mu_si_reader*,uint32_t proxy,struct mu_si_record_ref*,size_t,size_t*);
#endif
