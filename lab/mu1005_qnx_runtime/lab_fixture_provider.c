/* LAB ONLY: existing synthetic test provider, never an OEM replacement.
 * Built only into the disposable QNX test image. */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
static unsigned cert_calls,signature_calls;
int APSMFiPlatform_CopyCertificate(uint8_t **bytes,size_t *len){
 *bytes=malloc(4);if(!*bytes)return -1;
 memcpy(*bytes,"CERT",4);*len=4;cert_calls++;return 0;
}
int APSMFiPlatform_CreateSignature(const uint8_t *challenge,size_t len,
                                   uint8_t **bytes,size_t *out){
 if(len!=20)return -1;
 *bytes=malloc(20);if(!*bytes)return -1;
 for(unsigned i=0;i<20;i++)(*bytes)[i]=(uint8_t)(challenge[i]^0x55);
 *out=20;signature_calls++;return 0;
}
unsigned mu_lab_cert_calls(void){return cert_calls;}
unsigned mu_lab_signature_calls(void){return signature_calls;}
