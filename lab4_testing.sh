#!/bin/bash
# this script is intended to be run from within each student's lab directory. It should be copied there.
LABN=lab4

NUM_TESTS=4

declare -a file_list=("st_loop.x" "mov_simple.x" "br_same.x" "ld.x" "cancel_req.x" "test1.x") # "st_ld_loop.x"

#cp sim src/
rm "$LABN-test.txt"
echo "$LABN-test.txt"  >> "$LABN-test.txt"
echo "---------"  >> "$LABN-test.txt"

printf "\nSection 1: cycle-by-cycle comparison\n"  >> "$LABN-test.txt"

for inputfile in "${file_list[@]}";
do
	echo "Test: $inputfile" >> "$LABN-test.txt"
	rm diff_${inputfile}.txt # remove the file if it has previously been created
	echo "$inputfile"
	# Get ref and student total cycles
	echo "go\n rdump" | timeout 3 ../sim inputs/${inputfile} > reference_halt.txt
	echo "go\n rdump" | timeout 3 ./sim inputs/${inputfile} > actual_halt.txt
	ref_cycles=$(grep -oP "No. of Cycles:\s+\K\d+" reference_halt.txt)
	student_cycles=$(grep -oP "No. of Cycles:\s+\K\d+" actual_halt.txt)
	ref_cycles=$((ref_cycles+1))
	student_cycles=$((student_cycles+1))

	value=1
	i=1
	mismatch=0
	matching_cycles=0
	cycle=1
	reference_num_cycles=0
	ir_only=1 # Instructions Retired flag
	# echo "*Cycles*"
	while [ $i -eq $value ]
		do
			# echo $cycle
			echo "run $cycle\n rdump\n mdump 0x10000000 0x100000ff" | timeout 3 ../sim inputs/${inputfile} > reference.txt
			cd src
			echo "run $cycle\n rdump\n mdump 0x10000000 0x100000ff" | timeout 3 ./sim ../inputs/${inputfile} > actual.txt

			cd ..
			# pwd
			echo "------------------cycle $cycle---------------" >> diff_${inputfile}.txt
			value2=$(echo "$value2" | grep -r 'halted' reference.txt)
			value3=$(echo "$value3" | grep -r 'halted' src/actual.txt)
			if [[ -z "${value2// }" ]] && [[ -z "${value3// }" ]]
				then
					cycle=$((cycle+1))
				else
					i=2
			fi

			diff ./dumpsim ./src/dumpsim >> diff_${inputfile}.txt
			diff ./dumpsim ./src/dumpsim > curr_diff_${inputfile}.txt
			if ! [[ -s curr_diff_${inputfile}.txt ]] # [ -s FILE ] True if FILE exists and has a size greater than zero. if the file is empty, increment matching cycles
				then
					let "matching_cycles++"
				else
					# Grab number of lines = first integer outputted by wc
					num_lines=$(wc -l curr_diff_${inputfile}.txt | sed 's/^[^0-9]*\([0-9]\+\).*$/\1/')
					echo "num lines diff = $num_lines"
					if [[ $num_lines -eq 4 ]]; then
				   		# If only 1 mismatch in the diff output file
				   		# check if file contains 'Instructions Retired' string
				   		grep -q "Instruction Retired" curr_diff_${inputfile}.txt # -q means don't output anything
				   		if [[ $? -ne 0 ]]; then # If string NOT found, grep won't return 0
				   			# At this point, "Instruction Retired" field was NOT the only mismatch
				   			# so set ir_only flag to 0 to indicate that instr retired was not the only mismatch found; otherwise, ir_only flag will remain 1 to indicate that instr retired field was the only mismatch found
				   			ir_only=0
				   		fi
				   	else
				   		ir_only=0
				   	fi
			fi
		done
	echo "Reference simulator cycle count: $ref_cycles" >> "$LABN-test.txt"
	echo "Student simulator cycle count: $student_cycles" >> "$LABN-test.txt"
	echo "Number of matching cycles: $matching_cycles" >> "$LABN-test.txt"

	printf "\n" >> "$LABN-test.txt"

done
