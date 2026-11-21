CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wpedantic -Isrc
CORE = src/core/rocket.cpp src/core/motor.cpp src/core/flight.cpp src/core/telemetry.cpp src/core/mesh.cpp src/core/bench_link.cpp
UI = src/ui/canvas.cpp src/ui/rocket_view.cpp src/ui/app.cpp src/ui/app_draw.cpp
HEADERS = $(wildcard src/core/*.hpp src/ui/*.hpp)
.PHONY: all test preview ps4 video clean
all: out/eulolab-host

out:
	mkdir -p out

out/eulolab-host: $(CORE) $(UI) $(HEADERS) src/platform/host.cpp assets/font-atlas.bin assets/motors/Klima_C6.eng assets/motors/Klima_D9.eng | out
	$(CXX) $(CXXFLAGS) $(CORE) $(UI) src/platform/host.cpp -o $@

out/core-tests: $(CORE) $(HEADERS) tests/core_tests.cpp | out
	$(CXX) $(CXXFLAGS) $(CORE) tests/core_tests.cpp -o $@

out/ui-tests: $(CORE) $(UI) $(HEADERS) tests/ui_tests.cpp assets/font-atlas.bin | out
	$(CXX) $(CXXFLAGS) $(CORE) $(UI) tests/ui_tests.cpp -o $@

out/bench-tests: src/core/bench_link.cpp src/core/bench_board.cpp src/core/bench_link.hpp src/core/bench_board.hpp src/core/bench_wire.hpp src/core/bench_usb_descriptors.hpp tests/bench_tests.cpp | out
	$(CXX) $(CXXFLAGS) src/core/bench_link.cpp src/core/bench_board.cpp tests/bench_tests.cpp -o $@

test: out/eulolab-host out/core-tests out/ui-tests out/bench-tests
	./out/core-tests
	./out/ui-tests
	./out/bench-tests
	python3 -m unittest discover -s tests -p '*_test.py'

preview: out/eulolab-host
	python3 tools/host_preview.py --port $(if $(PORT),$(PORT),8000)

ps4:
	$(MAKE) -C ps4 pkg

video: out/eulolab-host
	python3 tools/make_video.py

clean:
	rm -rf out
