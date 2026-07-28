/*
    barrier -- mouse and keyboard sharing utility
    Copyright (C) 2021 Barrier contributors

    This package is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    found in the file LICENSE that should have accompanied this file.

    This package is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "net/SecureUtils.h"

#include "test/global/gtest.h"
#include "test/global/TestUtils.h"

namespace barrier {

TEST(SecureUtilsTest, FormatSslFingerprintHexWithSeparators)
{
    auto fingerprint = generate_pseudo_random_bytes(0, 32);
    ASSERT_EQ(format_ssl_fingerprint(fingerprint, true),
              "3E:8B:B9:EE:AC:EE:2F:E8:66:19:13:F8:8E:06:38:41:"
              "F3:49:75:9C:0F:F1:62:CA:2C:2F:1A:2A:DF:69:B8:97");
}

TEST(SecureUtilsTest, CreateFingerprintRandomArt)
{
    ASSERT_EQ(create_fingerprint_randomart(generate_pseudo_random_bytes(0, 32)),
              "+-----------------+\n"
              "|    ...oo        |\n"
              "| o .  .+.        |\n"
              "|..+ .  oo.       |\n"
              "|o .oo o ..       |\n"
              "|.o o +  S        |\n"
              "|+ + o  .         |\n"
              "|.= B o  o        |\n"
              "|+.X.Eo o o       |\n"
              "|+*+***O..        |\n"
              "+-----------------+");
    ASSERT_EQ(create_fingerprint_randomart(generate_pseudo_random_bytes(1, 32)),
              "+-----------------+\n"
              "|%+=+++           |\n"
              "|o=oo=o           |\n"
              "|oo +  o          |\n"
              "|..= .+ =         |\n"
              "|o.oEo @ S        |\n"
              "| +o  @           |\n"
              "|.+ .+ .          |\n"
              "|+.oo o           |\n"
              "|oooo+            |\n"
              "+-----------------+");
    ASSERT_EQ(create_fingerprint_randomart(generate_pseudo_random_bytes(2, 32)),
              "+-----------------+\n"
              "|    .o...o*==+ +.|\n"
              "|     ..  ++Xo + .|\n"
              "|      + o + *  . |\n"
              "|     . = o + =...|\n"
              "|        S . E o.=|\n"
              "|       .   o * * |\n"
              "|        + o . *  |\n"
              "|       o o   o   |\n"
              "|        .   .    |\n"
              "+-----------------+");
}

} // namespace barrier
