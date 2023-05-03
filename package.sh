# This shell script automates the process of compiling and compressing the project

# Compile natively (linux)
g++ main.c -o block_cycle -lraylib -Werror || exit

# Cross-compile for windows
x86_64-w64-mingw32-gcc main.c -o block_cycle.exe -Werror -lraylib || exit

# Copy in all of the required dynamic libraries
cp lib/win/*.dll ./

# Finally, zip and cleanup
zip -r Block-Cycle.zip .
rm *.dll