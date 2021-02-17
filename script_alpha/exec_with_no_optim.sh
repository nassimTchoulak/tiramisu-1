			export TIRAMISU_ROOT=/home/nassim/Desktop/tiramisu/
			export LD_LIBRARY_PATH=/home/nassim/Desktop/tiramisu/3rdParty/Halide/lib
			cd ${TIRAMISU_ROOT}/build
			sed -i -E "s/optimize=(false|true)/optimize=false/gi" ${TIRAMISU_ROOT}/tutorials/developers/retry/new.cpp
			

			g++ -std=c++11 -fno-rtti -DHALIDE_NO_JPEG -I${TIRAMISU_ROOT}/include -I${TIRAMISU_ROOT}/3rdParty/isl/include/ -I${TIRAMISU_ROOT}/3rdParty/Halide/include -I${TIRAMISU_ROOT}/build -L${TIRAMISU_ROOT}/build -L${TIRAMISU_ROOT}/3rdParty/isl/build/lib -L${TIRAMISU_ROOT}/3rdParty/Halide/lib/ -o ${TIRAMISU_ROOT}/build/new_generator  -ltiramisu -lisl -lHalide -ldl -lpthread -lz -lm -Wl,-rpath,${TIRAMISU_ROOT}/build ${TIRAMISU_ROOT}/tutorials/developers/retry/new.cpp -ltiramisu -lisl -lHalide -ldl -lpthread -lz -lm   

			./new_generator > log_test.txt
			#code generator ready

			g++ -std=c++11 -fno-rtti -I${TIRAMISU_ROOT}/include -I${TIRAMISU_ROOT}/3rdParty/Halide/include -L${TIRAMISU_ROOT}/build -L${TIRAMISU_ROOT}/3rdParty/Halide/lib/ -o exe_new  -ltiramisu -lHalide -ldl -lpthread -lz -lm -Wl,-rpath,${TIRAMISU_ROOT}/build ${TIRAMISU_ROOT}/tutorials/developers/retry/wrapper_new.cpp  ${TIRAMISU_ROOT}/build/new_1.o -ltiramisu -lHalide -ldl -lpthread -lz -lm >> log_test.txt
			
			a=$(date +%s)
			./exe_new 
			b=$(date +%s)
			echo a:$var1 b:$var2 c:$var3 diff=$((b-a)) >> log_test.txt
