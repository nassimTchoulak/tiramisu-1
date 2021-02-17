export TIRAMISU_ROOT=/home/nassim/Desktop/tiramisu/
export LD_LIBRARY_PATH=/home/nassim/Desktop/tiramisu/3rdParty/Halide/lib
cd ${TIRAMISU_ROOT}/build
echo starting test for siedel stencil
#sed -i "s/inner_paral=(true|false)/inner_paral=false/gi" test.txt
#sed -i "s/b_beta=([0-9]|-[0-9])/b_beta=1/gi" test.txt


for var1 in 1 2 3
do
	for var2 in -1 -2 1 2
	do
		for var3 in 'true' 'false'
		do
			sed -i -E "s/optimize=(false|true)/optimize=true/gi" ${TIRAMISU_ROOT}/tutorials/developers/retry/new.cpp
			sed -i -E "s/a_alpha=([0-9]|-[0-9])/a_alpha=$var1/gi" ${TIRAMISU_ROOT}/tutorials/developers/retry/new.cpp
			sed -i -E "s/b_beta=([0-9]|-[0-9])/b_beta=$var2/gi" ${TIRAMISU_ROOT}/tutorials/developers/retry/new.cpp
			sed -i -E "s/inner_paral=(true|false)/inner_paral=$var3/gi" ${TIRAMISU_ROOT}/tutorials/developers/retry/new.cpp

			g++ -std=c++11 -fno-rtti -DHALIDE_NO_JPEG -I${TIRAMISU_ROOT}/include -I${TIRAMISU_ROOT}/3rdParty/isl/include/ -I${TIRAMISU_ROOT}/3rdParty/Halide/include -I${TIRAMISU_ROOT}/build -L${TIRAMISU_ROOT}/build -L${TIRAMISU_ROOT}/3rdParty/isl/build/lib -L${TIRAMISU_ROOT}/3rdParty/Halide/lib/ -o ${TIRAMISU_ROOT}/build/new_generator  -ltiramisu -lisl -lHalide -ldl -lpthread -lz -lm -Wl,-rpath,${TIRAMISU_ROOT}/build ${TIRAMISU_ROOT}/tutorials/developers/retry/new.cpp -ltiramisu -lisl -lHalide -ldl -lpthread -lz -lm   

			./new_generator >> log_test.txt
			#code generator ready

			g++ -std=c++11 -fno-rtti -I${TIRAMISU_ROOT}/include -I${TIRAMISU_ROOT}/3rdParty/Halide/include -L${TIRAMISU_ROOT}/build -L${TIRAMISU_ROOT}/3rdParty/Halide/lib/ -o exe_new  -ltiramisu -lHalide -ldl -lpthread -lz -lm -Wl,-rpath,${TIRAMISU_ROOT}/build ${TIRAMISU_ROOT}/tutorials/developers/retry/wrapper_new.cpp  ${TIRAMISU_ROOT}/build/new_1.o -ltiramisu -lHalide -ldl -lpthread -lz -lm >> log_test.txt
			
			a=$(date +%s)
			./exe_new 
			b=$(date +%s)
			echo a:$var1 b:$var2 c:$var3 diff=$((b-a)) >> log_test.txt
		done
	done
done

