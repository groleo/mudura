all:
	$(CXX) -Wall -fPIC -c pcprofile.cpp -o libfinstr.o
	$(CXX) -finstrument-functions -O0 -g test-lib.cpp -shared -fPIC libfinstr.o -L$(PWD) -Wl,-rpath=$(PWD) -o libtest.so -ldl
	$(CXX) -O0 -g test-app.cpp -o test-app -ltest -L$(PWD) -Wl,-rpath=$(PWD)

clean:
	rm -rf libfinstr.so
	rm -rf test-app libtest.so
	rm -rf *.o
