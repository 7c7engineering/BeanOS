# Format all C/C++ files in current directory and subdirectories
git ls-files "*.cpp" "*.hpp" "*.h" "*.c" | xargs clang-format -i
