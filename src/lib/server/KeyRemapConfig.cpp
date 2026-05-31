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

#include "server/KeyRemapConfig.h"

#include <algorithm>
#include <cctype>

KeyRemapConfig::KeyRule::KeyRule() :
	m_fromID(kKeyNone),
	m_toID(kKeyNone)
{
}

KeyRemapConfig::KeyRule::KeyRule(KeyID fromID, KeyID toID) :
	m_fromID(fromID),
	m_toID(toID)
{
}

KeyRemapConfig::TapRule::TapRule() :
	m_fromID(kKeyNone),
	m_aloneID(kKeyNone),
	m_holdID(kKeyNone)
{
}

KeyRemapConfig::TapRule::TapRule(KeyID fromID, KeyID aloneID,
		KeyID holdID) :
	m_fromID(fromID),
	m_aloneID(aloneID),
	m_holdID(holdID)
{
}

KeyRemapConfig
KeyRemapConfig::makeDefault()
{
	KeyRemapConfig config;
	config.addRule("mac", kKeyAlt_R, kKeySuper_R);
	config.addTapRule("mac", kKeySuper_R, kKeyF19, kKeySuper_R);
	config.addRule("windows", kKeySuper_L, kKeyControl_L);
	return config;
}

std::string
KeyRemapConfig::normalizeScreen(const std::string& screen)
{
	std::string normalized(screen);
	std::transform(normalized.begin(), normalized.end(), normalized.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return normalized;
}

void
KeyRemapConfig::addRule(const std::string& screen, KeyID fromID, KeyID toID)
{
	m_keyRules[normalizeScreen(screen)].push_back(KeyRule(fromID, toID));
}

void
KeyRemapConfig::addTapRule(const std::string& screen, KeyID fromID,
		KeyID aloneID, KeyID holdID)
{
	m_tapRules[normalizeScreen(screen)].push_back(
		TapRule(fromID, aloneID, holdID));
}

void
KeyRemapConfig::clear()
{
	m_keyRules.clear();
	m_tapRules.clear();
}

const KeyRemapConfig::KeyRule*
KeyRemapConfig::findRule(const std::string& screen, KeyID id) const
{
	ScreenKeyRules::const_iterator rules =
		m_keyRules.find(normalizeScreen(screen));
	if (rules == m_keyRules.end()) {
		return NULL;
	}

	for (KeyRuleList::const_iterator rule = rules->second.begin();
			rule != rules->second.end(); ++rule) {
		if (rule->m_fromID == id) {
			return &*rule;
		}
	}

	return NULL;
}

const KeyRemapConfig::TapRule*
KeyRemapConfig::findTapRule(const std::string& screen, KeyID id) const
{
	ScreenTapRules::const_iterator rules =
		m_tapRules.find(normalizeScreen(screen));
	if (rules == m_tapRules.end()) {
		return NULL;
	}

	for (TapRuleList::const_iterator rule = rules->second.begin();
			rule != rules->second.end(); ++rule) {
		if (rule->m_fromID == id) {
			return &*rule;
		}
	}

	return NULL;
}
