#include <tiramisu/tiramisu.h>
#include <isl/set.h>
#include <isl/ctx.h>
#include <isl/aff.h>
#include <isl/set.h>
#include <isl/map.h>
#include <isl/id.h>
#include <isl/flow.h>
#include <isl/constraint.h>
#include <isl/union_map.h>
#include <isl/union_set.h>
#include <isl/ast_build.h>
#include <iostream>

int main(int argc, char **argv)
{

    tiramisu::init("function_0");


    // Algorithm
    //tiramisu::constant N("N", tiramisu::expr((int32_t) 100));
    tiramisu::constant N("N", tiramisu::expr((int32_t) 10));
    tiramisu::var i("i", 1, N-2), j("j", 1, N-2);
    tiramisu::var i1("i1"), j1("j1");
    tiramisu::var i2("i2"), j2("j2");

    tiramisu::input A("A", {i, j}, tiramisu::p_uint8);


    //Computations
    tiramisu::computation B_out("B_out", {i,j}, A(i-1,j) + A(i+1,j) + A(i,j-1) +A(i,j+1) + A(i,10-j)  );
    tiramisu::computation A_out("A_out", {i,j}, B_out(i,j) + 5);

    // -------------------------------------------------------
    // Layer II
    // -------------------------------------------------------
    B_out.then(A_out,tiramisu::computation::root);

    // -------------------------------------------------------
    // Layer III
    // -------------------------------------------------------
    //Input Buffers
    tiramisu::buffer b_A("b_A", {N,N}, tiramisu::p_uint8, tiramisu::a_input);    

    //Store inputs
    A.store_in(&b_A);

    //Store computations
    A_out.store_in(&b_A, {i,j});
    B_out.store_in(&b_A, {i,j});


    tiramisu::performe_full_dependecy_analysis();

    //tests

    tiramisu::function * fct = tiramisu::global::get_implicit_function();

    assert(tiramisu::check_legality_of_function() == true);


    

    B_out.then(A_out,j);
    //A_out.shift(i,1);
    //A_out.shift(j,1);
    fct->reset_all_static_dims_to_zero();
    tiramisu::prepare_schedules_for_legality_checks();
    //fct->reset_all_static_dims_to_zero();
    //assert(check_legality_of_function() == false);
    //assert(fct->check_partiel_legality_in_function({&A_out,&B_out}) == false);

    auto shiftings = fct->correcting_loop_fusion_with_shifting({&B_out},A_out,{i});

    assert(shiftings.size() > 0);

    for(auto const& tup:shiftings)
    {
        A_out.shift(
            std::get<0>(tup),
            std::get<1>(tup)
            );
    }

    // legality check of function
    //prepare_schedules_for_legality_checks(true) ;

    assert(tiramisu::check_legality_of_function() == true) ;


    tiramisu::codegen({&b_A}, "new_1.o");



    return 0;
    
}