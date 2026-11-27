.syntax unified
.arm
.text
.align 2
.global _ZN4util6config10JSONParser16parseFileToArrayEPKcPPNS0_5ArrayEj
.type _ZN4util6config10JSONParser16parseFileToArrayEPKcPPNS0_5ArrayEj,%function
_ZN4util6config10JSONParser16parseFileToArrayEPKcPPNS0_5ArrayEj:
 push {r0-r4,lr}
 mov r0,r1
 bl step8_observe
 mov ip,r0
 pop {r0-r4,lr}
 bx ip
.size _ZN4util6config10JSONParser16parseFileToArrayEPKcPPNS0_5ArrayEj,.-_ZN4util6config10JSONParser16parseFileToArrayEPKcPPNS0_5ArrayEj
