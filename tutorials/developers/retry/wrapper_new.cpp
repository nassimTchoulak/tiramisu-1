#include "Halide.h"
#include "tiramisu/utils.h"
#include "new.o.h"
#include <cstdlib>
#include <iostream>

int main(){

    Halide::Buffer<double> input(3000,3000);
    
    function_0(input.raw_buffer()) ;
    
    

    return 0 ;
}
