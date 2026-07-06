#include "window.hpp"
#include "layout.hpp"
#include "css.hpp"

#include <gtk4-layer-shell.h>
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <filesystem>
#include <glibmm/main.h>
#include <linux/input.h>
#include <signal.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>

namespace {
bool truthy(const std::string& value) {
	std::string lowered = value;
	std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return !(lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off");
}

std::string read_file(const std::filesystem::path& path) {
	std::ifstream file(path);
	std::string value;
	std::getline(file, value);
	return value;
}

bool haptics_candidate(const std::filesystem::path& event_path) {
	const auto name_path = event_path / "device/name";
	const auto ff_path = event_path / "device/capabilities/ff";
	std::string name = read_file(name_path);
	std::string ff = read_file(ff_path);
	std::string lowered = name;
	std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});

	return lowered.find("haptic") != std::string::npos ||
	       lowered.find("vibrator") != std::string::npos ||
	       (!ff.empty() && ff != "0" && ff != "none" && ff != "0000000000000000");
}

int parse_int_config(const std::string& value, int fallback, int minimum, int maximum) {
	char* end = nullptr;
	errno = 0;
	long parsed = std::strtol(value.c_str(), &end, 0);
	if (errno != 0 || end == value.c_str())
		return fallback;
	return std::clamp(static_cast<int>(parsed), minimum, maximum);
}

unsigned short parse_u16_config(const std::string& value, unsigned short fallback) {
	char* end = nullptr;
	errno = 0;
	unsigned long parsed = std::strtoul(value.c_str(), &end, 0);
	if (errno != 0 || end == value.c_str())
		return fallback;
	return static_cast<unsigned short>(std::clamp(parsed, 0UL, 0xffffUL));
}
}

sysboard::sysboard(const std::map<std::string, std::map<std::string, std::string>>& cfg) {
	config_main = cfg;

	// Layer shell stuff
	gtk_layer_init_for_window(gobj());
	gtk_layer_set_namespace(gobj(), "sysboard");
	gtk_layer_set_layer(gobj(), GTK_LAYER_SHELL_LAYER_OVERLAY);
	gtk_layer_auto_exclusive_zone_enable(gobj());

	gtk_layer_set_anchor(gobj(), GTK_LAYER_SHELL_EDGE_BOTTOM, true);
	gtk_layer_set_anchor(gobj(), GTK_LAYER_SHELL_EDGE_LEFT, true);
	gtk_layer_set_anchor(gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, true);

	// Initialization
	set_name("sysboard");
	set_default_size(-1, stoi(config_main["main"]["height"]));
	initialize_protos();
	load_layout();

	// Load custom css
	std::string style_path;
	if (std::filesystem::exists(std::string(getenv("HOME")) + "/.config/sys64/board/style.css"))
		style_path = std::string(getenv("HOME")) + "/.config/sys64/board/style.css";
	else if (std::filesystem::exists("/usr/share/sys64/board/style.css"))
		style_path = "/usr/share/sys64/board/style.css";
	else
		style_path = "/usr/local/share/sys64/board/style.css";
	css_loader css(style_path, this);
}

bool sysboard::initialize_haptics() {
	haptics_initialized = true;

	auto haptics = config_main["main"].find("haptics");
	if (haptics != config_main["main"].end() && !truthy(haptics->second))
		return false;

	std::vector<std::filesystem::path> candidates;
	const char* explicit_device = getenv("SYSBOARD_HAPTICS_DEVICE");
	if (explicit_device != nullptr && explicit_device[0] != '\0')
		candidates.emplace_back(explicit_device);

	for (const auto& entry : std::filesystem::directory_iterator("/sys/class/input")) {
		std::string event_name = entry.path().filename().string();
		if (event_name.rfind("event", 0) != 0)
			continue;
		if (haptics_candidate(entry.path()))
			candidates.emplace_back("/dev/input/" + event_name);
	}

	for (const auto& candidate : candidates) {
		haptics_fd = open(candidate.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
		if (haptics_fd < 0)
			continue;

		ff_effect effect {};
		effect.type = FF_RUMBLE;
		effect.id = -1;
		effect.u.rumble.strong_magnitude = 0x2200;
		effect.u.rumble.weak_magnitude = 0x2200;
		effect.replay.length = 18;

		auto duration = config_main["main"].find("haptics-duration-ms");
		if (duration != config_main["main"].end()) {
			effect.replay.length = parse_int_config(duration->second, effect.replay.length, 5, 80);
		}

		auto strength = config_main["main"].find("haptics-strength");
		if (strength != config_main["main"].end()) {
			auto parsed = parse_u16_config(strength->second, effect.u.rumble.strong_magnitude);
			effect.u.rumble.strong_magnitude = parsed;
			effect.u.rumble.weak_magnitude = parsed;
		}

		if (ioctl(haptics_fd, EVIOCSFF, &effect) == 0) {
			haptics_effect_id = effect.id;
			return true;
		}

		::close(haptics_fd);
		haptics_fd = -1;
	}

	return false;
}

void sysboard::play_haptic() {
	if (!haptics_initialized && !initialize_haptics())
		return;
	if (haptics_fd < 0 || haptics_effect_id < 0)
		return;

	struct timeval tv {};
	gettimeofday(&tv, nullptr);
	long now = tv.tv_sec * 1000000 + tv.tv_usec;
	if (now - last_haptic_time < 20000)
		return;
	last_haptic_time = now;

	input_event event {};
	event.type = EV_FF;
	event.code = haptics_effect_id;
	event.value = 1;
	(void)write(haptics_fd, &event, sizeof(event));
}

void sysboard::load_layout() {
	layout_board = Gtk::make_managed<layout>(this, config_main["main"]["layout"]);
	set_child(*layout_board);
	layout_board->set_margin(stoi(config_main["main"]["margin"]));
}

void sysboard::handle_signal(const int &signum, const bool& manual) {
	// Timeout exists to prevent a ping pong effect
	// Currently it's set to 250ms altho 100ms also works well enough
	timeout_connection.disconnect();
	timeout_connection = Glib::signal_timeout().connect([&, signum, manual]() {
		Glib::signal_idle().connect([&, signum, manual]() {

			// Reset all active modifiers to prevent weird behavior
			set_modifier(0);

			if (signum == SIGUSR1) { // Show
				show();
			}

			else if (signum == SIGUSR2) { // Hide
				layout_board->handle_keycode(nullptr, false);
				hide();
			}

			else if (signum == SIGRTMIN) { // Toggle
				set_visible(!manual_mode);
			}

			if (manual) {
				manual_mode = get_visible();
			}

			return false;
		});
		return false;
	}, 250);
}

extern "C" {
	sysboard* sysboard_create(const std::map<std::string, std::map<std::string, std::string>>& cfg) {
		return new sysboard(cfg);
	}
	void sysboard_signal(sysboard* window, int signal) {
		window->handle_signal(signal, true);
	}
}
