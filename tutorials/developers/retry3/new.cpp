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


    tiramisu::var i("i",0,6000) ;
    tiramisu::var j("j",0,6000) ;
    tiramisu::var t("t",1,1000);



    tiramisu::var i1("i1");
    tiramisu::var i2("i2");
    tiramisu::var j1("j1");
    tiramisu::var j2("j2");
    tiramisu::var j3("j3");


    


    /*
      declaring the computations
    */


    tiramisu::computation A("A", {i,j}, 0.5);
    tiramisu::computation B("B", {i,j}, 0.5);

    //Computations
    tiramisu::computation B_out("B_out", {t,i,j}, ( A(i-1,j+1) + A(i,j+1) + A(i+1,j+1) + 
                                                  A(i-1,j) + A(i,j) + A(i+1,j)+
                                                  A(i-1,j-1)+ A(i,j-1) + A(i+1,j-1) )
                                                  * 0.125 );

    tiramisu::computation A_out("A_out", {t,i,j}, (  B(i+1,j-1) + B(i+1,j+1) + B(i-1,j+1) +B(i-1,j-1) )*0.25  );

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
    tiramisu::buffer b_A("b_A", {6000,6000}, tiramisu::p_float64, tiramisu::a_output);    
    tiramisu::buffer b_B("b_B", {6000,6000}, tiramisu::p_float64, tiramisu::a_output);    

    //Store inputs
    A.store_in(&b_A);
    B.store_in(&b_B);

    //Store computations
    A_out.store_in(&b_A, {i,j});
    B_out.store_in(&b_B, {i,j});

     
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

    
    //A_out.after_change(B_out,j) ;
    //A_out.shift(i,1);
    //A_out.shift(j,1);
    

    tiramisu::prepare_schedules_for_legality_checks() ;

    bool optimize = false ;

    int a_alpha = 0 ;
    int b_beta = 0 ;
    bool inner_paral = false ;
    int version = 0;

    // parts that changes with sed 

    optimize=false ;
    a_alpha=3 ;
    b_beta=2 ;
    inner_paral=false;

    version=9;
    // v0 sequentiel
    // v1 sequentiel fuzed
    // v2 infused parallel 
    // v3 fuze+skewing
    // v4 fuze + skewing + parallel
    // v5 fuze + skewing + parallel splitted
    // v6 fuze + skewing outer + parallel 

    // end sed parts 

    tiramisu::function * fct = tiramisu::global::get_implicit_function() ;
      A_out.after_change(B_out,j) ;
       A_out.shift(i,1);
       A_out.shift(j,1);
       fct->prepare_schedules_for_legality_checks();
     fct->compute_best_legal_skewing({&B_out,&A_out},t,i);


    switch(version) {
    case 0:
      // code block
      std::cout<<" sequentiel unfuzed ";

      break;
    case 1:
      // code block
       std::cout<<" sequentiel + fuzed ";
       A_out.after_change(B_out,j) ;
       A_out.shift(i,1);
       A_out.shift(j,1);

      break;
    case 2:
      // code block
      std::cout<<" parallel + unfuzed ";
      A_out.parallelize(i);
      B_out.parallelize(i);

      break;

    case 3:
      // code block
      std::cout<<" parallel + unfuzed +split ";

      A_out.split(i,200,j2,j3) ;
      B_out.split(i,200,j2,j3) ;
      A_out.parallelize(j2);
      B_out.parallelize(j2);

      break;
    case 4:
      // code block
      std::cout<<" fuzed + inner skewing";
       A_out.after_change(B_out,j) ;
       A_out.shift(i,1);
       A_out.shift(j,1);

       A_out.angle_skew(i,j,1,1,false,i1,j1);
       B_out.angle_skew(i,j,1,1,false,i1,j1);

      break;
    case 5:
      // code block
      std::cout<<" fuzed + inner skewing + parallel  ";
       A_out.after_change(B_out,j) ;
       A_out.shift(i,1);
       A_out.shift(j,1);

       A_out.angle_skew(i,j,2,1,false,i1,j1);
       B_out.angle_skew(i,j,2,1,false,i1,j1);

       A_out.parallelize(j1);

      break;

    case 6:
      // code block
      std::cout<<" fuzed + skewing + parallel + splitting";
       A_out.after_change(B_out,j) ;
       A_out.shift(i,1);
       A_out.shift(j,1);

       A_out.angle_skew(i,j,2,1,false,i1,j1);
       B_out.angle_skew(i,j,2,1,false,i1,j1);

       A_out.split(j1,200,j2,j3) ;
       B_out.split(j1,200,j2,j3) ;

       A_out.parallelize(j2);

    break;

    case 7:
      // code block
      std::cout<<" fuzed + outer skewing + parallel ";
       A_out.after_change(B_out,j) ;
       A_out.shift(i,1);
       A_out.shift(j,1);

       A_out.angle_skew(t,i,2,1,false,i1,j1);
       B_out.angle_skew(t,i,2,1,false,i1,j1);

       A_out.parallelize(j1);

    break;

    case 8:
      // code block
      std::cout<<" fuzed + outer skewing + parallel + split";
       A_out.after_change(B_out,j) ;
       A_out.shift(i,1);
       A_out.shift(j,1);

       A_out.angle_skew(t,i,2,1,false,i1,j1);
       B_out.angle_skew(t,i,2,1,false,i1,j1);

       A_out.split(j1,200,j2,j3) ;
       B_out.split(j1,200,j2,j3) ;

       A_out.parallelize(j2);

    break;

    default:
      // code block
      std::cout<<" none ";

    }

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
    tiramisu::prepare_schedules_for_legality_checks();
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