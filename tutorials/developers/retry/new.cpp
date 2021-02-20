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

    /*
        
      for (t = 0; t <= _PB_TSTEPS - 1; t++)
        for (i = 1; i<= _PB_N - 2; i++)
          for (j = 1; j <= _PB_N - 2; j++)
              A[i][j] = (A[i-1][j-1] + A[i-1][j] + A[i-1][j+1]
                        + A[i][j-1] + A[i][j] + A[i][j+1]
                      + A[i+1][j-1] + A[i+1][j] + A[i+1][j+1])/9.0;
          
    
    */


    tiramisu::computation C_init("C_init", {i,j}, 0.5 );

    

    tiramisu::computation S0("A_out", {t,i,j}, (C_init(i-1, j-1) + C_init(i-1, j) + C_init(i-1, j+1)
		                               + C_init(i, j-1) + C_init(i, j) + C_init(i, j+1)
		                               + C_init(i+1, j-1) + C_init(i+1, j) + C_init(i+1, j+1))/9.0);
    

    // exec order
  
    C_init.then(S0,tiramisu::computation::root) ;

    // declaring buffers
    tiramisu::buffer b_output("b_output", {3000,3000}, tiramisu::p_float64, tiramisu::a_output);

    

    // mapping the computations to buffers
    // for S0 the outer most variable i is free, allowing repetitive calculations 
   

    C_init.store_in(&b_output,{i,j});
    S0.store_in(&b_output,{i,j});

     
    // get the function object

    
    

   /*
   
    performe a full dependecy analysis RAW/WAR/WAW and the result is stored in attributes inside the function
    to invoke this method user must : define computations order & define the buffer mapped to each computation

   */

    tiramisu::performe_full_dependecy_analysis();

    C_init.parallelize(i) ;
    C_init.vectorize(j,10) ;
/*
  the list of legality checks are methods that return a boolean :
        True if it's legal & false if not

  [they must be invoked after performe_full_dependecy_analysis() since they use these results ]
*/

    // full check of legality for this function

    tiramisu::check_legality_of_function();

    tiramisu::prepare_schedules_for_legality_checks() ;

    bool optimize = false ;

    int a_alpha = 0 ;
    int b_beta = 0 ;
    bool inner_paral = false ;

    // parts that changes with sed 

    optimize=true ;
    a_alpha=3 ;
    b_beta=2 ;
    inner_paral=false;

    // end sed parts 

    if(optimize)
    {

      S0.angle_skew(t,i,a_alpha,b_beta,false,i1,j1) ;

      if(!S0.schedules_pair_relation_is_legal(&S0))
      {
        S0.loop_reversal(j1,j2) ;



      }
      else{

           if(inner_paral)
            {
              if(S0.parallelization_is_legal(j1))
              {
                S0.parallelize(j1) ;std::cout<<" inner parallel ";
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
    
    

    tiramisu::codegen({&b_output}, "new_1.o");



    return 0;
    
}