#ifndef MU1005_SEMANTIC_H
#define MU1005_SEMANTIC_H
#include <stdint.h>
#include <sys/types.h>
#include <stddef.h>
/* Each service owns exactly one immutable SI admission. Callbacks execute on
 * native owner only: 1 accepted, 0 retry without side effects, -1 rejected. */
typedef struct {
 uint32_t input_seq, output_seq;
 size_t input_size,output_size;
 unsigned char input[8192], output[512];
} MuSemantic;
int mu_sem_input(MuSemantic *,const unsigned char *,size_t,void *,int (*)(void *,unsigned,const unsigned char *,size_t));
int mu_sem_output(MuSemantic *,uint32_t,void *,int (*)(void *,unsigned char *,size_t));
int mu_sem_csm(const unsigned char *,size_t);
#endif
