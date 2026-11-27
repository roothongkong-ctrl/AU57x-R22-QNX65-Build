#include "si_mu1005_registration.h"
#include <stdint.h>
int mu_si_register_prepared(const struct mu_reg_ops *ops,uint32_t proxy,
    uint64_t epoch,uint32_t session,void *key,void *info,size_t size,uint32_t *id){
    struct mu_si_record_ref before[MU_RECORD_LIMIT],after[MU_RECORD_LIMIT];
    size_t count=0,after_count=0;int32_t native_id;uintptr_t p=(uintptr_t)info,k=(uintptr_t)key;
    if(!id)return MU_REG_DENIED;
    *id=0;
    if(!ops||!ops->owner_session_current||!ops->add_unique_id||!ops->update_record||
       !ops->reader.read||!epoch||!session||!key||!info||size!=MU_INFO_SIZE||
       (p&7u)||(k&3u)||p>UINTPTR_MAX-size||k>UINTPTR_MAX-40u||
       (k<p+size&&p<k+40u))return MU_REG_DENIED;
    if(ops->owner_session_current(ops->context,proxy,epoch,session)!=1||
       mu_si_read_record_ids(&ops->reader,proxy,before,MU_RECORD_LIMIT,&count)!=MU_SI_OK||
       count>=MU_RECORD_LIMIT||ops->owner_session_current(ops->context,proxy,epoch,session)!=1)return MU_REG_DENIED;
    /* MU1005 addUniqueID consumes THREE arguments (r0,r1,r2), unlike donor.
     * No fallback to the donor two-argument function is permitted. */
    native_id=ops->add_unique_id((void *)(uintptr_t)proxy,key,(unsigned char*)info+MU_INFO_STRING0);
    if(native_id<=0||ops->owner_session_current(ops->context,proxy,epoch,session)!=1)return MU_REG_UNCERTAIN;
    for(size_t i=0;i<count;i++)if(before[i].id==(uint32_t)native_id)return MU_REG_UNCERTAIN;
    ops->update_record((void *)(uintptr_t)proxy,(uint32_t)native_id,info);
    if(ops->owner_session_current(ops->context,proxy,epoch,session)!=1||
       mu_si_read_record_ids(&ops->reader,proxy,after,MU_RECORD_LIMIT,&after_count)!=MU_SI_OK||
       after_count!=count+1||ops->owner_session_current(ops->context,proxy,epoch,session)!=1)return MU_REG_UNCERTAIN;
    /* Record allocations may move; preserve all original IDs and verify the
     * new ID, rather than assuming add/update cannot displace another peer. */
    int found=0;
    for(size_t j=0;j<after_count;j++)if(after[j].id==(uint32_t)native_id)found=1;
    if(!found)return MU_REG_UNCERTAIN;
    for(size_t i=0;i<count;i++){
        found=0;
        for(size_t j=0;j<after_count;j++)if(after[j].id==before[i].id)found=1;
        if(!found)return MU_REG_UNCERTAIN;
    }
    if(ops->owner_session_current(ops->context,proxy,epoch,session)!=1)return MU_REG_UNCERTAIN;
    *id=(uint32_t)native_id;return MU_SI_OK;
}
