g++ src/*.cpp -o "Blockbit.exe" ^
-std=c++20 ^
-O2 ^
-mwindows ^
-D_WIN32_WINNT=0x0601 ^
-lsfml-graphics ^
-lsfml-window ^
-lsfml-system ^
-lsfml-audio ^
-lws2_32 ^
-lmswsock

pause