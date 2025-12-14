# Else exist specifically for clang
ifeq ($(CXX),g++)
    EXTRA_FLAGS = --no-gnu-unique
else
    EXTRA_FLAGS =
endif

PLUGIN_NAME=hyprvibr
OUTPUT=out/$(PLUGIN_NAME).so

all: $(OUTPUT)

$(OUTPUT): main.cpp globals.hpp
	@mkdir -p out
	$(CXX) -shared -fPIC $(EXTRA_FLAGS) main.cpp -o $(OUTPUT) -g `pkg-config --cflags pixman-1 libdrm hyprland pangocairo libinput libudev wayland-server xkbcommon | sed 's#-I\([^ ]*/hyprland\)\($$\| \)#-I\1 -I\1/src #g'` -std=c++2b -O2

clean:
	rm -rf out
