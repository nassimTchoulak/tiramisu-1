#include <isl/set.h>
#include <isl/union_map.h>
#include <isl/union_set.h>
#include <isl/ast_build.h>
#include <isl/schedule.h>
#include <isl/schedule_node.h>

#include <tiramisu/debug.h>
#include <tiramisu/core.h>

#include <string.h>
#include <Halide.h>
#include "halide_image_io.h"


using namespace tiramisu;

int main(int argc, char **argv)
{
    global::set_default_tiramisu_options();

    tiramisu::function blurxy_tiramisu("blurxy_tiramisu_test");

    Halide::Buffer<uint8_t> in_image = Halide::Tools::load_image("./images/rgb.png");
    int SIZE0 = in_image.extent(0);
    int SIZE1 = in_image.extent(1);
    int SIZE2 = in_image.extent(2);

    // Output buffers.
    int by_ext_2 = SIZE2;
    int by_ext_1 = SIZE1 - 8;
    int by_ext_0 = SIZE0 - 8;
    tiramisu::buffer buff_by("buff_by", 3, {tiramisu::expr(by_ext_2), tiramisu::expr(by_ext_1), tiramisu::expr(by_ext_0)}, tiramisu::p_uint8, NULL, tiramisu::a_output, &blurxy_tiramisu);

    // Input buffers.
    int p0_ext_2 = SIZE2;
    int p0_ext_1 = SIZE1;
    int p0_ext_0 = SIZE0;
    tiramisu::buffer buff_p0("buff_p0", 3, {tiramisu::expr(p0_ext_2), tiramisu::expr(p0_ext_1), tiramisu::expr(p0_ext_0)}, tiramisu::p_uint8, NULL, tiramisu::a_input, &blurxy_tiramisu);
    tiramisu::computation p0("[p0_ext_2, p0_ext_1, p0_ext_0]->{p0[i2, i1, i0]: (0 <= i2 <= (p0_ext_2 + -1)) and (0 <= i1 <= (p0_ext_1 + -1)) and (0 <= i0 <= (p0_ext_0 + -1))}", expr(), false, tiramisu::p_uint8, &blurxy_tiramisu);
    p0.set_access("{p0[i2, i1, i0]->buff_p0[i2, i1, i0]}");


    // Define temporary buffers for "blur_x".
    tiramisu::buffer buff_bx("buff_bx", 3, {tiramisu::expr(by_ext_2), tiramisu::expr(by_ext_1 + 2), tiramisu::expr(by_ext_0)}, tiramisu::p_uint8, NULL, tiramisu::a_temporary, &blurxy_tiramisu);

    // Define loop bounds for dimension "blur_x_s0_c".
    tiramisu::constant bx_s0_c_min("bx_s0_c_min", tiramisu::expr((int32_t)0), tiramisu::p_int32, true, NULL, 0, &blurxy_tiramisu);
    tiramisu::constant bx_s0_c_ext("bx_s0_c_ext", tiramisu::expr(by_ext_2), tiramisu::p_int32, true, NULL, 0, &blurxy_tiramisu);

    // Define loop bounds for dimension "blur_x_s0_y".
    tiramisu::constant bx_s0_y_min("bx_s0_y_min", tiramisu::expr((int32_t)0), tiramisu::p_int32, true, NULL, 0, &blurxy_tiramisu);
    tiramisu::constant bx_s0_y_ext("bx_s0_y_ext", (tiramisu::expr(by_ext_1) + tiramisu::expr((int32_t)2)), tiramisu::p_int32, true, NULL, 0, &blurxy_tiramisu);

    // Define loop bounds for dimension "blur_x_s0_x".
    tiramisu::constant bx_s0_x_min("bx_s0_x_min", tiramisu::expr((int32_t)0), tiramisu::p_int32, true, NULL, 0, &blurxy_tiramisu);
    tiramisu::constant bx_s0_x_ext("bx_s0_x_ext", tiramisu::expr(by_ext_0), tiramisu::p_int32, true, NULL, 0, &blurxy_tiramisu);
    tiramisu::computation blur_x_s0("[bx_s0_c_min, bx_s0_c_ext, bx_s0_y_min, bx_s0_y_ext, bx_s0_x_min, bx_s0_x_ext]->{blur_x_s0[blur_x_s0_c, blur_x_s0_y, blur_x_s0_x]: "
                        "(bx_s0_c_min <= blur_x_s0_c <= ((bx_s0_c_min + bx_s0_c_ext) + -1)) and (bx_s0_y_min <= blur_x_s0_y <= ((bx_s0_y_min + bx_s0_y_ext) + -1)) and (bx_s0_x_min <= blur_x_s0_x <= ((bx_s0_x_min + bx_s0_x_ext) + -1))}",
                        (((p0(tiramisu::idx("blur_x_s0_c"), tiramisu::idx("blur_x_s0_y"), tiramisu::idx("blur_x_s0_x")) + p0(tiramisu::idx("blur_x_s0_c"), tiramisu::idx("blur_x_s0_y"), (tiramisu::idx("blur_x_s0_x") + tiramisu::expr((int32_t)1)))) + p0(tiramisu::idx("blur_x_s0_c"), tiramisu::idx("blur_x_s0_y"), (tiramisu::idx("blur_x_s0_x") + tiramisu::expr((int32_t)2))))/tiramisu::expr((uint8_t)3)), true, tiramisu::p_uint8, &blurxy_tiramisu);
    blur_x_s0.set_access("{blur_x_s0[blur_x_s0_c, blur_x_s0_y, blur_x_s0_x]->buff_bx[blur_x_s0_c, blur_x_s0_y, blur_x_s0_x]}");

    // Define loop bounds for dimension "blur_y_s0_c".
    tiramisu::constant by_s0_c_min("by_s0_c_min", tiramisu::expr((int32_t)0), tiramisu::p_int32, true, NULL, 0, &blurxy_tiramisu);
    tiramisu::constant by_s0_c_ext("by_s0_c_ext", tiramisu::expr(by_ext_2), tiramisu::p_int32, true, NULL, 0, &blurxy_tiramisu);

    // Define loop bounds for dimension "blur_y_s0_y".
    tiramisu::constant by_s0_y_min("by_s0_y_min", tiramisu::expr((int32_t)0), tiramisu::p_int32, true, NULL, 0, &blurxy_tiramisu);
    tiramisu::constant by_s0_y_ext("by_s0_y_ext", tiramisu::expr(by_ext_1), tiramisu::p_int32, true, NULL, 0, &blurxy_tiramisu);

    // Define loop bounds for dimension "blur_y_s0_x".
    tiramisu::constant by_s0_x_min("by_s0_x_min", tiramisu::expr((int32_t)0), tiramisu::p_int32, true, NULL, 0, &blurxy_tiramisu);
    tiramisu::constant by_s0_x_ext("by_s0_x_ext", tiramisu::expr(by_ext_0), tiramisu::p_int32, true, NULL, 0, &blurxy_tiramisu);
    tiramisu::computation blur_y_s0("[by_s0_c_min, by_s0_c_ext, by_s0_y_min, by_s0_y_ext, by_s0_x_min, by_s0_x_ext]->{blur_y_s0[blur_y_s0_c, blur_y_s0_y, blur_y_s0_x]: "
                        "(by_s0_c_min <= blur_y_s0_c <= ((by_s0_c_min + by_s0_c_ext) + -1)) and (by_s0_y_min <= blur_y_s0_y <= ((by_s0_y_min + by_s0_y_ext) + -1)) and (by_s0_x_min <= blur_y_s0_x <= ((by_s0_x_min + by_s0_x_ext) + -1))}",
                        (((blur_x_s0(tiramisu::idx("blur_y_s0_c"), tiramisu::idx("blur_y_s0_y"), tiramisu::idx("blur_y_s0_x")) + blur_x_s0(tiramisu::idx("blur_y_s0_c"), (tiramisu::idx("blur_y_s0_y") + tiramisu::expr((int32_t)1)), tiramisu::idx("blur_y_s0_x"))) + blur_x_s0(tiramisu::idx("blur_y_s0_c"), (tiramisu::idx("blur_y_s0_y") + tiramisu::expr((int32_t)2)), tiramisu::idx("blur_y_s0_x")))/tiramisu::expr((uint8_t)3)), true, tiramisu::p_uint8, &blurxy_tiramisu);
    blur_y_s0.set_access("{blur_y_s0[blur_y_s0_c, blur_y_s0_y, blur_y_s0_x]->buff_by[blur_y_s0_c, blur_y_s0_y, blur_y_s0_x]}");

    // Define compute level for "blur_y".
    blur_y_s0.after(blur_x_s0, computation::root_dimension);

    // Add schedules.
    blur_x_s0.tag_parallel_level(1);
    blur_x_s0.tag_parallel_level(0);
    blur_y_s0.tag_parallel_level(1);
    blur_y_s0.tag_parallel_level(0);

    blurxy_tiramisu.set_arguments({&buff_p0, &buff_by});
    blurxy_tiramisu.gen_time_processor_domain();
    blurxy_tiramisu.gen_isl_ast();
    blurxy_tiramisu.gen_halide_stmt();
    blurxy_tiramisu.dump_halide_stmt();
    blurxy_tiramisu.gen_halide_obj("build/generated_fct_test_14.o");
    blurxy_tiramisu.gen_c_code();

    return 0;
}