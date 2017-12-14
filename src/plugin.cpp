#include <stdio.h>

#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h> // for MAXPATHLEN
#include <fcntl.h>

#include <zip.h>
#include <jansson.h>

#if ARCH_WIN
	#include <windows.h>
	#include <direct.h>
	#define mkdir(_dir, _perms) _mkdir(_dir)
#else
	#include <dlfcn.h>
#endif
#include <dirent.h>

#include "plugin.hpp"
#include "app.hpp"
#include "asset.hpp"
#include "util/request.hpp"


namespace rack {

std::list<Plugin*> gPlugins;
std::string gToken;

std::string gTagNames[NUM_TAGS] = {
	"Amplifier/VCA",
	"Attenuator",
	"Blank",
	"Clock",
	"Controller",
	"Delay",
	"Digital",
	"Distortion",
	"Drum",
	"Dual/Stereo",
	"Dynamics",
	"Effect",
	"Envelope Follower",
	"Envelope Generator",
	"Equalizer",
	"External",
	"Filter/VCF",
	"Function Generator",
	"Granular",
	"LFO",
	"Logic",
	"Low Pass Gate",
	"MIDI",
	"Mixer",
	"Multiple",
	"Noise",
	"Oscillator/VCO",
	"Panning",
	"Quad",
	"Quantizer",
	"Random",
	"Reverb",
	"Ring Modulator",
	"Sample and Hold",
	"Sampler",
	"Sequencer",
	"Slew Limiter",
	"Switch",
	"Synth Voice",
	"Tuner",
	"Utility",
	"Visual",
	"Waveshaper",
};


static bool isDownloading = false;
static float downloadProgress = 0.0;
static std::string downloadName;
static std::string loginStatus;


Plugin::~Plugin() {
	for (Model *model : models) {
		delete model;
	}
}

void Plugin::addModel(Model *model) {
	assert(!model->plugin);
	model->plugin = this;
	models.push_back(model);
}


int loadPlugin(std::string path) {
	std::string libraryFilename;
#if ARCH_LIN
	libraryFilename = path + "/" + "plugin.so";
#elif ARCH_WIN
	libraryFilename = path + "/" + "plugin.dll";
#elif ARCH_MAC
	libraryFilename = path + "/" + "plugin.dylib";
#endif

	// Load dynamic/shared library
#if ARCH_WIN
	SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);
	HINSTANCE handle = LoadLibrary(libraryFilename.c_str());
	SetErrorMode(0);
	if (!handle) {
		int error = GetLastError();
		warn("Failed to load library %s: %d", libraryFilename.c_str(), error);
		return -1;
	}
#elif ARCH_LIN || ARCH_MAC
	void *handle = dlopen(libraryFilename.c_str(), RTLD_NOW);
	if (!handle) {
		warn("Failed to load library %s: %s", libraryFilename.c_str(), dlerror());
		return -1;
	}
#endif

	// Call plugin's init() function
	typedef void (*InitCallback)(Plugin *);
	InitCallback initCallback;
#if ARCH_WIN
	initCallback = (InitCallback) GetProcAddress(handle, "init");
#elif ARCH_LIN || ARCH_MAC
	initCallback = (InitCallback) dlsym(handle, "init");
#endif
	if (!initCallback) {
		warn("Failed to read init() symbol in %s", libraryFilename.c_str());
		return -2;
	}

	// Construct and initialize Plugin instance
	Plugin *plugin = new Plugin();
	plugin->path = path;
	plugin->handle = handle;
	initCallback(plugin);

	// Add plugin to list
	gPlugins.push_back(plugin);
	info("Loaded plugin %s", libraryFilename.c_str());

	return 0;
}

static void loadPlugins(std::string path) {
	DIR *dir = opendir(path.c_str());
	if (dir) {
		struct dirent *d;
		while ((d = readdir(dir))) {
			if (d->d_name[0] == '.')
				continue;
			loadPlugin(path + "/" + d->d_name);
		}
		closedir(dir);
	}
}

////////////////////
// plugin helpers
////////////////////

static int extractZipHandle(zip_t *za, const char *dir) {
	int err = 0;
	for (int i = 0; i < zip_get_num_entries(za, 0); i++) {
		zip_stat_t zs;
		err = zip_stat_index(za, i, 0, &zs);
		if (err)
			return err;
		int nameLen = strlen(zs.name);

		char path[MAXPATHLEN];
		snprintf(path, sizeof(path), "%s/%s", dir, zs.name);

		if (zs.name[nameLen - 1] == '/') {
			err = mkdir(path, 0755);
			if (err && errno != EEXIST)
				return err;
		}
		else {
			zip_file_t *zf = zip_fopen_index(za, i, 0);
			if (!zf)
				return 1;

			FILE *outFile = fopen(path, "wb");
			if (!outFile)
				continue;

			while (1) {
				char buffer[4096];
				int len = zip_fread(zf, buffer, sizeof(buffer));
				if (len <= 0)
					break;
				fwrite(buffer, 1, len, outFile);
			}

			err = zip_fclose(zf);
			if (err)
				return err;
			fclose(outFile);
		}
	}
	return 0;
}

static int extractZip(const char *filename, const char *dir) {
	int err = 0;
	zip_t *za = zip_open(filename, 0, &err);
	if (!za)
		return 1;

	if (!err) {
		err = extractZipHandle(za, dir);
	}

	zip_close(za);
	return err;
}

static void refreshPurchase(json_t *pluginJ) {
	json_t *slugJ = json_object_get(pluginJ, "slug");
	if (!slugJ) return;
	std::string slug = json_string_value(slugJ);

	json_t *nameJ = json_object_get(pluginJ, "name");
	if (!nameJ) return;
	std::string name = json_string_value(nameJ);

	json_t *versionJ = json_object_get(pluginJ, "version");
	if (!versionJ) return;
	std::string version = json_string_value(versionJ);

	// Check whether the plugin is already loaded
	for (Plugin *plugin : gPlugins) {
		if (plugin->slug == slug && plugin->version == version) {
			return;
		}
	}

	// Append token and version to download URL
	std::string url = gApiHost;
	url += "/download";
	url += "?product=";
	url += slug;
	url += "&version=";
	url += requestEscape(gApplicationVersion);
	url += "&token=";
	url += requestEscape(gToken);

	// If plugin is not loaded, download the zip file to /plugins
	downloadName = name;
	downloadProgress = 0.0;

	// Download zip
	std::string pluginsDir = assetLocal("plugins");
	std::string pluginPath = pluginsDir + "/" + slug;
	std::string zipPath = pluginPath + ".zip";
	bool success = requestDownload(url, zipPath, &downloadProgress);
	if (success) {
		// Unzip file
		int err = extractZip(zipPath.c_str(), pluginsDir.c_str());
		if (!err) {
			// Delete zip
			remove(zipPath.c_str());
			// Load plugin
			loadPlugin(pluginPath);
		}
	}

	downloadName = "";
}

////////////////////
// plugin API
////////////////////

void pluginInit() {
	// Load core
	// This function is defined in core.cpp
	Plugin *coreManufacturer = new Plugin();
	init(coreManufacturer);
	gPlugins.push_back(coreManufacturer);

	// Load plugins from global directory
	std::string globalPlugins = assetGlobal("plugins");
	info("Loading plugins from %s", globalPlugins.c_str());
	loadPlugins(globalPlugins);

	// Load plugins from local directory
	std::string localPlugins = assetLocal("plugins");
	if (globalPlugins != localPlugins) {
		mkdir(localPlugins.c_str(), 0755);
		info("Loading plugins from %s", localPlugins.c_str());
		loadPlugins(localPlugins);
	}
}

void pluginDestroy() {
	for (Plugin *plugin : gPlugins) {
		// Free library handle
#if ARCH_WIN
		if (plugin->handle)
			FreeLibrary((HINSTANCE)plugin->handle);
#elif ARCH_LIN || ARCH_MAC
		if (plugin->handle)
			dlclose(plugin->handle);
#endif

		// For some reason this segfaults.
		// It might be best to let them leak anyway, because "crash on exit" issues would occur with badly-written plugins.
		// delete plugin;
	}
	gPlugins.clear();
}

void pluginLogIn(std::string email, std::string password) {
	json_t *reqJ = json_object();
	json_object_set(reqJ, "email", json_string(email.c_str()));
	json_object_set(reqJ, "password", json_string(password.c_str()));
	json_t *resJ = requestJson(METHOD_POST, gApiHost + "/token", reqJ);
	json_decref(reqJ);

	if (resJ) {
		json_t *errorJ = json_object_get(resJ, "error");
		if (errorJ) {
			const char *errorStr = json_string_value(errorJ);
			loginStatus = errorStr;
		}
		else {
			json_t *tokenJ = json_object_get(resJ, "token");
			if (tokenJ) {
				const char *tokenStr = json_string_value(tokenJ);
				gToken = tokenStr;
				loginStatus = "";
			}
		}
		json_decref(resJ);
	}
}

void pluginLogOut() {
	gToken = "";
}

void pluginRefresh() {
	if (gToken.empty())
		return;

	isDownloading = true;
	downloadProgress = 0.0;
	downloadName = "";

	json_t *reqJ = json_object();
	json_object_set(reqJ, "version", json_string(gApplicationVersion.c_str()));
	json_object_set(reqJ, "token", json_string(gToken.c_str()));
	json_t *resJ = requestJson(METHOD_GET, gApiHost + "/purchases", reqJ);
	json_decref(reqJ);

	if (resJ) {
		json_t *errorJ = json_object_get(resJ, "error");
		if (errorJ) {
			const char *errorStr = json_string_value(errorJ);
			warn("Plugin refresh error: %s", errorStr);
		}
		else {
			json_t *purchasesJ = json_object_get(resJ, "purchases");
			size_t index;
			json_t *purchaseJ;
			json_array_foreach(purchasesJ, index, purchaseJ) {
				refreshPurchase(purchaseJ);
			}
		}
		json_decref(resJ);
	}

	isDownloading = false;
}

void pluginCancelDownload() {
	// TODO
}

bool pluginIsLoggedIn() {
	return gToken != "";
}

bool pluginIsDownloading() {
	return isDownloading;
}

float pluginGetDownloadProgress() {
	return downloadProgress;
}

std::string pluginGetDownloadName() {
	return downloadName;
}

std::string pluginGetLoginStatus() {
	return loginStatus;
}


} // namespace rack
