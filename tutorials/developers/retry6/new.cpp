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

    // declaring variables
    tiramisu::constant N("N", tiramisu::expr((int32_t) 20));

    tiramisu::var i("i",0,N) ;
    tiramisu::var j("j",0,N) ;
    tiramisu::var t("t",1,50);

    tiramisu::var i1("i1");
    tiramisu::var i2("i2");
    tiramisu::var j1("j1");
    tiramisu::var j2("j2");
    tiramisu::var j3("j3");

    /*
      declaring the computations
    */


    tiramisu::computation A("A", {i,j}, 0.5);

    //Computations
    tiramisu::computation B_out("B_out", {i,j}, ( A(i-1,j-1)  + A(i-1,j+1))* 0.5 );

    //tiramisu::computation B_out("B_out", {i,j}, ( A(i-1,j-1) + A(i-1,j) + A(i-1,j+1))* 0.5 );

    // -------------------------------------------------------
    // Layer II
    // -------------------------------------------------------
    A.then(B_out,tiramisu::computation::root);

    // -------------------------------------------------------
    // Layer III
    // -------------------------------------------------------
    //Input Buffers
   
    tiramisu::buffer b_B("b_B", {N,N}, tiramisu::p_float64, tiramisu::a_output);    

    //Store inputs
    A.store_in(&b_B);
    //Store computations
    B_out.store_in(&b_B, {i,j});
    // get the function object

  

    tiramisu::performe_full_dependecy_analysis();

    A.parallelize(i) ;



    tiramisu::prepare_schedules_for_legality_checks() ;

  

    tiramisu::function * fct = tiramisu::global::get_implicit_function() ;


      fct->prepare_schedules_for_legality_checks();

      int val=0;
      
      //fct->compute_best_legal_skewing({&B_out,&A_out},t,i,val);

      fct->skewing_local_solver({&B_out},i,j,3);


    
    tiramisu::prepare_schedules_for_legality_checks();
    if(tiramisu::check_legality_of_function())
    {
       std::cout<<" legal ";
    }
    else
    {
      std::cout<<" illegal ";
    }

    
    

    tiramisu::codegen({&b_B}, "new_1.o");



    return 0;
    
}