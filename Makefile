FLAGS += \
	-Iinclude \
	-Idep/include -Idep/lib/libzip/include

ALL_SOURCES = $(wildcard src/*.cpp src/*/*.cpp) \
	 	 	ext/nanovg/src/nanovg.c

MAIN_APP := src/main.cpp
TEST_APP := tests/doctest.cpp

SOURCES := $(filter-out $(MAIN_APP) $(TEST_APP), $(ALL_SOURCES))

include arch.mk

ifeq ($(ARCH), lin)
	SOURCES += ext/osdialog/osdialog_gtk2.c
	CFLAGS += $(shell pkg-config --cflags gtk+-2.0)
	LDFLAGS += -rdynamic \
		-lpthread -lGL -ldl \
		$(shell pkg-config --libs gtk+-2.0) \
		-Ldep/lib -lGLEW -lglfw -ljansson -lsamplerate -lcurl -lzip -lrtaudio -lrtmidi
	TARGET = Rack
endif

ifeq ($(ARCH), mac)
	SOURCES += ext/osdialog/osdialog_mac.m
	CXXFLAGS += -DAPPLE -stdlib=libc++
	LDFLAGS += -stdlib=libc++ -lpthread -ldl \
		-framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo \
		-Ldep/lib -lGLEW -lglfw -ljansson -lsamplerate -lcurl -lzip -lrtaudio -lrtmidi
	TARGET = Rack
	BUNDLE = dist/$(TARGET).app
endif

ifeq ($(ARCH), win)
	SOURCES += ext/osdialog/osdialog_win.c
	LDFLAGS += -static-libgcc -static-libstdc++ -lpthread \
		-Wl,--export-all-symbols,--out-implib,libRack.a -mwindows \
		-lgdi32 -lopengl32 -lcomdlg32 -lole32 \
		-Ldep/lib -lglew32 -lglfw3dll -lcurl -lzip -lrtaudio -lrtmidi \
		-Wl,-Bstatic -ljansson -lsamplerate
	TARGET = Rack.exe
	OBJECTS = Rack.res
endif


all: $(MAIN_APP) $(TARGET)
doctest: doctest $(TEST_APP) $(TARGET)

dep:
	$(MAKE) -C dep

run: $(TARGET)
ifeq ($(ARCH), lin)
	LD_LIBRARY_PATH=dep/lib ./$<
endif
ifeq ($(ARCH), mac)
	DYLD_FALLBACK_LIBRARY_PATH=dep/lib ./$<
endif
ifeq ($(ARCH), win)
	# TODO get rid of the mingw64 path
	env PATH=dep/bin:/mingw64/bin ./$<
endif

debug: $(TARGET)
ifeq ($(ARCH), lin)
	LD_LIBRARY_PATH=dep/lib gdb -ex run ./Rack
endif
ifeq ($(ARCH), mac)
	DYLD_FALLBACK_LIBRARY_PATH=dep/lib gdb -ex run ./Rack
endif
ifeq ($(ARCH), win)
	# TODO get rid of the mingw64 path
	env PATH=dep/bin:/mingw64/bin gdb -ex run ./Rack
endif

clean:
	rm -rfv $(TARGET) build dist

# For Windows resources
%.res: %.rc
	windres $^ -O coff -o $@

include compile.mk


dist: all
ifndef VERSION
	$(error VERSION must be defined when making distributables)
endif
	rm -rf dist
	$(MAKE) -C plugins/Fundamental dist

ifeq ($(ARCH), mac)
	mkdir -p $(BUNDLE)
	mkdir -p $(BUNDLE)/Contents
	mkdir -p $(BUNDLE)/Contents/Resources
	cp Info.plist $(BUNDLE)/Contents/
	cp -R LICENSE* res $(BUNDLE)/Contents/Resources

	mkdir -p $(BUNDLE)/Contents/MacOS
	cp Rack $(BUNDLE)/Contents/MacOS/
	cp icon.icns $(BUNDLE)/Contents/Resources/

	otool -L $(BUNDLE)/Contents/MacOS/Rack

	cp dep/lib/libGLEW.2.1.0.dylib $(BUNDLE)/Contents/MacOS/
	cp dep/lib/libglfw.3.dylib $(BUNDLE)/Contents/MacOS/
	cp dep/lib/libjansson.4.dylib $(BUNDLE)/Contents/MacOS/
	cp dep/lib/libsamplerate.0.dylib $(BUNDLE)/Contents/MacOS/
	cp dep/lib/libcurl.4.dylib $(BUNDLE)/Contents/MacOS/
	cp dep/lib/libzip.5.dylib $(BUNDLE)/Contents/MacOS/
	cp dep/lib/libportaudio.2.dylib $(BUNDLE)/Contents/MacOS/
	cp dep/lib/librtmidi.4.dylib $(BUNDLE)/Contents/MacOS/
	cp dep/lib/librtaudio.6.dylib $(BUNDLE)/Contents/MacOS/

	install_name_tool -change /usr/local/lib/libGLEW.2.1.0.dylib @executable_path/libGLEW.2.1.0.dylib $(BUNDLE)/Contents/MacOS/Rack
	install_name_tool -change lib/libglfw.3.dylib @executable_path/libglfw.3.dylib $(BUNDLE)/Contents/MacOS/Rack
	install_name_tool -change $(PWD)/dep/lib/libjansson.4.dylib @executable_path/libjansson.4.dylib $(BUNDLE)/Contents/MacOS/Rack
	install_name_tool -change $(PWD)/dep/lib/libsamplerate.0.dylib @executable_path/libsamplerate.0.dylib $(BUNDLE)/Contents/MacOS/Rack
	install_name_tool -change $(PWD)/dep/lib/libcurl.4.dylib @executable_path/libcurl.4.dylib $(BUNDLE)/Contents/MacOS/Rack
	install_name_tool -change $(PWD)/dep/lib/libzip.5.dylib @executable_path/libzip.5.dylib $(BUNDLE)/Contents/MacOS/Rack
	install_name_tool -change $(PWD)/dep/lib/libportaudio.2.dylib @executable_path/libportaudio.2.dylib $(BUNDLE)/Contents/MacOS/Rack
	install_name_tool -change $(PWD)/dep/lib/librtmidi.4.dylib @executable_path/librtmidi.4.dylib $(BUNDLE)/Contents/MacOS/Rack
	install_name_tool -change $(PWD)/dep/lib/librtaudio.6.dylib @executable_path/librtaudio.6.dylib $(BUNDLE)/Contents/MacOS/Rack

	otool -L $(BUNDLE)/Contents/MacOS/Rack

	mkdir -p $(BUNDLE)/Contents/Resources/plugins
	cp -R plugins/Fundamental/dist/Fundamental $(BUNDLE)/Contents/Resources/plugins
	# Make DMG image
	cd dist && ln -s /Applications Applications
	cd dist && hdiutil create -srcfolder . -volname Rack -ov -format UDZO Rack-$(VERSION)-$(ARCH).dmg
endif
ifeq ($(ARCH), win)
	mkdir -p dist/Rack
	cp -R LICENSE* res dist/Rack/
	cp Rack.exe dist/Rack/
	strip dist/Rack/Rack.exe
	cp /mingw64/bin/libwinpthread-1.dll dist/Rack/
	cp /mingw64/bin/zlib1.dll dist/Rack/
	cp /mingw64/bin/libstdc++-6.dll dist/Rack/
	cp /mingw64/bin/libgcc_s_seh-1.dll dist/Rack/
	cp dep/bin/glew32.dll dist/Rack/
	cp dep/bin/glfw3.dll dist/Rack/
	cp dep/bin/libcurl-4.dll dist/Rack/
	cp dep/bin/libjansson-4.dll dist/Rack/
	cp dep/bin/librtmidi-4.dll dist/Rack/
	cp dep/bin/libsamplerate-0.dll dist/Rack/
	cp dep/bin/libzip-5.dll dist/Rack/
	cp dep/bin/librtaudio.dll dist/Rack/
	mkdir -p dist/Rack/plugins
	cp -R plugins/Fundamental/dist/Fundamental dist/Rack/plugins/
	# Make ZIP
	cd dist && zip -5 -r Rack-$(VERSION)-$(ARCH).zip Rack
	# Make NSIS installer
	makensis installer.nsi
	mv Rack-setup.exe dist/Rack-$(VERSION)-$(ARCH).exe
endif
ifeq ($(ARCH), lin)
	mkdir -p dist/Rack
	cp -R LICENSE* res dist/Rack/
	cp Rack Rack.sh dist/Rack/
	cp dep/lib/libcurl.so.4 dist/Rack/
	cp dep/lib/libzip.so.5 dist/Rack/
	mkdir -p dist/Rack/plugins
	cp -R plugins/Fundamental/dist/Fundamental dist/Rack/plugins/
	# Make ZIP
	cd dist && zip -5 -r Rack-$(VERSION)-$(ARCH).zip Rack
endif


# Obviously this will only work if you have the private keys to my server
UPLOAD_URL = vortico@vcvrack.com:files/
upload: dist distplugins
ifeq ($(ARCH), mac)
	rsync dist/*.dmg $(UPLOAD_URL) -zP
endif
ifeq ($(ARCH), win)
	rsync dist/*.exe $(UPLOAD_URL) -P
	rsync dist/*.zip $(UPLOAD_URL) -P
endif
ifeq ($(ARCH), lin)
	rsync dist/*.zip $(UPLOAD_URL) -zP
endif
	rsync plugins/*/dist/*.zip $(UPLOAD_URL) -zP


# Plugin helpers

allplugins:
	for f in plugins/*; do $(MAKE) -C "$$f"; done

cleanplugins:
	for f in plugins/*; do $(MAKE) -C "$$f" clean; done

distplugins:
	for f in plugins/*; do $(MAKE) -C "$$f" dist; done

plugins:
	for f in plugins/*; do (cd "$$f" && ${CMD}); done

.PHONY: all dep run debug clean dist allplugins cleanplugins distplugins plugins
