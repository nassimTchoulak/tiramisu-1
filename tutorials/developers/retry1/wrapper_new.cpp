#include "Halide.h"
#include "tiramisu/utils.h"
#include "new.o.h"
#include <cstdlib>
#include <iostream>

int main(){

    Halide::Buffer<double> input(5000);
    Halide::Buffer<double> input2(5000);
    
    function_0(input.raw_buffer(),input2.raw_buffer()) ;
    
    

    return 0 ;
}