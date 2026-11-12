# RISC201 build file  (Team: Jiya, Chanchal, Rani, Kavya)
CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Isrc
SRC      = $(wildcard src/*.cpp)
OBJ      = $(SRC:.cpp=.o)

risc201: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f src/*.o risc201
