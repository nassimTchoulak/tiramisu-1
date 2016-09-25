#include <isl/set.h>
#include <isl/union_map.h>
#include <isl/union_set.h>
#include <isl/ast_build.h>
#include <isl/schedule.h>
#include <isl/schedule_node.h>

#include <coli/debug.h>
#include <coli/core.h>

#include <string.h>
#include <Halide.h>

using namespace coli;

void generate_function_1(std::string name, int size, int val0, int val1)
{
    coli::global::set_default_coli_options();

    coli::function function0(name);
    coli::expr e_N = coli::expr((int32_t) size);
    coli::constant N("N", &e_N, p_int32, true, NULL, 0, &function0);

    coli::computation S0("[N]->{S0[i,j]: 0<=i<N and 0<=j<N}", NULL, true, p_uint8, &function0);
    coli::expr e_M = coli::expr((uint8_t) val1);
    coli::constant M("M", &e_M, p_uint8, true, &S0, 0, &function0);
    coli::expr e1 = coli::expr((uint8_t) val0) + coli::var(p_uint8, "M");
    S0.set_expression(&e1);

    coli::buffer buf0("buf0", 2, {size,size}, coli::p_uint8, NULL,
                        a_output, &function0);
    S0.set_access("{S0[i,j]->buf0[i,j]}");
    S0.tile(0,1,2,2);
    S0.tag_parallel_dimension(0);

    function0.set_arguments({&buf0});
    function0.gen_isl_ast();
    function0.gen_halide_stmt();
    function0.gen_halide_obj("build/generated_fct_test_04.o");
}


int main(int argc, char **argv)
{
    generate_function_1("test_let_stmt", 1000, 3, 17);

    return 0;
}
