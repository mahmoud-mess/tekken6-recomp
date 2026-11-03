#pragma once

#include <memory>
#include <filesystem>

#include <rex/rex_app.h>
#include <rex/logging.h>
#include <rex/ui/presenter.h>
#include <rex/system/interfaces/graphics.h>
#include <rex/ppc/func.h>
#include <rex/memory/utils.h>
#include <rex/filesystem/devices/host_path_device.h>

#if defined(_WIN32)
void InstallTekken6CrashReport();
#endif

class Tekken6App final : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Tekken6App>(new Tekken6App(ctx, "tekken6",
                                                        PPCImageConfig));
  }

 protected:
  void OnLoadXexImage(std::string& xex_image) override {
    // The SDK preflights this path on the host before its case-insensitive
    // guest filesystem is used. Retail discs name the file Default.xex.
    if (!std::filesystem::is_regular_file(game_data_root() / "default.xex") &&
        std::filesystem::is_regular_file(game_data_root() / "Default.xex")) {
      xex_image = "game:\\Default.xex";
    }
  }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    rex::ReXApp::OnCreateDialogs(drawer);
    SetGuestFrameStats([this] {
      const auto* app_runtime = runtime();
      const auto* graphics = app_runtime ? app_runtime->graphics_system() : nullptr;
      if (!graphics) {
        return rex::ui::FrameStats{};
      }
      const auto stats = graphics->guest_frame_stats();
      return rex::ui::FrameStats{stats.frame_time_ms, stats.fps, stats.frame_count};
    });
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
#if defined(_WIN32)
    InstallTekken6CrashReport();
#endif
    // Select the Xenos runtime plugin for guest rendering.
    if (config.gpu_plugin.empty()) {
      config.gpu_plugin = "xenos";
    }
    // Tekken's Cache1 bootstrap checks sectors_per_allocation_unit *
    // bytes_per_sector == 0x8000.  Keep the SDK/Xenia default (0x10000) for
    // other titles and scope this compatibility geometry to Tekken.
    config.null_device_sectors_per_allocation_unit = 0x40;
  }

  void OnPostLoadXexImage() override {
    // These retail entries are vtable tail-call thunks that XenonRecomp did
    // not emit as function boundaries. Their verified four-instruction
    // sequences load the object's vtable slot into CTR and branch through it.
    // Install them before guest startup dispatches any of them.
    InstallVirtualCallThunk<0x82248968, 0x90>();
    InstallVirtualCallThunk<0x82248988, 0x12C>();
    InstallVirtualCallThunk<0x82248998, 0xB0>();
    InstallVirtualCallThunk<0x822489A8, 0xC0>();
    InstallVirtualCallThunk<0x822489B8, 0xE0>();
    InstallVirtualCallThunk<0x822489C8, 0x120>();
    InstallVirtualCallThunk<0x822489D8, 0xB4>();
    InstallVirtualCallThunk<0x821DA618, 0x130>();
    InstallVirtualCallThunk<0x822249A0, 0x104>();
    InstallVirtualCallThunk<0x822249B0, 0x114>();
    InstallVirtualCallThunk<0x822249C0, 0xE8>();
    InstallVirtualCallThunk<0x822249D0, 0x108>();
    InstallVirtualCallThunk<0x822249E0, 0x118>();
    InstallVirtualCallThunk<0x82224A08, 0xFC>();
    InstallVirtualCallThunk<0x82224A18, 0x10C>();
    InstallVirtualCallThunk<0x82224A28, 0xF0>();
    InstallVirtualCallThunk<0x82224A38, 0x100>();
    InstallVirtualCallThunk<0x82224A48, 0x110>();
    InstallVirtualCallThunk<0x822488A8, 0xF4>();
    InstallVirtualCallThunk<0x822488B8, 0x88>();
    InstallVirtualCallThunk<0x822488C8, 0x124>();
    InstallVirtualCallThunk<0x822488D8, 0xB8>();
    InstallVirtualCallThunk<0x822488E8, 0xC8>();
    InstallVirtualCallThunk<0x822488F8, 0xD8>();
    InstallVirtualCallThunk<0x82248908, 0xF8>();
    InstallVirtualCallThunk<0x82248918, 0x128>();
    InstallVirtualCallThunk<0x82248928, 0xBC>();
    InstallVirtualCallThunk<0x82248938, 0xCC>();
    InstallVirtualCallThunk<0x82248948, 0xDC>();
    InstallVirtualCallThunk<0x82248958, 0x80>();
    InstallVirtualCallThunk<0x82411178, 0x1A0>();
    InstallVirtualCallThunk<0x82411188, 0x1D0>();
    InstallVirtualCallThunk<0x82411198, 0x164>();
    InstallVirtualCallThunk<0x824111A8, 0x174>();
    InstallVirtualCallThunk<0x824111B8, 0x184>();
    InstallVirtualCallThunk<0x824111F8, 0x168>();
    InstallVirtualCallThunk<0x82411208, 0x188>();
    InstallVirtualCallThunk<0x82411218, 0x198>();
    InstallVirtualCallThunk<0x82411228, 0x1A8>();
    InstallVirtualCallThunk<0x82411238, 0x1D8>();
    InstallVirtualCallThunk<0x82411248, 0x15C>();
    InstallVirtualCallThunk<0x82411278, 0x17C>();
    InstallVirtualCallThunk<0x82411288, 0x18C>();
    InstallVirtualCallThunk<0x82411298, 0x19C>();
    InstallVirtualCallThunk<0x8253E4D0, 0x24, 11, 10>();

    // Additional exact-pattern tail-call thunks found in the full image scan.
    InstallVirtualCallThunk<0x82218538, 0x4, 7, 6>();
    InstallVirtualCallThunk<0x821EC970, 0x4, 11, 10>();
    InstallVirtualCallThunk<0x821ECAD4, 0x4, 11, 10>();
    InstallVirtualCallThunk<0x8220B2A8, 0x28, 11, 10>();
    InstallVirtualCallThunk<0x82248898, 0xE4>();
    InstallVirtualCallThunk<0x82248978, 0x11C>();
    InstallVirtualCallThunk<0x8224A0A4, 0x8, 11, 10>();
    InstallVirtualCallThunk<0x8234EED0, 0x60, 11, 10>();
    InstallVirtualCallThunk<0x82411138, 0x160>();
    InstallVirtualCallThunk<0x82411148, 0x278>();
    InstallVirtualCallThunk<0x82411158, 0x180>();
    InstallVirtualCallThunk<0x82411168, 0x190>();
    InstallVirtualCallThunk<0x824111C8, 0x194>();
    InstallVirtualCallThunk<0x824111D8, 0x1A4>();
    InstallVirtualCallThunk<0x824111E8, 0x1D4>();
    InstallVirtualCallThunk<0x82411258, 0x16C>();
    InstallVirtualCallThunk<0x82411268, 0x274>();
    InstallVirtualCallThunk<0x82459E80, 0x44, 11, 10>();
    InstallVirtualCallThunk<0x82459E98, 0x44, 11, 10>();
    InstallVirtualCallThunk<0x82459EB0, 0x44, 11, 10>();
    InstallVirtualCallThunk<0x82459EC8, 0x44, 11, 10>();
    InstallVirtualCallThunk<0x82459EE0, 0x44, 11, 10>();
    InstallVirtualCallThunk<0x82459F30, 0x4C, 11, 10>();
    InstallVirtualCallThunk<0x82459F48, 0x4C, 11, 10>();
    InstallVirtualCallThunk<0x82459F60, 0x4C, 11, 10>();
    InstallVirtualCallThunk<0x82459F78, 0x4C, 11, 10>();
    InstallVirtualCallThunk<0x82459F90, 0x4C, 11, 10>();
    InstallVirtualCallThunk<0x8249D4C4, 0x28, 11, 10>();
    InstallVirtualCallThunk<0x824A8394, 0x38, 11, 10>();
    InstallVirtualCallThunk<0x824A83AC, 0x3C, 11, 10>();
    InstallVirtualCallThunk<0x824A8518, 0x34, 11, 10>();
    InstallVirtualCallThunk<0x824F1B70, 0x8, 11, 10>();
    InstallVirtualCallThunk<0x824F1BB0, 0x18, 11, 10>();
    InstallVirtualCallThunk<0x824F3958, 0x4, 11, 10>();
    InstallVirtualCallThunk<0x82528060, 0xC, 11, 10>();
    InstallVirtualCallThunk<0x8256B22C, 0x18, 11, 10>();
    InstallVirtualCallThunk<0x8256BB28, 0x14, 11, 10>();
    InstallVirtualCallThunk<0x82592658, 0x88, 11, 10>();
    InstallVirtualCallThunk<0x825C52C0, 0x1C, 11, 11>();
    InstallVirtualCallThunk<0x8268B568, 0x14, 11, 11>();
    InstallVirtualCallThunk<0x8268B5B0, 0x18, 11, 11>();
    InstallVirtualCallThunk<0x8268B680, 0xC, 11, 11>();
    InstallVirtualCallThunk<0x8268B6C8, 0x10, 11, 11>();
    InstallVirtualCallThunk<0x82698ED4, 0x10, 11, 11>();
    InstallVirtualCallThunk<0x8222A88C, 0x18, 11, 10>();
    InstallVirtualCallThunk<0x8222A8A4, 0x18, 11, 10>();
    InstallVirtualCallThunk<0x8226F964, 0x14, 11, 10>();
    InstallVirtualCallThunk<0x8233BE78, 0x44, 11, 10>();
    InstallVirtualCallThunk<0x8233C3E8, 0x44, 11, 10>();
    InstallVirtualCallThunk<0x8236F218, 0xB0, 11, 10>();
    InstallVirtualCallThunk<0x8238EB30, 0x6C, 11, 10>();
    InstallVirtualCallThunk<0x823E9574, 0x8, 11, 10>();
    InstallVirtualCallThunk<0x8242D7DC, 0x2BC, 11, 10>();
    InstallVirtualCallThunk<0x8242D7EC, 0x2C0, 11, 10>();
    InstallVirtualCallThunk<0x8242D7FC, 0x2C4, 11, 10>();
    InstallVirtualCallThunk<0x8242D80C, 0x2C8, 11, 10>();
    InstallVirtualCallThunk<0x8242D81C, 0x2CC, 11, 10>();
    InstallVirtualCallThunk<0x8242D82C, 0x2D0, 11, 10>();
    InstallVirtualCallThunk<0x8242D83C, 0x2D4, 11, 10>();
    InstallVirtualCallThunk<0x8242D84C, 0x2D8, 11, 10>();
    InstallVirtualCallThunk<0x8242D85C, 0x2DC, 11, 10>();
    InstallVirtualCallThunk<0x8242D86C, 0x2E0, 11, 10>();
    InstallVirtualCallThunk<0x8242D87C, 0x2E4, 11, 10>();
    InstallVirtualCallThunk<0x8242D88C, 0x2E8, 11, 10>();
    InstallVirtualCallThunk<0x8242D89C, 0x2EC, 11, 10>();
    InstallVirtualCallThunk<0x8242D8AC, 0x2F0, 11, 10>();
    InstallVirtualCallThunk<0x8242D8BC, 0x2F4, 11, 10>();
    InstallVirtualCallThunk<0x8242D8CC, 0x2F8, 11, 10>();
    InstallVirtualCallThunk<0x8242D8DC, 0x2FC, 11, 10>();
    InstallVirtualCallThunk<0x8242D8EC, 0x300, 11, 10>();
    InstallVirtualCallThunk<0x8242D8FC, 0x304, 11, 10>();
    InstallVirtualCallThunk<0x8242D90C, 0x308, 11, 10>();
    InstallVirtualCallThunk<0x8242D91C, 0x30C, 11, 10>();
    InstallVirtualCallThunk<0x8242D92C, 0x310, 11, 10>();
    InstallVirtualCallThunk<0x8242D93C, 0x314, 11, 10>();
    InstallVirtualCallThunk<0x8242D94C, 0x318, 11, 10>();
    InstallVirtualCallThunk<0x8242D95C, 0x31C, 11, 10>();
    InstallVirtualCallThunk<0x8242D96C, 0x320, 11, 10>();
    InstallVirtualCallThunk<0x8242D97C, 0x324, 11, 10>();
    InstallVirtualCallThunk<0x8242D98C, 0x328, 11, 10>();
    InstallVirtualCallThunk<0x8242D99C, 0x32C, 11, 10>();
    InstallVirtualCallThunk<0x8242D9AC, 0x330, 11, 10>();
    InstallVirtualCallThunk<0x8242D9BC, 0x334, 11, 10>();
    InstallVirtualCallThunk<0x8242D9CC, 0x338, 11, 10>();
    InstallVirtualCallThunk<0x8242D9DC, 0x33C, 11, 10>();
    InstallVirtualCallThunk<0x8242D9EC, 0x340, 11, 10>();
    InstallVirtualCallThunk<0x8242D9FC, 0x344, 11, 10>();
    InstallVirtualCallThunk<0x8242DA0C, 0x348, 11, 10>();
    InstallVirtualCallThunk<0x8242DA1C, 0x34C, 11, 10>();
    InstallVirtualCallThunk<0x8242DA2C, 0x350, 11, 10>();
    InstallVirtualCallThunk<0x8242DA3C, 0x354, 11, 10>();
    InstallVirtualCallThunk<0x8242DA4C, 0x358, 11, 10>();
    InstallVirtualCallThunk<0x8242DA5C, 0x35C, 11, 10>();
    InstallVirtualCallThunk<0x8242DA6C, 0x360, 11, 10>();
    InstallVirtualCallThunk<0x8242DA7C, 0x364, 11, 10>();
    InstallVirtualCallThunk<0x8242DA8C, 0x368, 11, 10>();
    InstallVirtualCallThunk<0x8242DA9C, 0x36C, 11, 10>();
    InstallVirtualCallThunk<0x8242DAAC, 0x370, 11, 10>();
    InstallVirtualCallThunk<0x8242DABC, 0x374, 11, 10>();
    InstallVirtualCallThunk<0x8242DACC, 0x378, 11, 10>();
    InstallVirtualCallThunk<0x8242DADC, 0x37C, 11, 10>();
    InstallVirtualCallThunk<0x8242DAEC, 0x380, 11, 10>();
    InstallVirtualCallThunk<0x8242DAFC, 0x384, 11, 10>();
    InstallVirtualCallThunk<0x8242DB0C, 0x388, 11, 10>();
    InstallVirtualCallThunk<0x8242DB1C, 0x38C, 11, 10>();
    InstallVirtualCallThunk<0x8242DB2C, 0x390, 11, 10>();
    InstallVirtualCallThunk<0x8242DB3C, 0x394, 11, 10>();
    InstallVirtualCallThunk<0x8242DB4C, 0x398, 11, 10>();
    InstallVirtualCallThunk<0x8242DB5C, 0x39C, 11, 10>();
    InstallVirtualCallThunk<0x8242DB6C, 0x3A0, 11, 10>();
    InstallVirtualCallThunk<0x8242DB7C, 0x3A4, 11, 10>();
    InstallVirtualCallThunk<0x8242DB8C, 0x3A8, 11, 10>();
    InstallVirtualCallThunk<0x8242DB9C, 0x3AC, 11, 10>();
    InstallVirtualCallThunk<0x8242DBAC, 0x3B0, 11, 10>();
    InstallVirtualCallThunk<0x8242DBBC, 0x3B4, 11, 10>();
    InstallVirtualCallThunk<0x8242DBCC, 0x3B8, 11, 10>();
    InstallVirtualCallThunk<0x8242DBDC, 0x3BC, 11, 10>();
    InstallVirtualCallThunk<0x8242DBEC, 0x3C0, 11, 10>();
    InstallVirtualCallThunk<0x8242DBFC, 0x3C4, 11, 10>();
    InstallVirtualCallThunk<0x8242DC0C, 0x3C8, 11, 10>();
    InstallVirtualCallThunk<0x8242DC1C, 0x3CC, 11, 10>();
    InstallVirtualCallThunk<0x8242DC2C, 0x3D0, 11, 10>();
    InstallVirtualCallThunk<0x8242DC3C, 0x3D4, 11, 10>();
    InstallVirtualCallThunk<0x8242DC4C, 0x3D8, 11, 10>();
    InstallVirtualCallThunk<0x8242DC5C, 0x3DC, 11, 10>();
    InstallVirtualCallThunk<0x8242DC6C, 0x3E0, 11, 10>();
    InstallVirtualCallThunk<0x8242DC7C, 0x3E4, 11, 10>();
    InstallVirtualCallThunk<0x8242DC8C, 0x3E8, 11, 10>();
    InstallVirtualCallThunk<0x8242DC9C, 0x3EC, 11, 10>();
    InstallVirtualCallThunk<0x8242DCAC, 0x3F0, 11, 10>();
    InstallVirtualCallThunk<0x8242DCBC, 0x3F4, 11, 10>();
    InstallVirtualCallThunk<0x8242DCCC, 0x3F8, 11, 10>();
    InstallVirtualCallThunk<0x8242DCDC, 0x3FC, 11, 10>();
    InstallVirtualCallThunk<0x8242DCEC, 0x400, 11, 10>();
    InstallVirtualCallThunk<0x8242DCFC, 0x404, 11, 10>();
    InstallVirtualCallThunk<0x8242DD0C, 0x408, 11, 10>();
    InstallVirtualCallThunk<0x8242DD1C, 0x40C, 11, 10>();
    InstallVirtualCallThunk<0x8242DD2C, 0x410, 11, 10>();
    InstallVirtualCallThunk<0x8242DD3C, 0x414, 11, 10>();
    InstallVirtualCallThunk<0x8242DD4C, 0x418, 11, 10>();
    InstallVirtualCallThunk<0x8242DD5C, 0x41C, 11, 10>();
    InstallVirtualCallThunk<0x8242DD6C, 0x420, 11, 10>();
    InstallVirtualCallThunk<0x8242DD7C, 0x424, 11, 10>();
    InstallVirtualCallThunk<0x8242DD8C, 0x428, 11, 10>();
    InstallVirtualCallThunk<0x8242DD9C, 0x42C, 11, 10>();
    InstallVirtualCallThunk<0x8242DDAC, 0x430, 11, 10>();
    InstallVirtualCallThunk<0x8242DDBC, 0x434, 11, 10>();
    InstallVirtualCallThunk<0x8242DDCC, 0x438, 11, 10>();
    InstallVirtualCallThunk<0x8242DDDC, 0x43C, 11, 10>();
    InstallVirtualCallThunk<0x8242DDEC, 0x440, 11, 10>();
    InstallVirtualCallThunk<0x8242DDFC, 0x444, 11, 10>();
    InstallVirtualCallThunk<0x8242DE0C, 0x448, 11, 10>();
    InstallVirtualCallThunk<0x8242DE1C, 0x44C, 11, 10>();
    InstallVirtualCallThunk<0x8242DE2C, 0x450, 11, 10>();
    InstallVirtualCallThunk<0x8242DE3C, 0x454, 11, 10>();
    InstallVirtualCallThunk<0x8242DE4C, 0x458, 11, 10>();
    InstallVirtualCallThunk<0x8242DE5C, 0x45C, 11, 10>();
    InstallVirtualCallThunk<0x8242DE6C, 0x460, 11, 10>();
    InstallVirtualCallThunk<0x8242DE7C, 0x464, 11, 10>();
    InstallVirtualCallThunk<0x8242DE8C, 0x468, 11, 10>();
    InstallVirtualCallThunk<0x8242DE9C, 0x46C, 11, 10>();
    InstallVirtualCallThunk<0x8242DEAC, 0x470, 11, 10>();
    InstallVirtualCallThunk<0x8242DEBC, 0x474, 11, 10>();
    InstallVirtualCallThunk<0x8242DECC, 0x478, 11, 10>();
    InstallVirtualCallThunk<0x8242DEDC, 0x47C, 11, 10>();
    InstallVirtualCallThunk<0x8242DEEC, 0x480, 11, 10>();
    InstallVirtualCallThunk<0x8242DEFC, 0x484, 11, 10>();
    InstallVirtualCallThunk<0x8242DF0C, 0x488, 11, 10>();
    InstallVirtualCallThunk<0x8242DF1C, 0x48C, 11, 10>();
    InstallVirtualCallThunk<0x8242DF2C, 0x490, 11, 10>();
    InstallVirtualCallThunk<0x8242DF3C, 0x494, 11, 10>();
    InstallVirtualCallThunk<0x8242DF4C, 0x498, 11, 10>();
    InstallVirtualCallThunk<0x8242DF5C, 0x49C, 11, 10>();
    InstallVirtualCallThunk<0x8242DF6C, 0x4A0, 11, 10>();
    InstallVirtualCallThunk<0x8242DF7C, 0x4A4, 11, 10>();
    InstallVirtualCallThunk<0x8242DF8C, 0x4A8, 11, 10>();
    InstallVirtualCallThunk<0x8242DF9C, 0x4AC, 11, 10>();
    InstallVirtualCallThunk<0x8242DFAC, 0x4B0, 11, 10>();
    InstallVirtualCallThunk<0x8242DFBC, 0x4B4, 11, 10>();
    InstallVirtualCallThunk<0x8242DFCC, 0x4B8, 11, 10>();
    InstallVirtualCallThunk<0x8242DFDC, 0x4BC, 11, 10>();
    InstallVirtualCallThunk<0x8242DFEC, 0x4C0, 11, 10>();
    InstallVirtualCallThunk<0x8242DFFC, 0x4C4, 11, 10>();
    InstallVirtualCallThunk<0x8242E00C, 0x4C8, 11, 10>();
    InstallVirtualCallThunk<0x8242E01C, 0x4CC, 11, 10>();
    InstallVirtualCallThunk<0x8242E02C, 0x4D0, 11, 10>();
    InstallVirtualCallThunk<0x8242E03C, 0x4D4, 11, 10>();
    InstallVirtualCallThunk<0x8242E04C, 0x4D8, 11, 10>();
    InstallVirtualCallThunk<0x8242E05C, 0x4DC, 11, 10>();
    InstallVirtualCallThunk<0x8242E06C, 0x4E0, 11, 10>();
    InstallVirtualCallThunk<0x8242E07C, 0x4E4, 11, 10>();
    InstallVirtualCallThunk<0x8242E08C, 0x4E8, 11, 10>();
    InstallVirtualCallThunk<0x8242E09C, 0x4EC, 11, 10>();
    InstallVirtualCallThunk<0x8242E0AC, 0x4F0, 11, 10>();
    InstallVirtualCallThunk<0x8242E0BC, 0x4F4, 11, 10>();
    InstallVirtualCallThunk<0x8242E0CC, 0x4F8, 11, 10>();
    InstallVirtualCallThunk<0x8242E0DC, 0x4FC, 11, 10>();
    InstallVirtualCallThunk<0x8242E0EC, 0x500, 11, 10>();
    InstallVirtualCallThunk<0x8242E0FC, 0x504, 11, 10>();
    InstallVirtualCallThunk<0x8242E10C, 0x508, 11, 10>();
    InstallVirtualCallThunk<0x8242E11C, 0x50C, 11, 10>();
    InstallVirtualCallThunk<0x8242E12C, 0x510, 11, 10>();
    InstallVirtualCallThunk<0x8242E13C, 0x514, 11, 10>();
    InstallVirtualCallThunk<0x8242E14C, 0x518, 11, 10>();
    InstallVirtualCallThunk<0x8242E15C, 0x51C, 11, 10>();
    InstallVirtualCallThunk<0x8242E16C, 0x520, 11, 10>();
    InstallVirtualCallThunk<0x8242E17C, 0x524, 11, 10>();
    InstallVirtualCallThunk<0x8242E18C, 0x528, 11, 10>();
    InstallVirtualCallThunk<0x8242E19C, 0x52C, 11, 10>();
    InstallVirtualCallThunk<0x8242E1AC, 0x530, 11, 10>();
    InstallVirtualCallThunk<0x8242E1BC, 0x534, 11, 10>();
    InstallVirtualCallThunk<0x8242E1CC, 0x538, 11, 10>();
    InstallVirtualCallThunk<0x8242E1DC, 0x53C, 11, 10>();
    InstallVirtualCallThunk<0x8242E1EC, 0x540, 11, 10>();
    InstallVirtualCallThunk<0x8242E1FC, 0x544, 11, 10>();
    InstallVirtualCallThunk<0x8242E20C, 0x548, 11, 10>();
    InstallVirtualCallThunk<0x8242E21C, 0x54C, 11, 10>();
    InstallVirtualCallThunk<0x8242E22C, 0x550, 11, 10>();
    InstallVirtualCallThunk<0x8242E23C, 0x554, 11, 10>();
    InstallVirtualCallThunk<0x8242E24C, 0x558, 11, 10>();
    InstallVirtualCallThunk<0x8242E25C, 0x55C, 11, 10>();
    InstallVirtualCallThunk<0x8242E26C, 0x560, 11, 10>();
    InstallVirtualCallThunk<0x8242E27C, 0x564, 11, 10>();
    InstallVirtualCallThunk<0x8242E28C, 0x568, 11, 10>();
    InstallVirtualCallThunk<0x8242E29C, 0x56C, 11, 10>();
    InstallVirtualCallThunk<0x8242E2AC, 0x570, 11, 10>();
    InstallVirtualCallThunk<0x8242E2BC, 0x574, 11, 10>();
    InstallVirtualCallThunk<0x824F1B90, 0x14, 11, 10>();
    InstallVirtualCallThunk<0x824F55E0, 0xC, 11, 10>();
    InstallVirtualCallThunk<0x824F5628, 0x10, 11, 10>();
    InstallVirtualCallThunk<0x824FADE0, 0x34, 11, 10>();
    InstallVirtualCallThunk<0x824FAE04, 0x34, 11, 10>();
    InstallVirtualCallThunk<0x824FAE30, 0x3C, 11, 10>();
    InstallVirtualCallThunk<0x824FAE5C, 0x44, 11, 10>();
    InstallVirtualCallThunk<0x824FAE88, 0x4C, 11, 10>();
    InstallVirtualCallThunk<0x824FAED8, 0x38, 11, 10>();
    InstallVirtualCallThunk<0x824FAF28, 0x40, 11, 10>();
    InstallVirtualCallThunk<0x824FAF54, 0x48, 11, 10>();
    InstallVirtualCallThunk<0x824FAF80, 0x50, 11, 10>();
    InstallVirtualCallThunk<0x8254DB84, 0x10, 11, 10>();
    InstallVirtualCallThunk<0x8254DB9C, 0x14, 11, 10>();
    InstallVirtualCallThunk<0x8254DBB4, 0x18, 11, 10>();
    InstallVirtualCallThunk<0x8254DBCC, 0x2C, 11, 10>();
    InstallVirtualCallThunk<0x8256B258, 0x18, 11, 10>();
    InstallVirtualCallThunk<0x8256B280, 0x1C, 11, 10>();
    InstallVirtualCallThunk<0x8256B3D4, 0xC, 7, 6>();
    InstallVirtualCallThunk<0x8256B414, 0x10, 11, 10>();
    InstallVirtualCallThunk<0x8256B44C, 0x14, 7, 6>();
    InstallVirtualCallThunk<0x8256B48C, 0x18, 11, 10>();
    InstallVirtualCallThunk<0x8256E00C, 0x138, 11, 10>();
    InstallVirtualCallThunk<0x8256E034, 0x134, 11, 10>();
    InstallVirtualCallThunk<0x8256E26C, 0x80, 11, 10>();
    InstallVirtualCallThunk<0x8256E284, 0x84, 11, 10>();
    InstallVirtualCallThunk<0x8256E29C, 0x88, 11, 10>();
    InstallVirtualCallThunk<0x8256E2B4, 0xB8, 11, 10>();
    InstallVirtualCallThunk<0x8256E2CC, 0xBC, 11, 10>();
    InstallVirtualCallThunk<0x8256E2E4, 0xC8, 11, 10>();
    InstallVirtualCallThunk<0x8256E2FC, 0x110, 11, 10>();
    InstallVirtualCallThunk<0x8256E314, 0x114, 11, 10>();
    InstallVirtualCallThunk<0x8256E32C, 0x5C, 11, 10>();
    InstallVirtualCallThunk<0x8256E344, 0x60, 11, 10>();
    InstallVirtualCallThunk<0x8256E35C, 0x64, 11, 10>();
    InstallVirtualCallThunk<0x8256E3BC, 0x18, 11, 10>();
    InstallVirtualCallThunk<0x8256E3D4, 0x1C, 11, 10>();
    InstallVirtualCallThunk<0x8256E3EC, 0xD0, 11, 10>();
    InstallVirtualCallThunk<0x8256E404, 0x130, 11, 10>();
    InstallVirtualCallThunk<0x8256E41C, 0x124, 11, 10>();
    InstallVirtualCallThunk<0x8256E434, 0x118, 11, 10>();
    InstallVirtualCallThunk<0x8256E44C, 0x11C, 11, 10>();
    InstallVirtualCallThunk<0x8256E6BC, 0x128, 11, 10>();
    InstallVirtualCallThunk<0x8256E6D4, 0x12C, 11, 10>();
    InstallVirtualCallThunk<0x8256E824, 0xE8, 11, 10>();
    InstallVirtualCallThunk<0x8256E83C, 0xEC, 11, 10>();
    InstallVirtualCallThunk<0x8256E858, 0x70, 11, 10>();
    InstallVirtualCallThunk<0x8256E924, 0x8C, 11, 10>();
    InstallVirtualCallThunk<0x8256E998, 0x70, 11, 10>();
    InstallVirtualCallThunk<0x8256E9AC, 0x108, 11, 10>();
    InstallVirtualCallThunk<0x8256E9C4, 0x10C, 11, 10>();
    InstallVirtualCallThunk<0x8256E9DC, 0xDC, 11, 10>();
    InstallVirtualCallThunk<0x8256E9F4, 0x20, 11, 10>();
    InstallVirtualCallThunk<0x8256EA0C, 0x74, 11, 10>();
    InstallVirtualCallThunk<0x8256EA24, 0x7C, 11, 10>();
    InstallVirtualCallThunk<0x8256EA3C, 0xB0, 11, 10>();
    InstallVirtualCallThunk<0x8256EA54, 0xA8, 11, 10>();
    InstallVirtualCallThunk<0x8256EA6C, 0xD4, 11, 10>();
    InstallVirtualCallThunk<0x8256EA84, 0xD8, 11, 10>();
    InstallVirtualCallThunk<0x8256EA9C, 0x54, 11, 10>();
    InstallVirtualCallThunk<0x8256EAB4, 0x58, 11, 10>();
    InstallVirtualCallThunk<0x8256EACC, 0x68, 11, 10>();
    InstallVirtualCallThunk<0x8256EAE4, 0x6C, 11, 10>();
    InstallVirtualCallThunk<0x8256EAFC, 0xC0, 11, 10>();
    InstallVirtualCallThunk<0x8256EB14, 0xC4, 11, 10>();
    InstallVirtualCallThunk<0x8256EBDC, 0x90, 11, 10>();
    InstallVirtualCallThunk<0x8256EBF4, 0x94, 11, 10>();
    InstallVirtualCallThunk<0x8256EC0C, 0x98, 11, 10>();
    InstallVirtualCallThunk<0x8256EC24, 0x9C, 11, 10>();
    InstallVirtualCallThunk<0x8256EC3C, 0xA0, 11, 10>();
    InstallVirtualCallThunk<0x8256EC54, 0xA4, 11, 10>();
    InstallVirtualCallThunk<0x8256EC7C, 0x34, 9, 8>();
    InstallVirtualCallThunk<0x8256ED84, 0xF0, 11, 10>();
    InstallVirtualCallThunk<0x8256ED9C, 0x78, 11, 10>();
    InstallVirtualCallThunk<0x8256EDB4, 0xAC, 11, 10>();
    InstallVirtualCallThunk<0x8256EDCC, 0xB4, 11, 10>();
    InstallVirtualCallThunk<0x8267F87C, 0x0, 11, 11>();
    InstallVirtualCallThunk<0x8267F8A0, 0x4, 11, 11>();
    InstallVirtualCallThunk<0x8267F8C8, 0xC, 11, 11>();
    InstallVirtualCallThunk<0x8267F8F0, 0x14, 11, 11>();
    InstallVirtualCallThunk<0x82686030, 0x0, 11, 11>();
    InstallVirtualCallThunk<0x82686058, 0x4, 11, 11>();
    InstallVirtualCallThunk<0x82686080, 0xC, 11, 11>();
    InstallVirtualCallThunk<0x82691BEC, 0x28, 11, 11>();
    InstallVirtualCallThunk<0x82698814, 0xC, 11, 11>();
    InstallVirtualCallThunk<0x82698838, 0xC, 11, 11>();
    InstallVirtualCallThunk<0x82698874, 0x10, 11, 11>();
    InstallVirtualCallThunk<0x82698898, 0x10, 11, 11>();
    InstallVirtualCallThunk<0x82698B88, 0x8, 11, 11>();
    InstallVirtualCallThunk<0x8269E0A4, 0x1C, 11, 11>();
    InstallVirtualCallThunk<0x8269E0D0, 0xC, 11, 11>();
    InstallVirtualCallThunk<0x8269E100, 0x10, 11, 11>();

    // Tekken's retail storage bootstrap opens the optical-image device before
    // it starts requesting DATA*.IDX/BIN content.  RexGlue's extracted-tree
    // VFS mounts the same bytes at Partition1, but does not expose the retail
    // Image device name.  Alias only that exact device to the mounted game
    // volume; this keeps all normal file/content semantics on the stock VFS.
    if (runtime() && runtime()->file_system()) {
      // Keep the extracted-tree device for relative DATA/IDX opens, but make
      // its root handle expose the one raw volume sector the retail bootstrap
      // probes at offset 0x800.  This is an opt-in device feature; it does not
      // alter normal HostPathEntry reads.
      if (auto* partition1 = runtime()->file_system()->ResolvePath(
              "\\Device\\Harddisk0\\Partition1")) {
        if (auto* host_device = dynamic_cast<rex::filesystem::HostPathDevice*>(
                partition1->device())) {
          host_device->EnableRootSectorProbe();
        } else {
          REXLOG_WARN("Tekken6 storage bootstrap: Partition1 is not a HostPathDevice");
        }
      } else {
        REXLOG_WARN("Tekken6 storage bootstrap: Partition1 device is unavailable");
      }
      runtime()->file_system()->RegisterSymbolicLink(
          "\\Device\\Image", "\\Device\\Harddisk0\\Partition1");
      // The retail bootstrap opens Partition0 for the image-header probe and
      // then uses that handle as the root of relative content opens.  The
      // generic SDK registers it as an empty NullDevice, which makes those
      // reads succeed with zero bytes and prevents DINF/IDX discovery.  The
      // extracted ISO tree is the truthful read-only backing for this title.
      runtime()->file_system()->RegisterSymbolicLink(
          "\\Device\\Harddisk0\\Partition0", "\\Device\\Harddisk0\\Partition1");
    }
  }

 private:
  template <uint32_t Address, uint32_t SlotOffset,
            uint32_t VtableRegister = 12, uint32_t MethodRegister = 11>
  void InstallVirtualCallThunk() {
    if (!runtime() || !runtime()->function_dispatcher()) return;
    if (!runtime()->function_dispatcher()->SetFunction(
            Address,
            &VirtualCallThunk<SlotOffset, VtableRegister, MethodRegister>)) {
      REXLOG_WARN("Tekken6 could not register vtable thunk {:08X} (slot +{:X})",
                  Address, SlotOffset);
    }
  }

  template <uint32_t Register>
  static void SetGpr(PPCContext& ctx, uint32_t value) {
    if constexpr (Register == 0) ctx.r0.u64 = value;
    else if constexpr (Register == 1) ctx.r1.u64 = value;
    else if constexpr (Register == 2) ctx.r2.u64 = value;
    else if constexpr (Register == 3) ctx.r3.u64 = value;
    else if constexpr (Register == 4) ctx.r4.u64 = value;
    else if constexpr (Register == 5) ctx.r5.u64 = value;
    else if constexpr (Register == 6) ctx.r6.u64 = value;
    else if constexpr (Register == 7) ctx.r7.u64 = value;
    else if constexpr (Register == 8) ctx.r8.u64 = value;
    else if constexpr (Register == 9) ctx.r9.u64 = value;
    else if constexpr (Register == 10) ctx.r10.u64 = value;
    else if constexpr (Register == 11) ctx.r11.u64 = value;
    else if constexpr (Register == 12) ctx.r12.u64 = value;
    else if constexpr (Register == 13) ctx.r13.u64 = value;
    else if constexpr (Register == 14) ctx.r14.u64 = value;
    else if constexpr (Register == 15) ctx.r15.u64 = value;
    else if constexpr (Register == 16) ctx.r16.u64 = value;
    else if constexpr (Register == 17) ctx.r17.u64 = value;
    else if constexpr (Register == 18) ctx.r18.u64 = value;
    else if constexpr (Register == 19) ctx.r19.u64 = value;
    else if constexpr (Register == 20) ctx.r20.u64 = value;
    else if constexpr (Register == 21) ctx.r21.u64 = value;
    else if constexpr (Register == 22) ctx.r22.u64 = value;
    else if constexpr (Register == 23) ctx.r23.u64 = value;
    else if constexpr (Register == 24) ctx.r24.u64 = value;
    else if constexpr (Register == 25) ctx.r25.u64 = value;
    else if constexpr (Register == 26) ctx.r26.u64 = value;
    else if constexpr (Register == 27) ctx.r27.u64 = value;
    else if constexpr (Register == 28) ctx.r28.u64 = value;
    else if constexpr (Register == 29) ctx.r29.u64 = value;
    else if constexpr (Register == 30) ctx.r30.u64 = value;
    else if constexpr (Register == 31) ctx.r31.u64 = value;
  }

  template <uint32_t SlotOffset, uint32_t VtableRegister,
            uint32_t MethodRegister>
  static void VirtualCallThunk(PPCContext& ctx, uint8_t* base) {
    const uint32_t vtable = rex::memory::load_and_swap<uint32_t>(
        base + ctx.r3.u32);
    const uint32_t slot_address = vtable + SlotOffset;
    const uint32_t method = rex::memory::load_and_swap<uint32_t>(
        base + slot_address);
    SetGpr<VtableRegister>(ctx, vtable);
    SetGpr<MethodRegister>(ctx, method);
    ctx.ctr.u64 = method;
    ctx.last_indirect_target = method;
    // Use the same bounds-checked module table as generated bctr calls. The
    // resolver remains the fallback for cross-module and unregistered targets.
    REX_CALL_INDIRECT_FUNC(method);
  }

};
