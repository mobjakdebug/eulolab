CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wpedantic -Isrc
CORE = src/core/rocket.cpp src/core/motor.cpp src/core/flight.cpp
UI = src/ui/canvas.cpp src/ui/rocket_view.cpp src/ui/app.cpp
HEADERS = $(wildcard src/core/*.hpp src/ui/*.hpp)
.PHONY: all test ps4 video clean
all: out/eulolab-host

out:
	mkdir -p out

out/eulolab-host: $(CORE) $(UI) $(HEADERS) src/platform/host.cpp assets/font-atlas.bin assets/motors/Klima_C6.eng assets/motors/Klima_D9.eng | out
	$(CXX) $(CXXFLAGS) $(CORE) $(UI) src/platform/host.cpp -o $@

out/core-tests: $(CORE) $(HEADERS) tests/core_tests.cpp | out
	$(CXX) $(CXXFLAGS) $(CORE) tests/core_tests.cpp -o $@

out/ui-tests: $(CORE) $(UI) $(HEADERS) tests/ui_tests.cpp assets/font-atlas.bin | out
	$(CXX) $(CXXFLAGS) $(CORE) $(UI) tests/ui_tests.cpp -o $@

test: out/core-tests out/ui-tests
	./out/core-tests
	./out/ui-tests
	python3 -m unittest discover -s tests -p '*_test.py'

ps4:
	$(MAKE) -C ps4 pkg

video: out/eulolab-host
	python3 tools/make_video.py

clean:
	rm -rf out
