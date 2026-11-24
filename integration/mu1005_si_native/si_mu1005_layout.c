#include "si_mu1005_layout.h"
#include <string.h>
static int ptr_ok(uint32_t p) {return p>=0x10000u && p<=0xfffffffbu && !(p&3u);}
static int read_u32(const struct mu_si_reader*r,uint32_t p,uint32_t*v){
 unsigned char b[4];if(!r||!r->read||!ptr_ok(p)||r->read(r->context,p,b,4)!=1)return 0;
 *v=(uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);return 1;
}
static int read_header(const struct mu_si_reader*r,uint32_t proxy,uint32_t*n,uint32_t*a){
 if(!ptr_ok(proxy)||proxy>UINT32_MAX-MU_RECORD_ARRAY_PTR_OFF-4u)return 0;
 if(!read_u32(r,proxy+MU_RECORD_ARRAY_COUNT_OFF,n)||!read_u32(r,proxy+MU_RECORD_ARRAY_PTR_OFF,a)||*n>MU_RECORD_LIMIT)return 0;
 return !*n||(ptr_ok(*a)&&*a<=UINT32_MAX-4u*(*n));
}
int mu_si_read_record_ids(const struct mu_si_reader*r,uint32_t proxy,struct mu_si_record_ref*out,size_t cap,size_t*count){
 struct mu_si_record_ref tmp[MU_RECORD_LIMIT];uint32_t n,a,n2,a2,p,id;
 if(!count)return MU_SI_INVALID;
 *count=0;
 if(!read_header(r,proxy,&n,&a)||n>cap||(n&&!out))return MU_SI_INVALID;
 for(uint32_t i=0;i<n;i++){
  if(!read_u32(r,a+4u*i,&p)||!read_u32(r,p,&id)||!id)return MU_SI_INVALID;
  for(uint32_t j=0;j<i;j++)if(tmp[j].address==p||tmp[j].id==id)return MU_SI_INVALID;
  tmp[i]=(struct mu_si_record_ref){p,id};
 }
 if(!read_header(r,proxy,&n2,&a2)||n!=n2||a!=a2)return MU_SI_CHANGED;
 for(uint32_t i=0;i<n;i++){
  if(!read_u32(r,a+4u*i,&p)||p!=tmp[i].address||!read_u32(r,p,&id)||id!=tmp[i].id)return MU_SI_CHANGED;
 }
 if(!read_header(r,proxy,&n2,&a2)||n!=n2||a!=a2)return MU_SI_CHANGED;
 if(n)memcpy(out,tmp,n*sizeof(*out));
 *count=n;return MU_SI_OK;
}
int mu_si_find_record(const struct mu_si_reader*r,uint32_t proxy,uint32_t id,struct mu_si_record_ref*out){
 struct mu_si_record_ref tmp[MU_RECORD_LIMIT];size_t n;int rc;
 if(!id||!out)return MU_SI_INVALID;
 memset(out,0,sizeof(*out));rc=mu_si_read_record_ids(r,proxy,tmp,MU_RECORD_LIMIT,&n);
 if(rc!=MU_SI_OK)return rc;
 for(size_t i=0;i<n;i++)if(tmp[i].id==id){*out=tmp[i];return MU_SI_OK;}
 return MU_SI_NOT_FOUND;
}
