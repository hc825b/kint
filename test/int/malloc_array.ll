; ModuleID = '<stdin>'
source_filename = "malloc_array.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nounwind uwtable
define i8* @malloc_array_nc(i64 %n, i64 %size) #0 !dbg !8 {
entry:
  %mul = mul i64 %n, %size, !dbg !14
  %call = call noalias i8* @malloc(i64 %mul) #2, !dbg !15
  ret i8* %call, !dbg !16
}

; Function Attrs: nounwind
declare noalias i8* @malloc(i64) #1

; Function Attrs: nounwind uwtable
define i8* @malloc_array_0(i64 %n, i64 %size) #0 !dbg !17 {
entry:
  %tobool = icmp ne i64 %size, 0, !dbg !18
  br i1 %tobool, label %land.lhs.true, label %if.end, !dbg !20

land.lhs.true:                                    ; preds = %entry
  %div = udiv i64 -1, %size, !dbg !21
  %cmp = icmp ugt i64 %n, %div, !dbg !23
  br i1 %cmp, label %if.then, label %if.end, !dbg !24

if.then:                                          ; preds = %land.lhs.true
  br label %return, !dbg !25

if.end:                                           ; preds = %land.lhs.true, %entry
  %mul = mul i64 %n, %size, !dbg !26
  %call = call noalias i8* @malloc(i64 %mul) #2, !dbg !27
  br label %return, !dbg !28

return:                                           ; preds = %if.end, %if.then
  %retval.0 = phi i8* [ null, %if.then ], [ %call, %if.end ]
  ret i8* %retval.0, !dbg !29
}

; Function Attrs: nounwind uwtable
define i8* @malloc_array_1(i64 %n, i64 %size) #0 !dbg !30 {
entry:
  %tobool = icmp ne i64 %n, 0, !dbg !31
  br i1 %tobool, label %land.lhs.true, label %if.end, !dbg !33

land.lhs.true:                                    ; preds = %entry
  %div = udiv i64 -1, %n, !dbg !34
  %cmp = icmp ugt i64 %size, %div, !dbg !36
  br i1 %cmp, label %if.then, label %if.end, !dbg !37

if.then:                                          ; preds = %land.lhs.true
  br label %return, !dbg !38

if.end:                                           ; preds = %land.lhs.true, %entry
  %mul = mul i64 %n, %size, !dbg !39
  %call = call noalias i8* @malloc(i64 %mul) #2, !dbg !40
  br label %return, !dbg !41

return:                                           ; preds = %if.end, %if.then
  %retval.0 = phi i8* [ null, %if.then ], [ %call, %if.end ]
  ret i8* %retval.0, !dbg !42
}

; Function Attrs: nounwind uwtable
define i8* @malloc_array_2(i64 %n, i64 %size) #0 !dbg !43 {
entry:
  %mul = mul i64 %n, %size, !dbg !44
  %tobool = icmp ne i64 %size, 0, !dbg !45
  br i1 %tobool, label %land.lhs.true, label %if.end, !dbg !47

land.lhs.true:                                    ; preds = %entry
  %div = udiv i64 %mul, %size, !dbg !48
  %cmp = icmp ne i64 %n, %div, !dbg !50
  br i1 %cmp, label %if.then, label %if.end, !dbg !51

if.then:                                          ; preds = %land.lhs.true
  br label %return, !dbg !52

if.end:                                           ; preds = %land.lhs.true, %entry
  %call = call noalias i8* @malloc(i64 %mul) #2, !dbg !53
  br label %return, !dbg !54

return:                                           ; preds = %if.end, %if.then
  %retval.0 = phi i8* [ null, %if.then ], [ %call, %if.end ]
  ret i8* %retval.0, !dbg !55
}

attributes #0 = { nounwind uwtable "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!5, !6}
!llvm.ident = !{!7}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 3.9.1 (tags/RELEASE_391/final)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, retainedTypes: !3)
!1 = !DIFile(filename: "malloc_array.c", directory: "/home/chsieh16/Documents/CS526/kint/test/int")
!2 = !{}
!3 = !{!4}
!4 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64, align: 64)
!5 = !{i32 2, !"Dwarf Version", i32 4}
!6 = !{i32 2, !"Debug Info Version", i32 3}
!7 = !{!"clang version 3.9.1 (tags/RELEASE_391/final)"}
!8 = distinct !DISubprogram(name: "malloc_array_nc", scope: !1, file: !1, line: 7, type: !9, isLocal: false, isDefinition: true, scopeLine: 8, flags: DIFlagPrototyped, isOptimized: false, unit: !0, variables: !2)
!9 = !DISubroutineType(types: !10)
!10 = !{!4, !11, !11}
!11 = !DIDerivedType(tag: DW_TAG_typedef, name: "size_t", file: !12, line: 62, baseType: !13)
!12 = !DIFile(filename: "/class/cs526/sp17-projects/llvm/llvm-3.9.1/bin/../lib/clang/3.9.1/include/stddef.h", directory: "/home/chsieh16/Documents/CS526/kint/test/int")
!13 = !DIBasicType(name: "long unsigned int", size: 64, align: 64, encoding: DW_ATE_unsigned)
!14 = !DILocation(line: 9, column: 18, scope: !8)
!15 = !DILocation(line: 9, column: 9, scope: !8)
!16 = !DILocation(line: 9, column: 2, scope: !8)
!17 = distinct !DISubprogram(name: "malloc_array_0", scope: !1, file: !1, line: 12, type: !9, isLocal: false, isDefinition: true, scopeLine: 13, flags: DIFlagPrototyped, isOptimized: false, unit: !0, variables: !2)
!18 = !DILocation(line: 15, column: 6, scope: !19)
!19 = distinct !DILexicalBlock(scope: !17, file: !1, line: 15, column: 6)
!20 = !DILocation(line: 15, column: 11, scope: !19)
!21 = !DILocation(line: 15, column: 27, scope: !22)
!22 = !DILexicalBlockFile(scope: !19, file: !1, discriminator: 1)
!23 = !DILocation(line: 15, column: 16, scope: !22)
!24 = !DILocation(line: 15, column: 6, scope: !22)
!25 = !DILocation(line: 16, column: 3, scope: !19)
!26 = !DILocation(line: 17, column: 18, scope: !17)
!27 = !DILocation(line: 17, column: 9, scope: !17)
!28 = !DILocation(line: 17, column: 2, scope: !17)
!29 = !DILocation(line: 18, column: 1, scope: !17)
!30 = distinct !DISubprogram(name: "malloc_array_1", scope: !1, file: !1, line: 20, type: !9, isLocal: false, isDefinition: true, scopeLine: 21, flags: DIFlagPrototyped, isOptimized: false, unit: !0, variables: !2)
!31 = !DILocation(line: 24, column: 6, scope: !32)
!32 = distinct !DILexicalBlock(scope: !30, file: !1, line: 24, column: 6)
!33 = !DILocation(line: 24, column: 8, scope: !32)
!34 = !DILocation(line: 24, column: 27, scope: !35)
!35 = !DILexicalBlockFile(scope: !32, file: !1, discriminator: 1)
!36 = !DILocation(line: 24, column: 16, scope: !35)
!37 = !DILocation(line: 24, column: 6, scope: !35)
!38 = !DILocation(line: 25, column: 3, scope: !32)
!39 = !DILocation(line: 26, column: 18, scope: !30)
!40 = !DILocation(line: 26, column: 9, scope: !30)
!41 = !DILocation(line: 26, column: 2, scope: !30)
!42 = !DILocation(line: 27, column: 1, scope: !30)
!43 = distinct !DISubprogram(name: "malloc_array_2", scope: !1, file: !1, line: 29, type: !9, isLocal: false, isDefinition: true, scopeLine: 30, flags: DIFlagPrototyped, isOptimized: false, unit: !0, variables: !2)
!44 = !DILocation(line: 33, column: 19, scope: !43)
!45 = !DILocation(line: 34, column: 6, scope: !46)
!46 = distinct !DILexicalBlock(scope: !43, file: !1, line: 34, column: 6)
!47 = !DILocation(line: 34, column: 11, scope: !46)
!48 = !DILocation(line: 34, column: 25, scope: !49)
!49 = !DILexicalBlockFile(scope: !46, file: !1, discriminator: 1)
!50 = !DILocation(line: 34, column: 16, scope: !49)
!51 = !DILocation(line: 34, column: 6, scope: !49)
!52 = !DILocation(line: 35, column: 3, scope: !46)
!53 = !DILocation(line: 36, column: 9, scope: !43)
!54 = !DILocation(line: 36, column: 2, scope: !43)
!55 = !DILocation(line: 37, column: 1, scope: !43)
