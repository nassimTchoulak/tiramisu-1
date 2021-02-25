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


    tiramisu::var i("i",0,3000) ;
    tiramisu::var j("j",0,3000) ;
    tiramisu::var t("t",1,1000);



    tiramisu::var i1("i1");
    tiramisu::var i2("i2");
    tiramisu::var j1("j1");
    tiramisu::var j2("j2");


    


    /*
      declaring the computations
    */


    tiramisu::computation A("A", {i}, 0.5);
    tiramisu::computation B("B", {i}, 0.5);

    //Computations
    tiramisu::computation B_out("B_out", {t,i}, (A(i-1) + A(i) + A(i + 1))*0.33333);

    tiramisu::computation A_out("A_out", {t,i}, (B(i-1) + B(i) + B(i + 1))*0.33333);

    // -------------------------------------------------------
    // Layer II
    // -------------------------------------------------------
    A.then(B,i)
    .then(B_out,tiramisu::computation::root)
    .then(A_out,t) ;

    

    
    // -------------------------------------------------------
    // Layer III
    // -------------------------------------------------------
    //Input Buffers
    tiramisu::buffer b_A("b_A", {3000}, tiramisu::p_float64, tiramisu::a_output);    
    tiramisu::buffer b_B("b_B", {3000}, tiramisu::p_float64, tiramisu::a_output);    

    //Store inputs
    A.store_in(&b_A);
    B.store_in(&b_B);

    //Store computations
    A_out.store_in(&b_A, {i});
    B_out.store_in(&b_B, {i});

     
    // get the function object

    
    

   /*
   
    performe a full dependecy analysis RAW/WAR/WAW and the result is stored in attributes inside the function
    to invoke this method user must : define computations order & define the buffer mapped to each computation

   */

    tiramisu::performe_full_dependecy_analysis();

    A.parallelize(i) ;
    //A.vectorize(j,10) ;
/*
  the list of legality checks are methods that return a boolean :
        True if it's legal & false if not

  [they must be invoked after performe_full_dependecy_analysis() since they use these results ]
*/

    // full check of legality for this function

    
    A_out.after_change(B_out,i) ;
    A_out.shift(i,1) ;

    tiramisu::check_legality_of_function();

    tiramisu::prepare_schedules_for_legality_checks() ;

    bool optimize = false ;

    int a_alpha = 0 ;
    int b_beta = 0 ;
    bool inner_paral = false ;

    // parts that changes with sed 

    optimize=false ;
    a_alpha=3 ;
    b_beta=2 ;
    inner_paral=false;

    // end sed parts 

    if(optimize)
    {

      A_out.angle_skew(t,i,a_alpha,b_beta,false,i1,j1) ;
      B_out.angle_skew(t,i,a_alpha,b_beta,false,i1,j1) ;

      if(!A_out.schedules_pair_relation_is_legal(&B_out))
      {
        A_out.loop_reversal(j1,j2) ;
        B_out.loop_reversal(j1,j2) ;



      }
      else{

           if(inner_paral)
            {

              if(tiramisu::loop_parallelization_is_legal(j1,{&A_out,&B_out}))
              {
                A_out.parallelize(j1) ;std::cout<<" inner parallel ";
              }
            }


      }

     

    }

    if(tiramisu::check_legality_of_function())
    {
       std::cout<<" legal ";
    }
    else
    {
      std::cout<<" illegal ";
    }
    
    

    tiramisu::codegen({&b_A,&b_B}, "new_1.o");



    return 0;
    
}