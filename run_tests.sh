set -e

python prebuild.py

SRC=$(find src/ -name "*.c")

clang -std=c11 --coverage -O1 -Wfatal-errors -o main src2/main.c $SRC -Iinclude -DPK_ENABLE_OS=1 -DPK_DEBUG_PRECOMPILED_EXEC=1 -DPK_ENABLE_PROFILER=1

python scripts/run_tests.py

# if prev error exit
if [ $? -ne 0 ]; then
    exit 1
fi

rm -rf .coverage
mkdir .coverage
rm pocketpy_c.gcno

UNITS=$(find ./ -name "*.gcno")
llvm-cov-15 gcov ${UNITS} -r -s include/ -r -s src/ >> .coverage/coverage.txt

mv *.gcov .coverage
rm *.gcda
rm *.gcno
