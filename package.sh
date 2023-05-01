g++ main.c -o block_cycle -lraylib
x86_64-w64-mingw32-gcc main.c -o block_cycle.exe -lraylib
cp builds/win/*.dll ./
zip -r Block-Cycle.zip .
rm *.dll