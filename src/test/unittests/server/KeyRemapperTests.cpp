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

#define BARRIER_TEST_ENV

#include "server/Config.h"
#include "server/KeyRemapper.h"

#include "test/global/gtest.h"

#include <sstream>

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
		KeyModifierSuper, kSuperButton);
	expectEvent(up[1], KeyRemapper::KeyEvent::kUp, kKeyF19,
		KeyModifierSuper, kSuperButton);
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
		config.getKeyRemapConfig().findChordRule("mac", ' ',
			KeyModifierControl);
	ASSERT_TRUE(rule != NULL);
	EXPECT_EQ(KeyModifierControl, rule->m_fromMask);
	EXPECT_EQ(' ', rule->m_fromID);
	EXPECT_EQ(0u, rule->m_toMask);
	EXPECT_EQ(kKeyF19, rule->m_toID);
}
