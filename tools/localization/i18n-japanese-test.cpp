#include "i18n.hpp"
#include <cassert>

int main()
{
    assert(i18n::init("3ds/assets/romfs/i18n.json"));
    i18n::setLanguage("ja");
    assert(i18n::language() == "ja");
    assert(i18n::t("main.restore") == "復元");
    assert(i18n::t("outcome.restore_failed", {"テスト"}) == "テストの復元に失敗しました。");
    assert(i18n::t("settings.about.sd_usage", {"1 MiB", "2 MiB"}) == "2 MiB中1 MiB使用");
    i18n::setLanguage("en");
    assert(i18n::t("main.restore") == "Restore");
}
