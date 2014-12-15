; RUN: opt < %s -S -inline | FileCheck %s
;
; The purpose of this test is to check that inline pass preserves debug info
; for variable using the dbg.declare intrinsic.
;
;; This test was generated by running this command:
;; clang.exe -S -O0 -emit-llvm -g foo.c
;;
;; foo.c
;; ==========================
;; float foo(float x)
;; {
;;    return x;
;; }
;;
;; void bar(float *dst)
;; {
;;    dst[0] = foo(dst[0]);
;; }
;; ==========================

target datalayout = "e-m:w-p:32:32-i64:64-f80:32-n8:16:32-S32"
target triple = "i686-pc-windows-msvc"

; Function Attrs: nounwind
define float @foo(float %x) #0 {
entry:
  %x.addr = alloca float, align 4
  store float %x, float* %x.addr, align 4
  call void @llvm.dbg.declare(metadata float* %x.addr, metadata !16, metadata !17), !dbg !18
  %0 = load float* %x.addr, align 4, !dbg !19
  ret float %0, !dbg !19
}

; Function Attrs: nounwind readnone
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; CHECK: define void @bar

; Function Attrs: nounwind
define void @bar(float* %dst) #0 {
entry:

; CHECK: [[x_addr_i:%[a-zA-Z0-9.]+]] = alloca float, align 4

  %dst.addr = alloca float*, align 4
  store float* %dst, float** %dst.addr, align 4
  call void @llvm.dbg.declare(metadata float** %dst.addr, metadata !20, metadata !17), !dbg !21
  %0 = load float** %dst.addr, align 4, !dbg !22
  %arrayidx = getelementptr inbounds float* %0, i32 0, !dbg !22
  %1 = load float* %arrayidx, align 4, !dbg !22
  %call = call float @foo(float %1), !dbg !22

; CHECK-NOT: call float @foo
; CHECK: void @llvm.dbg.declare(metadata float* [[x_addr_i]], metadata [[m23:![0-9]+]], metadata !17), !dbg [[m24:![0-9]+]]

  %2 = load float** %dst.addr, align 4, !dbg !22
  %arrayidx1 = getelementptr inbounds float* %2, i32 0, !dbg !22
  store float %call, float* %arrayidx1, align 4, !dbg !22
  ret void, !dbg !23
}

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!13, !14}
!llvm.ident = !{!15}

!0 = !{!"0x11\0012\00clang version 3.6.0 (trunk)\000\00\000\00\001", !1, !2, !2, !3, !2, !2} ; [ DW_TAG_compile_unit ] [foo.c] [DW_LANG_C99]
!1 = !{!"foo.c", !""}
!2 = !{}
!3 = !{!4, !9}
!4 = !{!"0x2e\00foo\00foo\00\001\000\001\000\000\00256\000\002", !1, !5, !6, null, float (float)* @foo, null, null, !2} ; [ DW_TAG_subprogram ] [line 1] [def] [scope 2] [foo]
!5 = !{!"0x29", !1}    ; [ DW_TAG_file_type ] [foo.c]
!6 = !{!"0x15\00\000\000\000\000\000\000", null, null, null, !7, null, null, null} ; [ DW_TAG_subroutine_type ] [line 0, size 0, align 0, offset 0] [from ]
!7 = !{!8, !8}
!8 = !{!"0x24\00float\000\0032\0032\000\000\004", null, null} ; [ DW_TAG_base_type ] [float] [line 0, size 32, align 32, offset 0, enc DW_ATE_float]
!9 = !{!"0x2e\00bar\00bar\00\006\000\001\000\000\00256\000\007", !1, !5, !10, null, void (float*)* @bar, null, null, !2} ; [ DW_TAG_subprogram ] [line 6] [def] [scope 7] [bar]
!10 = !{!"0x15\00\000\000\000\000\000\000", null, null, null, !11, null, null, null} ; [ DW_TAG_subroutine_type ] [line 0, size 0, align 0, offset 0] [from ]
!11 = !{null, !12}
!12 = !{!"0xf\00\000\0032\0032\000\000", null, null, !8} ; [ DW_TAG_pointer_type ] [line 0, size 32, align 32, offset 0] [from float]
!13 = !{i32 2, !"Dwarf Version", i32 4}
!14 = !{i32 2, !"Debug Info Version", i32 2}
!15 = !{!"clang version 3.6.0 (trunk)"}
!16 = !{!"0x101\00x\0016777217\000", !4, !5, !8} ; [ DW_TAG_arg_variable ] [x] [line 1]
!17 = !{!"0x102"}               ; [ DW_TAG_expression ]
!18 = !{i32 1, i32 17, !4, null}
!19 = !{i32 3, i32 5, !4, null}
!20 = !{!"0x101\00dst\0016777222\000", !9, !5, !12} ; [ DW_TAG_arg_variable ] [dst] [line 6]
!21 = !{i32 6, i32 17, !9, null}
!22 = !{i32 8, i32 14, !9, null}
!23 = !{i32 9, i32 1, !9, null}

; CHECK: [[m23]] = !{!"0x101\00x\0016777217\000", !4, !5, !8, !22} ; [ DW_TAG_arg_variable ] [x] [line 1]
; CHECK: [[m24]] = !{i32 1, i32 17, !4, !22}
