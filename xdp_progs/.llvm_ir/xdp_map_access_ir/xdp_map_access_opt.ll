; ModuleID = 'bpf-jit'
source_filename = "bpf-jit"

declare i64 @_bpf_helper_ext_0001(i64, i64, i64, i64, i64) local_unnamed_addr

define i64 @bpf_main(ptr %0, i64 %1) local_unnamed_addr {
setupBlock:
  %stackBegin1 = alloca [2058 x i64], align 8
  %stackEnd = getelementptr inbounds i64, ptr %stackBegin1, i64 2048
  %2 = ptrtoint ptr %stackEnd to i64
  %3 = load i64, ptr %0, align 4
  %4 = getelementptr i8, ptr %0, i64 8
  %5 = load i64, ptr %4, align 4
  %6 = getelementptr inbounds i8, ptr %stackBegin1, i64 16380
  store i32 0, ptr %6, align 4
  %7 = getelementptr inbounds i64, ptr %stackBegin1, i64 2047
  store i32 0, ptr %7, align 8
  %8 = add i64 %2, -4
  %9 = call i64 @_bpf_helper_ext_0001(i64 0, i64 %8, i64 undef, i64 undef, i64 undef)
  %10 = icmp eq i64 %9, 0
  br i1 %10, label %exitBlock, label %bb_inst_13

bb_inst_13:                                       ; preds = %setupBlock
  %11 = inttoptr i64 %9 to ptr
  %12 = load i32, ptr %11, align 4
  %.not = icmp eq i32 %12, 0
  br i1 %.not, label %bb_inst_15, label %exitBlock

bb_inst_15:                                       ; preds = %bb_inst_13
  %13 = add i64 %2, -8
  %14 = call i64 @_bpf_helper_ext_0001(i64 0, i64 %13, i64 undef, i64 undef, i64 undef)
  %15 = icmp eq i64 %14, 0
  br i1 %15, label %bb_inst_24, label %bb_inst_21

bb_inst_21:                                       ; preds = %bb_inst_15
  %16 = inttoptr i64 %14 to ptr
  %17 = load i64, ptr %16, align 4
  %18 = add i64 %17, 1
  store i64 %18, ptr %16, align 4
  br label %bb_inst_24

bb_inst_24:                                       ; preds = %bb_inst_21, %bb_inst_15
  %19 = add i64 %3, 14
  %20 = icmp ugt i64 %19, %5
  br i1 %20, label %exitBlock, label %bb_inst_28

bb_inst_28:                                       ; preds = %bb_inst_24
  %21 = inttoptr i64 %3 to ptr
  %22 = load i16, ptr %21, align 2
  %23 = getelementptr i8, ptr %21, i64 6
  %24 = load i16, ptr %23, align 2
  store i16 %24, ptr %21, align 2
  %25 = getelementptr i8, ptr %21, i64 8
  %26 = load i16, ptr %25, align 2
  %27 = getelementptr i8, ptr %21, i64 2
  %28 = load i16, ptr %27, align 2
  store i16 %28, ptr %25, align 2
  store i16 %26, ptr %27, align 2
  %29 = getelementptr i8, ptr %21, i64 10
  %30 = load i16, ptr %29, align 2
  %31 = getelementptr i8, ptr %21, i64 4
  %32 = load i16, ptr %31, align 2
  store i16 %32, ptr %29, align 2
  store i16 %22, ptr %23, align 2
  store i16 %30, ptr %31, align 2
  br label %exitBlock

exitBlock:                                        ; preds = %setupBlock, %bb_inst_13, %bb_inst_24, %bb_inst_28
  %r0.0 = phi i64 [ 2, %setupBlock ], [ 1, %bb_inst_24 ], [ 3, %bb_inst_28 ], [ 2, %bb_inst_13 ]
  ret i64 %r0.0
}
