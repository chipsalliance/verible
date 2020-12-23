
if [ $1 == "linter" ]; then
	# Second arg: file_name
	# Third arg: rules
	~/bin/verible-verilog-lint --rules=$3 verilog/tools/lint/testdata/$2.sv
fi

if [ $1 == "syntax" ]; then
	# Second arg: file_name
	~/bin/verible-verilog-syntax --printtree verilog/tools/lint/testdata/$2.sv
fi

if [ $1 == "build" ]; then
	bazel run -c opt :install -- ~/bin
fi



