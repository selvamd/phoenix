g++ -o Tables.o -c -g Tables.cpp

ar rc Utils.a Tables.o
ranlib Utils.a

g++ -o test.o -c -g test.cpp

g++ -o test.exe test.o Utils.a  -lm 

del *.o *.a