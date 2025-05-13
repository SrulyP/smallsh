CC=gcc-14
CXX=g++-14
CFLAGS=-Wall -std=c11
CXXFLAGS=-fopenmp

# Rule for C files
%: %.c
	$(CC) $(CFLAGS) $< -o $@

# Rule for C++ files
%: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@