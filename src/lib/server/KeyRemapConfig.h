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

#pragma once

#include "barrier/key_types.h"

#include <map>
#include <string>
#include <vector>

class KeyRemapConfig {
public:
	class KeyRule {
	public:
		KeyRule();
		KeyRule(KeyID fromID, KeyID toID);

	public:
		KeyID m_fromID;
		KeyID m_toID;
	};

	class TapRule {
	public:
		TapRule();
		TapRule(KeyID fromID, KeyID aloneID, KeyID holdID);

	public:
		KeyID m_fromID;
		KeyID m_aloneID;
		KeyID m_holdID;
	};

	typedef std::vector<KeyRule> KeyRuleList;
	typedef std::vector<TapRule> TapRuleList;

	static KeyRemapConfig makeDefault();
	static std::string normalizeScreen(const std::string& screen);

	void addRule(const std::string& screen, KeyID fromID, KeyID toID);
	void addTapRule(const std::string& screen, KeyID fromID,
		KeyID aloneID, KeyID holdID);
	void clear();

	const KeyRule* findRule(const std::string& screen, KeyID id) const;
	const TapRule* findTapRule(const std::string& screen, KeyID id) const;

private:
	typedef std::map<std::string, KeyRuleList> ScreenKeyRules;
	typedef std::map<std::string, TapRuleList> ScreenTapRules;

private:
	ScreenKeyRules m_keyRules;
	ScreenTapRules m_tapRules;
};
