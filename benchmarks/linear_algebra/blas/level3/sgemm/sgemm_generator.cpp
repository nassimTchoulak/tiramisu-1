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

#include "sgemm_wrapper.h"

using namespace tiramisu;

/**
 * Benchmark sgemm
 *     C = aAB + bC
 */

void generate_function(std::string name)
{
    tiramisu::global::set_default_tiramisu_options();

    // -------------------------------------------------------
    // Layer I
    // -------------------------------------------------------

    tiramisu::function function0(name);
    tiramisu::var i("i"), j("j"), k("k");
    tiramisu::computation SIZES("{SIZES[i]}", tiramisu::expr(), false, p_int32, &function0);
    tiramisu::computation alpha("{alpha[i]}", tiramisu::expr(), false, p_float32, &function0);
    tiramisu::computation beta("{beta[i]}", tiramisu::expr(), false, p_float32, &function0);
    tiramisu::computation A("{A[i,j]}", tiramisu::expr(), false, p_float32, &function0);
    tiramisu::computation B("{B[i,j]}", tiramisu::expr(), false, p_float32, &function0);
    tiramisu::computation C("{C[i,j]}", tiramisu::expr(), false, p_float32, &function0);
    tiramisu::constant N("N", SIZES(0), p_int32, true, NULL, 0, &function0);
    tiramisu::constant M("M", SIZES(1), p_int32, true, NULL, 0, &function0);
    tiramisu::constant K("K", SIZES(2), p_int32, true, NULL, 0, &function0);
    tiramisu::constant a("a", alpha(0), p_float32, true, NULL, 0, &function0);
    tiramisu::constant b("b", beta(0), p_float32, true, NULL, 0, &function0);


    tiramisu::computation reduced_AB_0("[N, M, K]->{reduced_AB_0[i,j,-1]: 0<=i<N and 0<=j<M}", (float) 0, true, p_float32, &function0);
    tiramisu::computation reduced_AB_1("[N, M, K]->{reduced_AB_1[i,j,k]: 0<=i<N and 0<=j<M and 0<=k<K}", reduced_AB_0(i,j,0) + A(i,k)*B(k,j), true, p_float32, &function0);
    tiramisu::computation result("[N, M, K]->{result[i,j]: 0<=i<N and 0<=j<M}", tiramisu::var(p_float32, "a") * reduced_AB_1(i,j,0) + tiramisu::var(p_float32, "b") * C(i,j) , true, p_float32, &function0);


     function0.add_context_constraints("[N, M, K]->{:N>1 and K>1 and M>1 and N=1024 and M=1024 and K=1024}");

    // -------------------------------------------------------
    // Layer II
    // -------------------------------------------------------

#define B0 32
#define B1 32
#define B2 16

#define L3_B0 8
#define L3_B1 8
#define L3_B2 8


    // L2 3D Tiling
    reduced_AB_0.tile(0,1, B0,B1);
    reduced_AB_1.tile(0,1,2, B0,B1,B2);
    result.tile(0,1, B0,B1);

    // L3 3D Tiling
    reduced_AB_0.tile(0,1, L3_B0,L3_B1);
    reduced_AB_1.tile(0,1,2, L3_B0,L3_B1,L3_B2);
    result.tile(0,1, L3_B0,L3_B1);

    // Vectorization
    // reduced_AB_0.tag_vector_level(5, B1);
    reduced_AB_1.tag_vector_level(7, B1);
    // result.tag_vector_level(5, B1);

    // reduced_AB_1.tag_unroll_level(6);

    reduced_AB_0.tag_parallel_level(0);

    reduced_AB_1.after(reduced_AB_0, 5);
    result.after(reduced_AB_1, 5);


    // -------------------------------------------------------
    // Layer III
    // -------------------------------------------------------

    tiramisu::buffer buf_SIZES("buf_SIZES", {3}, tiramisu::p_int32, a_input, &function0);
    tiramisu::buffer buf_alpha("buf_alpha", {1}, tiramisu::p_float32, a_input, &function0);
    tiramisu::buffer buf_beta("buf_beta", {1}, tiramisu::p_float32, a_input, &function0);
    tiramisu::buffer buf_A("buf_A", {tiramisu::var(p_int32, "N"), tiramisu::var(p_int32, "K")}, tiramisu::p_float32, a_input, &function0);
    tiramisu::buffer buf_B("buf_B", {tiramisu::var(p_int32, "K"), tiramisu::var(p_int32, "M")}, tiramisu::p_float32, a_input, &function0);
    tiramisu::buffer buf_temps("buf_temp", {tiramisu::var(p_int32, "N"), tiramisu::var(p_int32, "M")}, tiramisu::p_float32, a_temporary, &function0);
    tiramisu::buffer buf_C("buf_C", {tiramisu::var(p_int32, "N"), tiramisu::var(p_int32, "M")}, tiramisu::p_float32, a_output, &function0);


    SIZES.set_access("{SIZES[i]->buf_SIZES[i]}");
    alpha.set_access("{alpha[i]->buf_alpha[i]}");
    beta.set_access("{beta[i]->buf_beta[i]}");
    A.set_access("{A[i,j]->buf_A[i,j]}");
    B.set_access("{B[i,j]->buf_B[i,j]}");
    C.set_access("{C[i,j]->buf_C[i,j]}");
    reduced_AB_0.set_access("{reduced_AB_0[i,j,k]->buf_temp[i,j]}");
    reduced_AB_1.set_access("{reduced_AB_1[i,j,k]->buf_temp[i,j]}");
    result.set_access("{result[i,j]->buf_C[i,j]}");

    // -------------------------------------------------------
    // Code Generation
    // -------------------------------------------------------

    function0.set_arguments({&buf_SIZES, &buf_alpha, &buf_beta, &buf_A, &buf_B, &buf_C});
    function0.gen_time_space_domain();
    function0.gen_isl_ast();
    function0.gen_halide_stmt();
    function0.gen_halide_obj("generated_sgemm.o");
}

int main(int argc, char **argv)
{
    generate_function("sgemm_tiramisu");

    return 0;
}
