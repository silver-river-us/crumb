#include "plugins/google_drive/google_drive_plugin.hpp"

#include <cassert>

int main() {
    assert(crumb::plugins::google_drive::GoogleDrivePlugin::url_for_item_id("abc_123-xyz") ==
           "https://drive.google.com/open?id=abc_123-xyz");
}
