; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc < %s -mtriple=x86_64-unknown-unknown -mattr=+avx | FileCheck %s

;
; testz(~X,Y) -> testc(X,Y)
;

define i32 @testpdz_128_invert(<2 x double> %c, <2 x double> %d, i32 %a, i32 %b) {
; CHECK-LABEL: testpdz_128_invert:
; CHECK:       # %bb.0:
; CHECK-NEXT:    movl %edi, %eax
; CHECK-NEXT:    vpcmpeqd %xmm2, %xmm2, %xmm2
; CHECK-NEXT:    vpxor %xmm2, %xmm0, %xmm0
; CHECK-NEXT:    vtestpd %xmm1, %xmm0
; CHECK-NEXT:    cmovnel %esi, %eax
; CHECK-NEXT:    retq
  %t0 = bitcast <2 x double> %c to <2 x i64>
  %t1 = xor <2 x i64> %t0, <i64 -1, i64 -1>
  %t2 = bitcast <2 x i64> %t1 to <2 x double>
  %t3 = call i32 @llvm.x86.avx.vtestz.pd(<2 x double> %t2, <2 x double> %d)
  %t4 = icmp ne i32 %t3, 0
  %t5 = select i1 %t4, i32 %a, i32 %b
  ret i32 %t5
}

define i32 @testpdz_256_invert(<4 x double> %c, <4 x double> %d, i32 %a, i32 %b) {
; CHECK-LABEL: testpdz_256_invert:
; CHECK:       # %bb.0:
; CHECK-NEXT:    movl %edi, %eax
; CHECK-NEXT:    vxorps %xmm2, %xmm2, %xmm2
; CHECK-NEXT:    vcmptrueps %ymm2, %ymm2, %ymm2
; CHECK-NEXT:    vxorps %ymm2, %ymm0, %ymm0
; CHECK-NEXT:    vtestpd %ymm1, %ymm0
; CHECK-NEXT:    cmovnel %esi, %eax
; CHECK-NEXT:    vzeroupper
; CHECK-NEXT:    retq
  %t0 = bitcast <4 x double> %c to <4 x i64>
  %t1 = xor <4 x i64> %t0, <i64 -1, i64 -1, i64 -1, i64 -1>
  %t2 = bitcast <4 x i64> %t1 to <4 x double>
  %t3 = call i32 @llvm.x86.avx.vtestz.pd.256(<4 x double> %t2, <4 x double> %d)
  %t4 = icmp ne i32 %t3, 0
  %t5 = select i1 %t4, i32 %a, i32 %b
  ret i32 %t5
}

;
; testc(~X,Y) -> testz(X,Y)
;

define i32 @testpdc_128_invert(<2 x double> %c, <2 x double> %d, i32 %a, i32 %b) {
; CHECK-LABEL: testpdc_128_invert:
; CHECK:       # %bb.0:
; CHECK-NEXT:    movl %edi, %eax
; CHECK-NEXT:    vpcmpeqd %xmm2, %xmm2, %xmm2
; CHECK-NEXT:    vpxor %xmm2, %xmm0, %xmm0
; CHECK-NEXT:    vtestpd %xmm1, %xmm0
; CHECK-NEXT:    cmovael %esi, %eax
; CHECK-NEXT:    retq
  %t0 = bitcast <2 x double> %c to <2 x i64>
  %t1 = xor <2 x i64> %t0, <i64 -1, i64 -1>
  %t2 = bitcast <2 x i64> %t1 to <2 x double>
  %t3 = call i32 @llvm.x86.avx.vtestc.pd(<2 x double> %t2, <2 x double> %d)
  %t4 = icmp ne i32 %t3, 0
  %t5 = select i1 %t4, i32 %a, i32 %b
  ret i32 %t5
}

define i32 @testpdc_256_invert(<4 x double> %c, <4 x double> %d, i32 %a, i32 %b) {
; CHECK-LABEL: testpdc_256_invert:
; CHECK:       # %bb.0:
; CHECK-NEXT:    movl %edi, %eax
; CHECK-NEXT:    vxorps %xmm2, %xmm2, %xmm2
; CHECK-NEXT:    vcmptrueps %ymm2, %ymm2, %ymm2
; CHECK-NEXT:    vxorps %ymm2, %ymm0, %ymm0
; CHECK-NEXT:    vtestpd %ymm1, %ymm0
; CHECK-NEXT:    cmovael %esi, %eax
; CHECK-NEXT:    vzeroupper
; CHECK-NEXT:    retq
  %t0 = bitcast <4 x double> %c to <4 x i64>
  %t1 = xor <4 x i64> %t0, <i64 -1, i64 -1, i64 -1, i64 -1>
  %t2 = bitcast <4 x i64> %t1 to <4 x double>
  %t3 = call i32 @llvm.x86.avx.vtestc.pd.256(<4 x double> %t2, <4 x double> %d)
  %t4 = icmp ne i32 %t3, 0
  %t5 = select i1 %t4, i32 %a, i32 %b
  ret i32 %t5
}

;
; testnzc(~X,Y) -> testnzc(X,Y)
;

define i32 @testpdnzc_128_invert(<2 x double> %c, <2 x double> %d, i32 %a, i32 %b) {
; CHECK-LABEL: testpdnzc_128_invert:
; CHECK:       # %bb.0:
; CHECK-NEXT:    movl %edi, %eax
; CHECK-NEXT:    vpcmpeqd %xmm2, %xmm2, %xmm2
; CHECK-NEXT:    vpxor %xmm2, %xmm0, %xmm0
; CHECK-NEXT:    vtestpd %xmm1, %xmm0
; CHECK-NEXT:    cmovbel %esi, %eax
; CHECK-NEXT:    retq
  %t0 = bitcast <2 x double> %c to <2 x i64>
  %t1 = xor <2 x i64> %t0, <i64 -1, i64 -1>
  %t2 = bitcast <2 x i64> %t1 to <2 x double>
  %t3 = call i32 @llvm.x86.avx.vtestnzc.pd(<2 x double> %t2, <2 x double> %d)
  %t4 = icmp ne i32 %t3, 0
  %t5 = select i1 %t4, i32 %a, i32 %b
  ret i32 %t5
}

define i32 @testpdnzc_256_invert(<4 x double> %c, <4 x double> %d, i32 %a, i32 %b) {
; CHECK-LABEL: testpdnzc_256_invert:
; CHECK:       # %bb.0:
; CHECK-NEXT:    movl %edi, %eax
; CHECK-NEXT:    vxorps %xmm2, %xmm2, %xmm2
; CHECK-NEXT:    vcmptrueps %ymm2, %ymm2, %ymm2
; CHECK-NEXT:    vxorps %ymm2, %ymm0, %ymm0
; CHECK-NEXT:    vtestpd %ymm1, %ymm0
; CHECK-NEXT:    cmovbel %esi, %eax
; CHECK-NEXT:    vzeroupper
; CHECK-NEXT:    retq
  %t0 = bitcast <4 x double> %c to <4 x i64>
  %t1 = xor <4 x i64> %t0, <i64 -1, i64 -1, i64 -1, i64 -1>
  %t2 = bitcast <4 x i64> %t1 to <4 x double>
  %t3 = call i32 @llvm.x86.avx.vtestnzc.pd.256(<4 x double> %t2, <4 x double> %d)
  %t4 = icmp ne i32 %t3, 0
  %t5 = select i1 %t4, i32 %a, i32 %b
  ret i32 %t5
}

declare i32 @llvm.x86.avx.vtestz.pd(<2 x double>, <2 x double>) nounwind readnone
declare i32 @llvm.x86.avx.vtestc.pd(<2 x double>, <2 x double>) nounwind readnone
declare i32 @llvm.x86.avx.vtestnzc.pd(<2 x double>, <2 x double>) nounwind readnone

declare i32 @llvm.x86.avx.vtestz.pd.256(<4 x double>, <4 x double>) nounwind readnone
declare i32 @llvm.x86.avx.vtestc.pd.256(<4 x double>, <4 x double>) nounwind readnone
declare i32 @llvm.x86.avx.vtestnzc.pd.256(<4 x double>, <4 x double>) nounwind readnone
