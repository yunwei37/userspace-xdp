; ModuleID = '.llvm_ir/xdp_adjust_tail.bpf.ll'
source_filename = "bpf-jit"

declare i64 @_bpf_helper_ext_0001(i64, i64, i64, i64, i64) local_unnamed_addr

declare i64 @_bpf_helper_ext_0006(i64, i64, i64, i64, i64) local_unnamed_addr

declare i64 @_bpf_helper_ext_0028(i64, i64, i64, i64, i64) local_unnamed_addr

declare i64 @_bpf_helper_ext_0044(i64, i64, i64, i64, i64) local_unnamed_addr

declare i64 @_bpf_helper_ext_0065(i64, i64, i64, i64, i64) local_unnamed_addr

define i64 @bpf_main(ptr %0, i64 %1) local_unnamed_addr {
setupBlock:
  %stackBegin170 = alloca [2058 x i64], align 8
  %stackEnd = getelementptr inbounds [2058 x i64], ptr %stackBegin170, i64 0, i64 2048
  %2 = ptrtoint ptr %stackEnd to i64
  %3 = ptrtoint ptr %0 to i64
  %4 = getelementptr i8, ptr %0, i64 8
  %5 = load i64, ptr %4, align 4
  %6 = load i64, ptr %0, align 4
  %7 = add i64 %6, 1
  %.not = icmp ult i64 %5, %7
  br i1 %.not, label %bb_inst_9, label %bb_inst_13

bb_inst_9:                                        ; preds = %bb_inst_34, %setupBlock
  %r3.0 = phi i64 [ %7, %setupBlock ], [ %13, %bb_inst_34 ]
  %r2.0 = phi i64 [ 35, %setupBlock ], [ 24, %bb_inst_34 ]
  %r1.0 = phi i64 [ 0, %setupBlock ], [ 35, %bb_inst_34 ]
  %8 = call i64 @_bpf_helper_ext_0006(i64 %r1.0, i64 %r2.0, i64 %r3.0, i64 99, i64 undef)
  br label %bb_inst_11

bb_inst_11:                                       ; preds = %bb_inst_9, %bb_inst_52, %bb_inst_156, %bb_inst_64, %bb_inst_16, %bb_inst_13
  %r6.0 = phi i64 [ 2, %bb_inst_13 ], [ 2, %bb_inst_16 ], [ 3, %bb_inst_64 ], [ 3, %bb_inst_156 ], [ 1, %bb_inst_52 ], [ 1, %bb_inst_9 ]
  ret i64 %r6.0

bb_inst_13:                                       ; preds = %setupBlock
  %9 = inttoptr i64 %6 to ptr
  %10 = getelementptr i8, ptr %9, i64 12
  %11 = load i16, ptr %10, align 2
  %.not171 = icmp eq i16 %11, 8
  br i1 %.not171, label %bb_inst_16, label %bb_inst_11

bb_inst_16:                                       ; preds = %bb_inst_13
  %12 = sub i64 %5, %6
  %13 = and i64 %12, 4294967295
  %.not172 = icmp ugt i64 %13, 98
  br i1 %.not172, label %bb_inst_34, label %bb_inst_11

bb_inst_34:                                       ; preds = %bb_inst_16
  %14 = sub i64 98, %12
  %15 = call i64 @_bpf_helper_ext_0065(i64 %3, i64 %14, i64 %13, i64 99, i64 undef)
  %16 = call i64 @_bpf_helper_ext_0044(i64 %3, i64 4294967268, i64 %13, i64 99, i64 undef)
  %17 = icmp eq i64 %16, 0
  br i1 %17, label %bb_inst_47, label %bb_inst_9

bb_inst_47:                                       ; preds = %bb_inst_34
  %18 = load i64, ptr %4, align 4
  %19 = load i64, ptr %0, align 4
  %20 = add i64 %19, 126
  %.not173 = icmp ult i64 %18, %20
  br i1 %.not173, label %bb_inst_52, label %bb_inst_64

bb_inst_52:                                       ; preds = %bb_inst_47
  %21 = call i64 @_bpf_helper_ext_0006(i64 59, i64 62, i64 %13, i64 99, i64 undef)
  %22 = call i64 @_bpf_helper_ext_0006(i64 121, i64 57, i64 %19, i64 126, i64 %18)
  br label %bb_inst_11

bb_inst_64:                                       ; preds = %bb_inst_47
  %23 = add i64 %19, 6
  %24 = add i64 %19, 28
  %25 = inttoptr i64 %24 to ptr
  %26 = getelementptr i8, ptr %25, i64 4
  %27 = load i16, ptr %26, align 2
  %28 = inttoptr i64 %23 to ptr
  %29 = getelementptr i8, ptr %28, i64 4
  store i16 %27, ptr %29, align 2
  %30 = getelementptr i8, ptr %25, i64 2
  %31 = load i16, ptr %30, align 2
  %32 = getelementptr i8, ptr %28, i64 2
  store i16 %31, ptr %32, align 2
  %33 = load i16, ptr %25, align 2
  %34 = inttoptr i64 %19 to ptr
  %35 = getelementptr i8, ptr %34, i64 6
  store i16 %33, ptr %35, align 2
  %36 = add i64 %19, 34
  %37 = inttoptr i64 %36 to ptr
  %38 = getelementptr i8, ptr %37, i64 4
  %39 = load i16, ptr %38, align 2
  %40 = getelementptr i8, ptr %34, i64 4
  store i16 %39, ptr %40, align 2
  %41 = getelementptr i8, ptr %25, i64 6
  %42 = load i16, ptr %41, align 2
  store i16 %42, ptr %34, align 2
  %43 = getelementptr i8, ptr %37, i64 2
  %44 = load i16, ptr %43, align 2
  %45 = getelementptr i8, ptr %34, i64 2
  store i16 %44, ptr %45, align 2
  %46 = getelementptr i8, ptr %25, i64 12
  %47 = load i16, ptr %46, align 2
  %48 = getelementptr i8, ptr %34, i64 12
  store i16 %47, ptr %48, align 2
  store i8 3, ptr %37, align 1
  %49 = getelementptr i8, ptr %37, i64 1
  store i8 4, ptr %49, align 1
  %50 = getelementptr i8, ptr %37, i64 6
  store i16 -3329, ptr %50, align 2
  store i16 0, ptr %43, align 2
  %51 = call i64 @_bpf_helper_ext_0028(i64 0, i64 0, i64 %36, i64 92, i64 0)
  %52 = lshr i64 %51, 16
  %53 = add i64 %52, %51
  %54 = trunc i64 %53 to i16
  %55 = xor i16 %54, -1
  store i16 %55, ptr %43, align 2
  %56 = add i64 %19, 14
  %57 = inttoptr i64 %56 to ptr
  %58 = getelementptr i8, ptr %57, i64 8
  store i8 64, ptr %58, align 1
  %59 = add i64 %19, 42
  %60 = inttoptr i64 %59 to ptr
  %61 = getelementptr i8, ptr %60, i64 12
  %62 = load i32, ptr %61, align 4
  %63 = getelementptr i8, ptr %57, i64 16
  store i32 %62, ptr %63, align 4
  %64 = getelementptr i8, ptr %60, i64 16
  %65 = load i32, ptr %64, align 4
  %66 = getelementptr i8, ptr %57, i64 12
  store i32 %65, ptr %66, align 4
  store i8 69, ptr %57, align 1
  %67 = getelementptr i8, ptr %57, i64 9
  store i8 1, ptr %67, align 1
  %68 = getelementptr i8, ptr %57, i64 1
  store i8 0, ptr %68, align 1
  %69 = getelementptr i8, ptr %57, i64 2
  store i16 28672, ptr %69, align 2
  %70 = getelementptr i8, ptr %57, i64 10
  store i16 0, ptr %70, align 2
  %71 = call i64 @_bpf_helper_ext_0028(i64 0, i64 0, i64 %56, i64 20, i64 0)
  %72 = lshr i64 %71, 16
  %73 = add i64 %72, %71
  %74 = trunc i64 %73 to i16
  %75 = xor i16 %74, -1
  store i16 %75, ptr %70, align 2
  %76 = getelementptr inbounds i8, ptr %stackEnd, i64 -8
  store i64 0, ptr %76, align 8
  %77 = add i64 %2, -8
  %78 = call i64 @_bpf_helper_ext_0001(i64 0, i64 %77, i64 %56, i64 20, i64 0)
  %79 = icmp eq i64 %78, 0
  br i1 %79, label %bb_inst_11, label %bb_inst_156

bb_inst_156:                                      ; preds = %bb_inst_64
  %80 = inttoptr i64 %78 to ptr
  %81 = load i64, ptr %80, align 4
  %82 = add i64 %81, 1
  store i64 %82, ptr %80, align 4
  br label %bb_inst_11
}
