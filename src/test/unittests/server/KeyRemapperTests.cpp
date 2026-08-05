/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2002 Chris Schoeneman
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define INPUTLEAP_TEST_ENV

#include "server/Config.h"
#include "server/KeyRemapper.h"

#include "test/global/gtest.h"

#include <sstream>

using namespace inputleap;

namespace {

const KeyButton kControlButton = 1;
const KeyButton kSpaceButton = 2;
const KeyButton kAltButton = 3;
const KeyButton kSuperButton = 4;
const KeyButton kCButton = 5;

void
expectEvent(const KeyRemapper::KeyEvent& event,
		KeyRemapper::KeyEvent::Type type, KeyID id,
		KeyModifierMask mask, KeyButton button)
{
	EXPECT_EQ(type, event.m_type);
	EXPECT_EQ(id, event.m_id);
	EXPECT_EQ(mask, event.m_mask);
	EXPECT_EQ(button, event.m_button);
}

}

TEST(KeyRemapperTests, emptyConfigPassesEventsThrough)
{
	KeyRemapper remapper;
	KeyRemapper::KeyEventList down =
		remapper.remapKeyDown("mac", 'a', KeyModifierShift, kCButton);
	KeyRemapper::KeyEventList repeat =
		remapper.remapKeyRepeat("mac", 'a', KeyModifierShift, 3, kCButton);
	KeyRemapper::KeyEventList up =
		remapper.remapKeyUp("mac", 'a', KeyModifierShift, kCButton);

	ASSERT_EQ(1u, down.size());
	expectEvent(down[0], KeyRemapper::KeyEvent::kDown, 'a',
		KeyModifierShift, kCButton);
	ASSERT_EQ(1u, repeat.size());
	expectEvent(repeat[0], KeyRemapper::KeyEvent::kRepeat, 'a',
		KeyModifierShift, kCButton);
	EXPECT_EQ(3, repeat[0].m_count);
	ASSERT_EQ(1u, up.size());
	expectEvent(up[0], KeyRemapper::KeyEvent::kUp, 'a',
		KeyModifierShift, kCButton);
}

TEST(KeyRemapperTests, simpleRemapTranslatesModifierKeyAndMask)
{
	KeyRemapConfig config;
	config.addRule("mac", kKeyAlt_R, kKeySuper_R);

	KeyRemapper remapper(config);
	KeyRemapper::KeyEventList down =
		remapper.remapKeyDown("mac", kKeyAlt_R, KeyModifierAlt, kAltButton);
	KeyRemapper::KeyEventList up =
		remapper.remapKeyUp("mac", kKeyAlt_R, KeyModifierAlt, kAltButton);

	ASSERT_EQ(1u, down.size());
	expectEvent(down[0], KeyRemapper::KeyEvent::kDown, kKeySuper_R,
		KeyModifierSuper, kAltButton);
	ASSERT_EQ(1u, up.size());
	expectEvent(up[0], KeyRemapper::KeyEvent::kUp, kKeySuper_R,
		KeyModifierSuper, kAltButton);
}

TEST(KeyRemapperTests, tapRuleEmitsAloneKeyOnKeyUp)
{
	KeyRemapConfig config;
	config.addTapRule("mac", kKeySuper_R, kKeyF19, kKeySuper_R);

	KeyRemapper remapper(config);
	KeyRemapper::KeyEventList down =
		remapper.remapKeyDown("mac", kKeySuper_R, KeyModifierSuper,
			kSuperButton);
	KeyRemapper::KeyEventList up =
		remapper.remapKeyUp("mac", kKeySuper_R, KeyModifierSuper,
			kSuperButton);

	EXPECT_TRUE(down.empty());
	ASSERT_EQ(2u, up.size());
	expectEvent(up[0], KeyRemapper::KeyEvent::kDown, kKeyF19,
		0, kSuperButton);
	expectEvent(up[1], KeyRemapper::KeyEvent::kUp, kKeyF19,
		0, kSuperButton);
}

TEST(KeyRemapperTests, tapRuleCanEmitModifierChord)
{
	KeyRemapConfig config;
	config.addTapRule("mac", kKeyAlt_R, ' ', kKeyAlt_R,
		KeyModifierControl);

	KeyRemapper remapper(config);
	KeyRemapper::KeyEventList down =
		remapper.remapKeyDown("mac", kKeyAlt_R, KeyModifierAlt, kAltButton);
	KeyRemapper::KeyEventList up =
		remapper.remapKeyUp("mac", kKeyAlt_R, KeyModifierAlt, kAltButton);

	EXPECT_TRUE(down.empty());
	ASSERT_EQ(2u, up.size());
	expectEvent(up[0], KeyRemapper::KeyEvent::kDown, ' ',
		KeyModifierControl, kAltButton);
	expectEvent(up[1], KeyRemapper::KeyEvent::kUp, ' ',
		KeyModifierControl, kAltButton);
}

TEST(KeyRemapperTests, rightAltTapSendsPlainF19AndHoldActsAsRightSuper)
{
	KeyRemapConfig config;
	config.addTapRule("mac", kKeyAlt_R, kKeyF19, kKeySuper_R);

	KeyRemapper remapper(config);
	KeyRemapper::KeyEventList pending =
		remapper.remapKeyDown("mac", kKeyAlt_R, KeyModifierAlt, kAltButton);
	KeyRemapper::KeyEventList tap =
		remapper.remapKeyUp("mac", kKeyAlt_R, KeyModifierAlt, kAltButton);

	EXPECT_TRUE(pending.empty());
	ASSERT_EQ(2u, tap.size());
	expectEvent(tap[0], KeyRemapper::KeyEvent::kDown, kKeyF19,
		0, kAltButton);
	expectEvent(tap[1], KeyRemapper::KeyEvent::kUp, kKeyF19,
		0, kAltButton);

	pending = remapper.remapKeyDown(
		"mac", kKeyAlt_R, KeyModifierAlt, kAltButton);
	KeyRemapper::KeyEventList chord =
		remapper.remapKeyDown("mac", 'c', KeyModifierAlt, kCButton);

	EXPECT_TRUE(pending.empty());
	ASSERT_EQ(2u, chord.size());
	expectEvent(chord[0], KeyRemapper::KeyEvent::kDown, kKeySuper_R,
		KeyModifierSuper, kAltButton);
	expectEvent(chord[1], KeyRemapper::KeyEvent::kDown, 'c',
		KeyModifierSuper, kCButton);
}

TEST(KeyRemapperTests, windowsKoreanRightAltHangulTapSendsPlainF19)
{
	KeyRemapConfig config;
	config.addTapRule("mac", kKeyHangul, kKeyF19, kKeySuper_R);

	KeyRemapper remapper(config);
	KeyRemapper::KeyEventList down =
		remapper.remapKeyDown("mac", kKeyHangul, 0, kAltButton);
	KeyRemapper::KeyEventList up =
		remapper.remapKeyUp("mac", kKeyHangul, 0, kAltButton);

	EXPECT_TRUE(down.empty());
	ASSERT_EQ(2u, up.size());
	expectEvent(up[0], KeyRemapper::KeyEvent::kDown, kKeyF19,
		0, kAltButton);
	expectEvent(up[1], KeyRemapper::KeyEvent::kUp, kKeyF19,
		0, kAltButton);
}

TEST(KeyRemapperTests, tapRuleFlushesHoldBeforeChordKey)
{
	KeyRemapConfig config;
	config.addTapRule("mac", kKeySuper_R, kKeyF19, kKeySuper_R);

	KeyRemapper remapper(config);
	KeyRemapper::KeyEventList pending =
		remapper.remapKeyDown("mac", kKeySuper_R, KeyModifierSuper,
			kSuperButton);
	KeyRemapper::KeyEventList cDown =
		remapper.remapKeyDown("mac", 'c', KeyModifierSuper, kCButton);

	EXPECT_TRUE(pending.empty());
	ASSERT_EQ(2u, cDown.size());
	expectEvent(cDown[0], KeyRemapper::KeyEvent::kDown, kKeySuper_R,
		KeyModifierSuper, kSuperButton);
	expectEvent(cDown[1], KeyRemapper::KeyEvent::kDown, 'c',
		KeyModifierSuper, kCButton);
}

TEST(KeyRemapperTests, tapRuleFlushesHoldForTimeout)
{
	KeyRemapConfig config;
	config.addTapRule("mac", kKeySuper_R, kKeyF19, kKeySuper_R);

	KeyRemapper remapper(config);
	KeyRemapper::KeyEventList pending =
		remapper.remapKeyDown("mac", kKeySuper_R, KeyModifierSuper,
			kSuperButton);
	KeyRemapper::ScreenKeyEventMap timeoutEvents =
		remapper.flushPendingTapHolds();
	KeyRemapper::KeyEventList up =
		remapper.remapKeyUp("mac", kKeySuper_R, KeyModifierSuper,
			kSuperButton);

	EXPECT_TRUE(pending.empty());
	ASSERT_EQ(1u, timeoutEvents.size());
	ASSERT_EQ(1u, timeoutEvents["mac"].size());
	expectEvent(timeoutEvents["mac"][0], KeyRemapper::KeyEvent::kDown,
		kKeySuper_R, KeyModifierSuper, kSuperButton);
	ASSERT_EQ(1u, up.size());
	expectEvent(up[0], KeyRemapper::KeyEvent::kUp, kKeySuper_R,
		KeyModifierSuper, kSuperButton);
}

TEST(KeyRemapperTests, chordRuleTapsTargetAndSuppressesSourceKeyUp)
{
	KeyRemapConfig config;
	config.addChordRule("mac", KeyModifierControl, ' ', 0, kKeyF19);

	KeyRemapper remapper(config);
	KeyRemapper::KeyEventList controlDown =
		remapper.remapKeyDown("mac", kKeyControl_L, KeyModifierControl,
			kControlButton);
	KeyRemapper::KeyEventList spaceDown =
		remapper.remapKeyDown("mac", ' ', KeyModifierControl, kSpaceButton);
	KeyRemapper::KeyEventList spaceRepeat =
		remapper.remapKeyRepeat("mac", ' ', KeyModifierControl, 1,
			kSpaceButton);
	KeyRemapper::KeyEventList spaceUp =
		remapper.remapKeyUp("mac", ' ', KeyModifierControl, kSpaceButton);
	KeyRemapper::KeyEventList controlUp =
		remapper.remapKeyUp("mac", kKeyControl_L, KeyModifierControl,
			kControlButton);

	ASSERT_EQ(1u, controlDown.size());
	expectEvent(controlDown[0], KeyRemapper::KeyEvent::kDown,
		kKeyControl_L, KeyModifierControl, kControlButton);
	ASSERT_EQ(4u, spaceDown.size());
	expectEvent(spaceDown[0], KeyRemapper::KeyEvent::kUp,
		kKeyControl_L, 0, kControlButton);
	expectEvent(spaceDown[1], KeyRemapper::KeyEvent::kDown,
		kKeyF19, 0, kSpaceButton);
	expectEvent(spaceDown[2], KeyRemapper::KeyEvent::kUp,
		kKeyF19, 0, kSpaceButton);
	expectEvent(spaceDown[3], KeyRemapper::KeyEvent::kDown,
		kKeyControl_L, KeyModifierControl, kControlButton);
	EXPECT_TRUE(spaceRepeat.empty());
	EXPECT_TRUE(spaceUp.empty());
	ASSERT_EQ(1u, controlUp.size());
	expectEvent(controlUp[0], KeyRemapper::KeyEvent::kUp,
		kKeyControl_L, KeyModifierControl, kControlButton);
}

TEST(KeyRemapperTests, chordRuleDoesNotMatchExtraModifiers)
{
	KeyRemapConfig config;
	config.addChordRule("mac", KeyModifierControl, ' ', 0, kKeyF19);

	KeyRemapper remapper(config);
	KeyRemapper::KeyEventList spaceDown =
		remapper.remapKeyDown("mac", ' ',
			KeyModifierControl | KeyModifierShift, kSpaceButton);

	ASSERT_EQ(1u, spaceDown.size());
	expectEvent(spaceDown[0], KeyRemapper::KeyEvent::kDown, ' ',
		KeyModifierControl | KeyModifierShift, kSpaceButton);
}

TEST(KeyRemapperTests, unmodifiedKeyCanEmitModifierChord)
{
	KeyRemapConfig config;
	config.addChordRule("mac", 0, kKeyPrint,
		KeyModifierSuper | KeyModifierShift, '4');

	KeyRemapper remapper(config);
	KeyRemapper::KeyEventList down =
		remapper.remapKeyDown("mac", kKeyPrint, 0, 0x0063);
	KeyRemapper::KeyEventList repeat =
		remapper.remapKeyRepeat("mac", kKeyPrint, 0, 1, 0x0063);
	KeyRemapper::KeyEventList up =
		remapper.remapKeyUp("mac", kKeyPrint, 0, 0x0063);

	ASSERT_EQ(2u, down.size());
	expectEvent(down[0], KeyRemapper::KeyEvent::kDown, '4',
		KeyModifierSuper | KeyModifierShift, 0x0063);
	expectEvent(down[1], KeyRemapper::KeyEvent::kUp, '4',
		KeyModifierSuper | KeyModifierShift, 0x0063);
	EXPECT_TRUE(repeat.empty());
	EXPECT_TRUE(up.empty());
}

TEST(KeyRemapperTests, configReadsChordRulesFromRemapsSection)
{
	Config config;
	std::stringstream stream;
	stream
		<< "section: screens\n"
		<< "\tserver:\n"
		<< "\tmac:\n"
		<< "end\n"
		<< "section: remaps\n"
		<< "\tmac:\n"
		<< "\t\tcontrol+space = F19\n"
		<< "end\n";

	stream >> config;

	const KeyRemapConfig::ChordRule* rule =
		config.get_key_remap_config().findChordRule("mac", ' ',
			KeyModifierControl);
	ASSERT_TRUE(rule != nullptr);
	EXPECT_EQ(KeyModifierControl, rule->m_fromMask);
	EXPECT_EQ(' ', rule->m_fromID);
	EXPECT_EQ(0u, rule->m_toMask);
	EXPECT_EQ(kKeyF19, rule->m_toID);
}

TEST(KeyRemapperTests, configReadsUnmodifiedSourceToChordTarget)
{
	Config config;
	std::stringstream stream;
	stream
		<< "section: screens\n"
		<< "\tserver:\n"
		<< "\tmac:\n"
		<< "end\n"
		<< "section: remaps\n"
		<< "\tmac:\n"
		<< "\t\tprint_screen = command+shift+4\n"
		<< "end\n";

	stream >> config;

	const KeyRemapConfig::ChordRule* rule =
		config.get_key_remap_config().findChordRule("mac", kKeyPrint, 0);
	ASSERT_TRUE(rule != nullptr);
	EXPECT_EQ(0u, rule->m_fromMask);
	EXPECT_EQ(kKeyPrint, rule->m_fromID);
	EXPECT_EQ(KeyModifierSuper | KeyModifierShift, rule->m_toMask);
	EXPECT_EQ('4', rule->m_toID);
}

TEST(KeyRemapperTests, configReadsWindowsStyleCopyAndPasteChords)
{
	Config config;
	std::stringstream stream;
	stream
		<< "section: screens\n"
		<< "\tserver:\n"
		<< "\tmac:\n"
		<< "end\n"
		<< "section: remaps\n"
		<< "\tmac:\n"
		<< "\t\tcontrol+c = command+c\n"
		<< "\t\tcontrol+v = command+v\n"
		<< "end\n";

	stream >> config;

	const auto& remaps = config.get_key_remap_config();
	const auto* copy = remaps.findChordRule("mac", 'c', KeyModifierControl);
	const auto* paste = remaps.findChordRule("mac", 'v', KeyModifierControl);
	ASSERT_NE(nullptr, copy);
	ASSERT_NE(nullptr, paste);
	EXPECT_EQ(KeyModifierSuper, copy->m_toMask);
	EXPECT_EQ('c', copy->m_toID);
	EXPECT_EQ(KeyModifierSuper, paste->m_toMask);
	EXPECT_EQ('v', paste->m_toID);
}
TEST(KeyRemapperTests, configReadsHangulTapAlias)
{
	Config config;
	std::stringstream stream;
	stream
		<< "section: screens\n"
		<< "\tserver:\n"
		<< "\twindows:\n"
		<< "end\n"
		<< "section: remaps\n"
		<< "\twindows:\n"
		<< "\t\tright_super.alone = hangul\n"
		<< "end\n";

	stream >> config;

	const KeyRemapConfig::TapRule* rule =
		config.get_key_remap_config().findTapRule("windows", kKeySuper_R);
	ASSERT_TRUE(rule != nullptr);
	EXPECT_EQ(kKeyHangul, rule->m_aloneID);
	EXPECT_EQ(kKeySuper_R, rule->m_holdID);
}

TEST(KeyRemapperTests, configReadsTapTargetChord)
{
	Config config;
	std::stringstream stream;
	stream
		<< "section: screens\n"
		<< "\tserver:\n"
		<< "\tmac:\n"
		<< "end\n"
		<< "section: remaps\n"
		<< "\tmac:\n"
		<< "\t\tright_alt.alone = control+space\n"
		<< "end\n";

	stream >> config;

	const KeyRemapConfig::TapRule* rule =
		config.get_key_remap_config().findTapRule("mac", kKeyAlt_R);
	ASSERT_NE(nullptr, rule);
	EXPECT_EQ(' ', rule->m_aloneID);
	EXPECT_EQ(KeyModifierControl, rule->m_aloneMask);
	EXPECT_EQ(kKeyAlt_R, rule->m_holdID);
}

TEST(KeyRemapperTests, configReadsHangulSourceForWindowsKoreanRightAlt)
{
	Config config;
	std::stringstream stream;
	stream
		<< "section: screens\n"
		<< "\tESKui-MacBookPro:\n"
		<< "end\n"
		<< "section: remaps\n"
		<< "\tESKui-MacBookPro:\n"
		<< "\t\thangul.alone = F19\n"
		<< "\t\thangul.hold = right_super\n"
		<< "end\n";

	stream >> config;

	const KeyRemapConfig::TapRule* rule =
		config.get_key_remap_config().findTapRule(
			"ESKui-MacBookPro", kKeyHangul);
	ASSERT_NE(nullptr, rule);
	EXPECT_EQ(kKeyF19, rule->m_aloneID);
	EXPECT_EQ(kKeySuper_R, rule->m_holdID);
}
