TEST_SRC=$(wildcard test_*.cpp)
TEST_BIN=$(TEST_SRC:%.cpp=%.o)

CXXFLAGS=-std=c++0x -Wall -Wextra -Werror

ifeq ($(shell uname),Darwin)
$(TEST_BIN): LDFLAGS+=-framework OpenGL
else
$(TEST_BIN): LDFLAGS+=-lrt -lGL
endif

all: ../microprofilehtml.h

test: $(TEST_BIN)

$(TEST_BIN): %.o: %.cpp
	$(CXX) $< $(CXXFLAGS) $(LDFLAGS) -I.. -o $@
	./$@

embed.o: embed.c
	$(CC) embed.c -o embed.o

../microprofilehtml.h: embed.o microprofile.html
	./embed.o $@ microprofile.html ____embed____ g_MicroProfileHtml MICROPROFILE_EMBED_HTML

.INTERMEDIATE: embed.o $(TEST_BIN)
.PHONY: all test