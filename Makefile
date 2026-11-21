CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wpedantic -Isrc
CORE = src/core/rocket.cpp src/core/motor.cpp src/core/flight.cpp src/core/telemetry.cpp src/core/fin_inspection.cpp src/core/mesh.cpp src/core/bench_profile.cpp src/core/bench_link.cpp
UI = src/ui/canvas.cpp src/ui/rocket_view.cpp src/ui/app.cpp src/ui/app_draw.cpp
HEADERS = $(wildcard src/core/*.hpp src/ui/*.hpp)
# Pass tag names as environment values, never splice them into a shell command.
ifneq ($(strip $(BASE)),)
export RELEASE_BASE := $(BASE)
endif
ifneq ($(strip $(TAG)),)
export RELEASE_TAG := $(TAG)
endif
.PHONY: all test preview ps4 video teaser release-media release-check clean
all: out/eulolab-host

out:
	mkdir -p out

out/eulolab-host: $(CORE) $(UI) $(HEADERS) src/platform/host.cpp assets/font-atlas.bin assets/motors/Klima_C6.eng assets/motors/Klima_D9.eng | out
	$(CXX) $(CXXFLAGS) $(CORE) $(UI) src/platform/host.cpp -o $@

out/core-tests: $(CORE) $(HEADERS) tests/core_tests.cpp | out
	$(CXX) $(CXXFLAGS) $(CORE) tests/core_tests.cpp -o $@

out/ui-tests: $(CORE) $(UI) $(HEADERS) tests/ui_tests.cpp assets/font-atlas.bin | out
	$(CXX) $(CXXFLAGS) $(CORE) $(UI) tests/ui_tests.cpp -o $@

out/bench-tests: src/core/bench_link.cpp src/core/bench_board.cpp src/core/bench_profile.cpp $(wildcard src/core/bench_*.hpp) tests/bench_tests.cpp | out
	$(CXX) $(CXXFLAGS) src/core/bench_link.cpp src/core/bench_board.cpp src/core/bench_profile.cpp tests/bench_tests.cpp -o $@

out/stm32-port-tests: firmware/stm32/bench_port.cpp firmware/stm32/bench_port.hpp src/core/bench_board.cpp $(wildcard src/core/bench_*.hpp) tests/stm32_port_tests.cpp | out
	$(CXX) $(CXXFLAGS) -I. firmware/stm32/bench_port.cpp src/core/bench_board.cpp tests/stm32_port_tests.cpp -o $@

out/arduino-radio-tests: firmware/arduino/EuloRecorder/radio_packet.hpp tests/arduino_radio_tests.cpp | out
	$(CXX) $(CXXFLAGS) -I. tests/arduino_radio_tests.cpp -o $@

test: out/eulolab-host out/core-tests out/ui-tests out/bench-tests out/stm32-port-tests out/arduino-radio-tests
	./out/core-tests
	./out/ui-tests
	./out/bench-tests
	./out/stm32-port-tests
	./out/arduino-radio-tests
	python3 -m unittest discover -s tests -p '*_test.py'

preview: out/eulolab-host
	python3 tools/host_preview.py --port $(if $(PORT),$(PORT),8000)

ps4:
	$(MAKE) -C ps4 pkg

video: out/eulolab-host
	python3 tools/make_video.py

# 30-second walkthrough of current native host states; never hardware footage.
teaser: out/eulolab-host
	python3 tools/make_teaser.py

# Run on every release candidate after a human rewrite of README.md.
# Media dependencies: .venv/bin/pip install -r requirements-media.txt
release-media: out/eulolab-host
	python3 tools/make_video.py
	python3 tools/make_teaser.py
	python3 tools/release_gate.py --write

# BASE=<previous release tag> TAG=<planned tag> require a fresh README and
# completed per-tag reviewer record. Without TAG this is automated QA only.
release-check: test
	python3 tools/release_gate.py --check

clean:
	rm -rf out
