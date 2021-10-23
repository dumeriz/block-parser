sources=`find src -name "*.cpp"`
headers=`find include -name "*.hpp"`
files=("$sources $headers")

for file in $files; do clang-format -i $file; done
