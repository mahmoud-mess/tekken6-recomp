// Verify the display name follows the account without changing saved identity.
#include <rex/system/xam/user_profile.h>

#include <pwd.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

int main() {
  const auto* account = getpwuid(getuid());
  if (!account || !account->pw_name || !account->pw_name[0]) {
    throw std::runtime_error("test requires a resolvable Linux account");
  }
  const std::string expected = account->pw_name;
  setenv("USER", "wrong-environment-name", 1);
  setenv("LOGNAME", "wrong-environment-name", 1);
  rex::system::xam::UserProfile profile;
  if (profile.display_name() != expected || profile.name() != "User" ||
      profile.xuid() != 0xB13EBABEBABEBABE) {
    throw std::runtime_error("profile display name or storage identity regression");
  }
  std::cout << "PASS: display name = " << profile.display_name()
            << "; save identity unchanged\n";
}
