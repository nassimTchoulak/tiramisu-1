#include "Halide.h"
#include "tiramisu/utils.h"
#include "new.o.h"
#include <cstdlib>
#include <iostream>

int main(){

    Halide::Buffer<double> input(6000,6000);
    function_0(input.raw_buffer()) ;
    
    

    return 0 ;
}
