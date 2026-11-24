/* SPDX-License-Identifier: GPL-3.0-only */
#include "wcp_wire.h"
static int kind_valid(uint8_t kind,const uint8_t *p,size_t n){
 size_t i;unsigned has_nonzero=0,has_nonff=0;
 if(kind==WC_RF_DATA)return n>0&&p!=0;
 if(kind==WC_RF_DISCONNECTED)return n==0;
 if(kind!=WC_RF_CONNECTED||n!=6||!p||(p[0]&1u)!=0)return 0;
 for(i=0;i<n;++i){has_nonzero|=p[i]!=0;has_nonff|=p[i]!=0xff;}
 return has_nonzero&&has_nonff;
}
int wc_wire_decode(const uint8_t *b,size_t n,WcWireFrame *out){
 size_t length;uint32_t generation;
 if(!b||!out||n<WC_RF_HEADER||b[1]!=0)return -1;
 length=(size_t)b[2] | ((size_t)b[3]<<8);
 generation=(uint32_t)b[4]|((uint32_t)b[5]<<8)|((uint32_t)b[6]<<16)|((uint32_t)b[7]<<24);
 if(length>WC_RF_MAX||n!=WC_RF_HEADER+length||generation==0||
    !kind_valid(b[0],b+WC_RF_HEADER,length))return -1;
 out->kind=b[0];out->generation=generation;out->data=b+WC_RF_HEADER;out->size=length;
 return 0;
}
int wc_wire_encode(uint8_t k,uint32_t gen,const uint8_t*data,size_t n,
                   uint8_t *out,size_t cap,size_t *written){
 size_t i;WcWireFrame verify;
 if(!out||!written||gen==0||n>WC_RF_MAX||cap<WC_RF_HEADER+n||
    !kind_valid(k,data,n))return -1;
 out[0]=k;out[1]=0;out[2]=(uint8_t)(n&0xffu);out[3]=(uint8_t)((n>>8)&0xffu);
 for(i=0;i<4;++i)out[4+i]=(uint8_t)((gen>>(8*i))&0xffu);
 for(i=0;i<n;++i)out[WC_RF_HEADER+i]=data[i];
 *written=WC_RF_HEADER+n;
 return wc_wire_decode(out,*written,&verify);
}
