/*
 * Copyright(c) 2019 Netflix, Inc.
 * SPDX - License - Identifier: BSD - 2 - Clause - Patent
 */

/******************************************************************************
 * @file Convolve2dTest.cc
 *
 * @brief Unit test for interpolation in inter prediction:
 * - av1_highbd_convolve_2d_copy_sr_avx2
 * - av1_highbd_jnt_convolve_2d_copy_avx2
 * - av1_highbd_convolve_x_sr_avx2
 * - av1_highbd_convolve_y_sr_avx2
 * - av1_highbd_convolve_2d_sr_avx2
 * - av1_highbd_jnt_convolve_x_avx2
 * - av1_highbd_jnt_convolve_y_avx2
 * - av1_highbd_jnt_convolve_2d_avx2
 * - av1_convolve_2d_copy_sr_avx2
 * - av1_jnt_convolve_2d_copy_avx2
 * - av1_convolve_x_sr_avx2
 * - av1_convolve_y_sr_avx2
 * - av1_convolve_2d_sr_avx2
 * - av1_jnt_convolve_x_avx2
 * - av1_jnt_convolve_y_avx2
 * - av1_jnt_convolve_2d_avx2
 *
 * @author Cidana-Wenyao
 *
 ******************************************************************************/
#include "stdlib.h"
#include "gtest/gtest.h"
#include "EbDefinitions.h"
#include "random.h"
#include "util.h"
#include "convolve.h"
#include "convolve_2d_funcs.h"

const int kMaxSize = 128 + 32;  // padding
using svt_av1_test_tool::SVTRandom;
namespace {
using highbd_convolve_2d_func =
    void (*)(const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride,
             int w, int h, const InterpFilterParams *filter_params_x,
             const InterpFilterParams *filter_params_y, const int subpel_x_qn,
             const int subpel_y_qn, ConvolveParams *conv_params, int bd);

using lowbd_convolve_2d_func =
    void (*)(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride,
             int w, int h, InterpFilterParams *filter_params_x,
             InterpFilterParams *filter_params_y, const int subpel_x_qn,
             const int subpel_y_qn, ConvolveParams *conv_params);

/**
 * @brief Unit test for interpolation in inter prediction:
 * - av1_{highbd, }_{jnt, }_convolve_{x, y, 2d}_{sr, copy}_avx2
 *
 * Test strategy:
 * Verify this assembly code by comparing with reference c implementation.
 * Feed the same data and check test output and reference output.
 * Define a template class to handle the common process, and
 * declare sub class to handle different bitdepth and function types.
 *
 * Expect result:
 * Output from assemble functions should be the same with output from c.
 *
 * Test coverage:
 * Test cases:
 * input value: Fill with random values
 * modes: jnt, sr, copy, x, y modes
 * TxSize: all the TxSize.
 * BitDepth: 8bit, 10bit, 12bit
 *
 */
typedef ::testing::tuple<int, int, int, BlockSize> Convolve2DParam;
template <typename Sample, typename FuncType>
class AV1Convolve2DTest : public ::testing::TestWithParam<Convolve2DParam> {
  public:
    AV1Convolve2DTest() : bd_(TEST_GET_PARAM(0)) {
        is_jnt_ = 0;
    }

    virtual ~AV1Convolve2DTest() {
    }

    // make the address algined to 32.
    void SetUp() override {
        conv_buf_ref_ = reinterpret_cast<ConvBufType *>(
            ((intptr_t)conv_ref_data_ + 31) & ~31);
        conv_buf_tst_ = reinterpret_cast<ConvBufType *>(
            ((intptr_t)conv_tst_data_ + 31) & ~31);
        output_ref_ =
            reinterpret_cast<Sample *>(((intptr_t)output_ref_data_ + 31) & ~31);
        output_tst_ =
            reinterpret_cast<Sample *>(((intptr_t)output_tst_data_ + 31) & ~31);
    }

    void TearDown() override {
        aom_clear_system_state();
    }

    virtual void run_convolve(int offset_r, int offset_c, int src_stride,
                              int dst_stride, int w, int h,
                              InterpFilterParams *filter_params_x,
                              InterpFilterParams *filter_params_y,
                              const int32_t subpel_x_q4,
                              const int32_t subpel_y_q4,
                              ConvolveParams *conv_params1,
                              ConvolveParams *conv_params2, int bd) {
        (void)offset_r;
        (void)offset_c;
        (void)src_stride;
        (void)dst_stride;
        (void)w;
        (void)h;
        (void)filter_params_x;
        (void)filter_params_y;
        (void)subpel_x_q4;
        (void)subpel_y_q4;
        (void)conv_params1;
        (void)conv_params2;
        (void)bd;
    }

    void test_convolve(int has_subx, int has_suby, int src_stride,
                       int dst_stride, int output_w, int output_h,
                       InterpFilterParams *filter_params_x,
                       InterpFilterParams *filter_params_y,
                       ConvolveParams *conv_params_ref,
                       ConvolveParams *conv_params_tst, int bd) {
        const int subx_range = has_subx ? 16 : 1;
        const int suby_range = has_suby ? 16 : 1;
        int subx, suby;
        for (subx = 0; subx < subx_range; ++subx) {
            for (suby = 0; suby < suby_range; ++suby) {
                const int offset_r = 3;
                const int offset_c = 3;

                run_convolve(offset_r,
                             offset_c,
                             src_stride,
                             dst_stride,
                             output_w,
                             output_h,
                             filter_params_x,
                             filter_params_y,
                             subx,
                             suby,
                             conv_params_ref,
                             conv_params_tst,
                             bd);

                if (memcmp(output_ref_,
                           output_tst_,
                           MAX_SB_SQUARE * sizeof(output_ref_[0]))) {
                    for (int i = 0; i < output_h; ++i) {
                        for (int j = 0; j < output_w; ++j) {
                            int idx = i * MAX_SB_SIZE + j;
                            ASSERT_EQ(output_ref_[idx], output_tst_[idx])
                                << output_w << "x" << output_h
                                << " Pixel mismatch at "
                                   "index "
                                << idx << " = (" << j << ", " << i
                                << "), sub pixel offset = (" << suby << ", "
                                << subx << ")";
                        }
                    }
                }

                if (is_jnt_) {
                    for (int i = 0; i < output_h; ++i) {
                        for (int j = 0; j < output_w; ++j) {
                            int idx = i * MAX_SB_SIZE + j;
                            ASSERT_EQ(conv_buf_ref_[idx], conv_buf_tst_[idx])
                                << output_w << "x" << output_h
                                << " Pixel mismatch at index " << idx << " = ("
                                << j << ", " << i << "), sub pixel offset = ("
                                << suby << ", " << subx << ")";
                        }
                    }
                }
            }
        }
    }

  protected:
    void prepare_data(int w, int h) {
        SVTRandom rnd_(bd_, false);  // bd_-bits, unsigned

        for (int i = 0; i < h; ++i)
            for (int j = 0; j < w; ++j)
                input[i * w + j] = rnd_.random();

        for (int i = 0; i < MAX_SB_SQUARE; ++i) {
            conv_buf_ref_[i] = conv_buf_tst_[i] = 0;
            output_ref_[i] = output_tst_[i] = 0;
        }
    }

    void run_test() {
        const int input_w = kMaxSize, input_h = kMaxSize;
        const int bd = TEST_GET_PARAM(0);
        const int has_subx = TEST_GET_PARAM(1);
        const int has_suby = TEST_GET_PARAM(2);
        const int block_idx = TEST_GET_PARAM(3);
        int hfilter, vfilter;

        // fill the input data with random
        prepare_data(input_w, input_h);

        // loop the filter type and subpixel position
        const int output_w = block_size_wide[block_idx];
        const int output_h = block_size_high[block_idx];
        const InterpFilter max_hfilter =
            has_subx ? INTERP_FILTERS_ALL : EIGHTTAP_SMOOTH;
        const InterpFilter max_vfilter =
            has_suby ? INTERP_FILTERS_ALL : EIGHTTAP_SMOOTH;
        for (hfilter = EIGHTTAP_REGULAR; hfilter < max_hfilter; ++hfilter) {
            for (vfilter = EIGHTTAP_REGULAR; vfilter < max_vfilter; ++vfilter) {
                InterpFilterParams filter_params_x =
                    av1_get_interp_filter_params_with_block_size(
                        (InterpFilter)hfilter, output_w);
                InterpFilterParams filter_params_y =
                    av1_get_interp_filter_params_with_block_size(
                        (InterpFilter)vfilter, output_h);
                for (int do_average = 0; do_average < 1 + is_jnt_;
                     ++do_average) {
                    // setup convolveParams according to jnt or sr
                    ConvolveParams conv_params_ref;
                    ConvolveParams conv_params_tst;
                    if (is_jnt_) {
                        conv_params_ref =
                            get_conv_params_no_round(0,
                                                     do_average,
                                                     0,
                                                     conv_buf_ref_,
                                                     MAX_SB_SIZE,
                                                     1,
                                                     bd);
                        conv_params_tst =
                            get_conv_params_no_round(0,
                                                     do_average,
                                                     0,
                                                     conv_buf_tst_,
                                                     MAX_SB_SIZE,
                                                     1,
                                                     bd);
                        // Test special case where dist_wtd_comp_avg is not
                        // used
                        conv_params_ref.use_jnt_comp_avg = 0;
                        conv_params_tst.use_jnt_comp_avg = 0;

                    } else {
                        conv_params_ref = get_conv_params_no_round(
                            0, do_average, 0, nullptr, 0, 0, bd);
                        conv_params_tst = get_conv_params_no_round(
                            0, do_average, 0, nullptr, 0, 0, bd);
                    }

                    test_convolve(has_subx,
                                  has_suby,
                                  input_w,
                                  MAX_SB_SIZE,
                                  output_w,
                                  output_h,
                                  &filter_params_x,
                                  &filter_params_y,
                                  &conv_params_ref,
                                  &conv_params_tst,
                                  bd);

                    if (is_jnt_ == 0)
                        continue;

                    const int quant_dist_lookup_table[2][4][2] = {
                        {{9, 7}, {11, 5}, {12, 4}, {13, 3}},
                        {{7, 9}, {5, 11}, {4, 12}, {3, 13}},
                    };
                    // Test different combination of fwd and bck offset
                    // weights
                    for (int k = 0; k < 2; ++k) {
                        for (int l = 0; l < 4; ++l) {
                            conv_params_ref.use_jnt_comp_avg = 1;
                            conv_params_tst.use_jnt_comp_avg = 1;
                            conv_params_ref.fwd_offset =
                                quant_dist_lookup_table[k][l][0];
                            conv_params_ref.bck_offset =
                                quant_dist_lookup_table[k][l][1];
                            conv_params_tst.fwd_offset =
                                quant_dist_lookup_table[k][l][0];
                            conv_params_tst.bck_offset =
                                quant_dist_lookup_table[k][l][1];

                            test_convolve(has_subx,
                                          has_suby,
                                          input_w,
                                          MAX_SB_SIZE,
                                          output_w,
                                          output_h,
                                          &filter_params_x,
                                          &filter_params_y,
                                          &conv_params_ref,
                                          &conv_params_tst,
                                          bd);
                        }
                    }
                }
            }
        }
    }

  protected:
    FuncType func_ref_;
    FuncType func_tst_;
    int bd_;
    int is_jnt_;  // jnt or single reference
    Sample input[kMaxSize * kMaxSize];
    ConvBufType *conv_buf_ref_;  // aligned address
    ConvBufType *conv_buf_tst_;  // aligned address
    Sample *output_ref_;         // aligned address
    Sample *output_tst_;         // aligned address
    DECLARE_ALIGNED(32, ConvBufType, conv_ref_data_[MAX_SB_SQUARE + 32]);
    DECLARE_ALIGNED(32, ConvBufType, conv_tst_data_[MAX_SB_SQUARE + 32]);
    DECLARE_ALIGNED(32, Sample, output_ref_data_[MAX_SB_SQUARE + 32]);
    DECLARE_ALIGNED(32, Sample, output_tst_data_[MAX_SB_SQUARE + 32]);
};

::testing::internal::ParamGenerator<Convolve2DParam> BuildParams(int has_subx,
                                                                 int has_suby,
                                                                 int highbd) {
    if (highbd)
        return ::testing::Combine(::testing::Range(8, 13, 2),
                                  ::testing::Values(has_subx),
                                  ::testing::Values(has_suby),
                                  ::testing::Range(BLOCK_4X4, BlockSizeS_ALL));
    else
        return ::testing::Combine(::testing::Values(8),
                                  ::testing::Values(has_subx),
                                  ::testing::Values(has_suby),
                                  ::testing::Range(BLOCK_4X4, BlockSizeS_ALL));
}

class AV1LbdConvolve2DTest
    : public AV1Convolve2DTest<uint8_t, lowbd_convolve_2d_func> {
  public:
    AV1LbdConvolve2DTest() {
    }
    virtual ~AV1LbdConvolve2DTest() {
    }
    void run_convolve(int offset_r, int offset_c, int src_stride,
                      int dst_stride, int output_w, int output_h,
                      InterpFilterParams *filter_params_x,
                      InterpFilterParams *filter_params_y,
                      const int32_t subpel_x_q4, const int32_t subpel_y_q4,
                      ConvolveParams *conv_params_ref,
                      ConvolveParams *conv_params_tst, int bd) override {
        (void)bd;
        func_ref_(input + offset_r * src_stride + offset_c,
                  src_stride,
                  output_ref_,
                  dst_stride,
                  output_w,
                  output_h,
                  filter_params_x,
                  filter_params_y,
                  subpel_x_q4,
                  subpel_y_q4,
                  conv_params_ref);
        func_tst_(input + offset_r * src_stride + offset_c,
                  src_stride,
                  output_tst_,
                  dst_stride,
                  output_w,
                  output_h,
                  filter_params_x,
                  filter_params_y,
                  subpel_x_q4,
                  subpel_y_q4,
                  conv_params_tst);
    }
};

class AV1LbdJntConvolve2DTest : public AV1LbdConvolve2DTest {
  public:
    AV1LbdJntConvolve2DTest() {
        is_jnt_ = 1;
        func_ref_ = av1_jnt_convolve_2d_c;
        const int has_subx = TEST_GET_PARAM(1);
        const int has_suby = TEST_GET_PARAM(2);
        if (has_subx == 1 && has_suby == 1)
            func_tst_ = av1_jnt_convolve_2d_avx2;
        else if (has_subx == 1)
            func_tst_ = av1_jnt_convolve_x_avx2;
        else if (has_suby == 1)
            func_tst_ = av1_jnt_convolve_y_avx2;
        else
            func_tst_ = av1_jnt_convolve_2d_copy_avx2;
        bd_ = TEST_GET_PARAM(0);
    }
    virtual ~AV1LbdJntConvolve2DTest() {
    }
};

TEST_P(AV1LbdJntConvolve2DTest, MatchTest) {
    run_test();
}
INSTANTIATE_TEST_CASE_P(AVX2_COPY, AV1LbdJntConvolve2DTest,
                        BuildParams(0, 0, 0));
INSTANTIATE_TEST_CASE_P(ConvolveTest2D, AV1LbdJntConvolve2DTest,
                        BuildParams(1, 1, 0));
INSTANTIATE_TEST_CASE_P(ConvolveTestX, AV1LbdJntConvolve2DTest,
                        BuildParams(1, 0, 0));
INSTANTIATE_TEST_CASE_P(ConvolveTestY, AV1LbdJntConvolve2DTest,
                        BuildParams(0, 1, 0));

class AV1LbdSrConvolve2DTest : public AV1LbdConvolve2DTest {
  public:
    AV1LbdSrConvolve2DTest() {
        is_jnt_ = 0;
        func_ref_ = av1_convolve_2d_sr_c;
        const int has_subx = TEST_GET_PARAM(1);
        const int has_suby = TEST_GET_PARAM(2);
        if (has_subx == 1 && has_suby == 1)
            func_tst_ = av1_convolve_2d_sr_avx2;
        else if (has_subx == 1)
            func_tst_ = av1_convolve_x_sr_avx2;
        else if (has_suby == 1)
            func_tst_ = av1_convolve_y_sr_avx2;
        else
            func_tst_ = av1_convolve_2d_copy_sr_avx2;
        bd_ = TEST_GET_PARAM(0);
    }
    virtual ~AV1LbdSrConvolve2DTest() {
    }
};

TEST_P(AV1LbdSrConvolve2DTest, MatchTest) {
    run_test();
}
INSTANTIATE_TEST_CASE_P(ConvolveTestX, AV1LbdSrConvolve2DTest,
                        BuildParams(1, 0, 0));
INSTANTIATE_TEST_CASE_P(ConvolveTest2D, AV1LbdSrConvolve2DTest,
                        BuildParams(1, 1, 0));
INSTANTIATE_TEST_CASE_P(ConvolveTestY, AV1LbdSrConvolve2DTest,
                        BuildParams(0, 1, 0));
INSTANTIATE_TEST_CASE_P(ConvolveTestCopy, AV1LbdSrConvolve2DTest,
                        BuildParams(0, 0, 0));

class AV1HbdConvolve2DTest
    : public AV1Convolve2DTest<uint16_t, highbd_convolve_2d_func> {
  public:
    AV1HbdConvolve2DTest() {
    }
    virtual ~AV1HbdConvolve2DTest() {
    }
    void run_convolve(int offset_r, int offset_c, int src_stride,
                      int dst_stride, int blk_w, int blk_h,
                      InterpFilterParams *filter_params_x,
                      InterpFilterParams *filter_params_y,
                      const int32_t subpel_x_q4, const int32_t subpel_y_q4,
                      ConvolveParams *conv_params_ref,
                      ConvolveParams *conv_params_tst, int bd) override {
        func_ref_(input + offset_r * src_stride + offset_c,
                  src_stride,
                  output_ref_,
                  dst_stride,
                  blk_w,
                  blk_h,
                  filter_params_x,
                  filter_params_y,
                  subpel_x_q4,
                  subpel_y_q4,
                  conv_params_ref,
                  bd);
        func_tst_(input + offset_r * src_stride + offset_c,
                  src_stride,
                  output_tst_,
                  dst_stride,
                  blk_w,
                  blk_h,
                  filter_params_x,
                  filter_params_y,
                  subpel_x_q4,
                  subpel_y_q4,
                  conv_params_tst,
                  bd);
    }
};

class AV1HbdJntConvolve2DTest : public AV1HbdConvolve2DTest {
  public:
    AV1HbdJntConvolve2DTest() {
        is_jnt_ = 1;
        func_ref_ = av1_highbd_jnt_convolve_2d_c;
        const int has_subx = TEST_GET_PARAM(1);
        const int has_suby = TEST_GET_PARAM(2);
        if (has_subx == 1 && has_suby == 1)
            func_tst_ = av1_highbd_jnt_convolve_2d_avx2;
        else if (has_subx == 1)
            func_tst_ = av1_highbd_jnt_convolve_x_avx2;
        else if (has_suby == 1)
            func_tst_ = av1_highbd_jnt_convolve_y_avx2;
        else
            func_tst_ = av1_highbd_jnt_convolve_2d_copy_avx2;

        bd_ = TEST_GET_PARAM(0);
    }
    virtual ~AV1HbdJntConvolve2DTest() {
    }
};

TEST_P(AV1HbdJntConvolve2DTest, MatchTest) {
    run_test();
}
INSTANTIATE_TEST_CASE_P(AVX2_COPY, AV1HbdJntConvolve2DTest,
                        BuildParams(0, 0, 1));
INSTANTIATE_TEST_CASE_P(ConvolveTest2D, AV1HbdJntConvolve2DTest,
                        BuildParams(1, 1, 1));
INSTANTIATE_TEST_CASE_P(ConvolveTestX, AV1HbdJntConvolve2DTest,
                        BuildParams(1, 0, 1));
INSTANTIATE_TEST_CASE_P(ConvolveTestY, AV1HbdJntConvolve2DTest,
                        BuildParams(0, 1, 1));

class AV1HbdSrConvolve2DTest : public AV1HbdConvolve2DTest {
  public:
    AV1HbdSrConvolve2DTest() {
        is_jnt_ = 0;
        func_ref_ = av1_highbd_convolve_2d_sr_c;
        const int has_subx = TEST_GET_PARAM(1);
        const int has_suby = TEST_GET_PARAM(2);
        if (has_subx == 1 && has_suby == 1)
            func_tst_ = av1_highbd_convolve_2d_sr_avx2;
        else if (has_subx == 1)
            func_tst_ = av1_highbd_convolve_x_sr_avx2;
        else if (has_suby == 1)
            func_tst_ = av1_highbd_convolve_y_sr_avx2;
        else
            func_tst_ = av1_highbd_convolve_2d_copy_sr_avx2;
        bd_ = TEST_GET_PARAM(0);
    }
    virtual ~AV1HbdSrConvolve2DTest() {
    }
};

TEST_P(AV1HbdSrConvolve2DTest, MatchTest) {
    run_test();
}
INSTANTIATE_TEST_CASE_P(ConvolveTestX, AV1HbdSrConvolve2DTest,
                        BuildParams(1, 0, 1));
INSTANTIATE_TEST_CASE_P(ConvolveTest2D, AV1HbdSrConvolve2DTest,
                        BuildParams(1, 1, 1));
INSTANTIATE_TEST_CASE_P(ConvolveTestY, AV1HbdSrConvolve2DTest,
                        BuildParams(0, 1, 1));
INSTANTIATE_TEST_CASE_P(ConvolveTestCopy, AV1HbdSrConvolve2DTest,
                        BuildParams(0, 0, 1));
}  // namespace
