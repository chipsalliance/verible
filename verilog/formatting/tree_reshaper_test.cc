// Copyright 2017-2020 The Verible Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/formatting/formatter.h"
#include "verilog/formatting/tree_unwrapper.h"

// prevent header re-ordering

#include <sstream>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/strings/position.h"
#include "common/util/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verilog/formatting/format_style.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

struct FormatterTestCase {
  absl::string_view input;
  absl::string_view expected;
  absl::string_view experimental;
};

static const verible::LineNumberSet kEnableAllLines;

namespace verilog {
namespace formatter {
namespace {

static constexpr FormatterTestCase kTestCasesUnder40[] = {
    //----------- 40 column marker --------->|
    {
     "module m;initial ffffffffffff("
     "aaaaaaaaaaaaaaaaaaaaa,bbbbbbbbbbbbbbbbbbbbb("
     "zzzzzzzzzzzzzzzzzzz(qqqqq,wwwwwwwwww,"
     "eeeeeeeeee,rrrrrr))); endmodule",
     "module m;\n"
     "  initial\n"
     "    ffffffffffff(aaaaaaaaaaaaaaaaaaaaa,\n"
     "                 bbbbbbbbbbbbbbbbbbbbb(\n"
     "                 zzzzzzzzzzzzzzzzzzz(\n"
     "                     qqqqq,\n"
     "                     wwwwwwwwww,\n"
     "                     eeeeeeeeee,\n"
     "                     rrrrrr\n"
     "                 )\n"
     "                 ));\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    ffffffffffff(\n"
     "        aaaaaaaaaaaaaaaaaaaaa,\n"
     "        bbbbbbbbbbbbbbbbbbbbb(\n"
     "            zzzzzzzzzzzzzzzzzzz(\n"
     "                qqqqq,\n"
     "                wwwwwwwwww,\n"
     "                eeeeeeeeee,\n"
     "                rrrrrr)));\n"
     "endmodule\n",
    },
    {
     "module m;initial fffff(eeeeeee,aaaaaaaa,bbbbbbbbbbbbbbb"
     "(kkkkk,gggggg(aaaaaaa,bbbbbbbb,cccccccc,ddddd(uuuuuu,"
     "iiiiiii,yyyyyyyyy,tttttttttt),eeeeeeee),iiiiiiiiiii),"
     "cccccccc,ddddddddd,eeeeeeeeee,fffffffffff(uuuuuuu,"
     "aaaaaaaaaa,cccccccccc,dddddddd),gggggg); endmodule",
     "module m;\n"
     "  initial\n"
     "    fffff(eeeeeee, aaaaaaaa,\n"
     "          bbbbbbbbbbbbbbb(\n"
     "          kkkkk,\n"
     "          gggggg(\n"
     "              aaaaaaa,\n"
     "              bbbbbbbb,\n"
     "              cccccccc,\n"
     "              ddddd(\n"
     "                  uuuuuu,\n"
     "                  iiiiiii,\n"
     "                  yyyyyyyyy,\n"
     "                  tttttttttt\n"
     "              ),\n"
     "              eeeeeeee\n"
     "          ),\n"
     "          iiiiiiiiiii\n"
     "          ), cccccccc, ddddddddd,\n"
     "          eeeeeeeeee, fffffffffff(\n"
     "          uuuuuuu,\n"
     "          aaaaaaaaaa,\n"
     "          cccccccccc,\n"
     "          dddddddd\n"
     "          ), gggggg);\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    fffff(\n"
     "        eeeeeee, aaaaaaaa,\n"
     "        bbbbbbbbbbbbbbb(\n"
     "            kkkkk,\n"
     "            gggggg(\n"
     "                aaaaaaa,\n"
     "                bbbbbbbb,\n"
     "                cccccccc,\n"
     "                ddddd(\n"
     "                    uuuuuu,\n"
     "                    iiiiiii,\n"
     "                    yyyyyyyyy,\n"
     "                    tttttttttt),\n"
     "                eeeeeeee),\n"
     "            iiiiiiiiiii), cccccccc,\n"
     "        ddddddddd, eeeeeeeeee,\n"
     "        fffffffffff(\n"
     "            uuuuuuu, aaaaaaaaaa,\n"
     "            cccccccccc,\n"
     "            dddddddd), gggggg);\n"
     "endmodule\n"
    },
    {
     "module m; initial ffff(aaaaaa(sssss,aaaaa,vvvvv,uuuuu),"
     "bbbbb,ccccc,dddd); endmodule",
     "module m;\n"
     "  initial\n"
     "    ffff(aaaaaa(\n"
     "         sssss, aaaaa, vvvvv, uuuuu),\n"
     "         bbbbb, ccccc, dddd);\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    ffff(aaaaaa(\n"
     "             sssss, aaaaa,\n"
     "             vvvvv, uuuuu),\n"
     "         bbbbb, ccccc, dddd);\n"
     "endmodule\n"
    },
    {
     "module m; initial fff(aaaaaa(sssss(kkkkkkkk,mm(yyy,cc),"
     "nnnnnnn,ooooo),xx(w,e,qq),vvvvv,uuuuu),bbbbb,ccccc,dddd); endmodule",
     "module m;\n"
     "  initial\n"
     "    fff(aaaaaa(\n"
     "        sssss(\n"
     "            kkkkkkkk,\n"
     "            mm(\n"
     "                yyy, cc\n"
     "            ),\n"
     "            nnnnnnn,\n"
     "            ooooo\n"
     "        ),\n"
     "        xx(\n"
     "            w, e, qq\n"
     "        ),\n"
     "        vvvvv,\n"
     "        uuuuu\n"
     "        ), bbbbb, ccccc, dddd);\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    fff(\n"
     "        aaaaaa(\n"
     "            sssss(\n"
     "                kkkkkkkk,\n"
     "                mm(\n"
     "                    yyy,\n"
     "                    cc),\n"
     "                nnnnnnn,\n"
     "                ooooo),\n"
     "            xx(\n"
     "                w,\n"
     "                e, qq),\n"
     "            vvvvv, uuuuu),\n"
     "        bbbbb, ccccc, dddd);\n"
     "endmodule\n"
    },
    {
     "module m; initial fffffffff(aaaaaa(sssss(kkkkkkkk,"
     "mm(yyy,iiiiiiiiiiiiiiiiiiiiiiiiii(nnnn,xxxx,ddddd,"
     "xxxxx),cc),nnnnnnn,ooooo),xxxxxxxxxxxx(wwwwwwwwww,"
     "eeeeeeeee,qq),vvvvv,uuuuu),bbbbb,ccccc,dddd); endmodule",
     "module m;\n"
     "  initial\n"
     "    fffffffff(aaaaaa(\n"
     "              sssss(\n"
     "                  kkkkkkkk,\n"
     "                  mm(\n"
     "                      yyy,\n"
     "                      iiiiiiiiiiiiiiiiiiiiiiiiii\n"
     "                          (\n"
     "                          nnnn,\n"
     "                          xxxx,\n"
     "                          ddddd,\n"
     "                          xxxxx\n"
     "                      ),\n"
     "                      cc\n"
     "                  ),\n"
     "                  nnnnnnn,\n"
     "                  ooooo\n"
     "              ),\n"
     "              xxxxxxxxxxxx(\n"
     "                  wwwwwwwwww,\n"
     "                  eeeeeeeee,\n"
     "                  qq\n"
     "              ),\n"
     "              vvvvv,\n"
     "              uuuuu\n"
     "              ), bbbbb, ccccc, dddd);\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    fffffffff(\n"
     "        aaaaaa(\n"
     "            sssss(\n"
     "                kkkkkkkk,\n"
     "                mm(\n"
     "                    yyy,\n"
     "                    iiiiiiiiiiiiiiiiiiiiiiiiii\n"
     "                        (\n"
     "                        nnnn,\n"
     "                        xxxx,\n"
     "                        ddddd,\n"
     "                        xxxxx),\n"
     "                    cc),\n"
     "                nnnnnnn,\n"
     "                ooooo),\n"
     "            xxxxxxxxxxxx(\n"
     "                wwwwwwwwww,\n"
     "                eeeeeeeee,\n"
     "                qq),\n"
     "            vvvvv, uuuuu),\n"
     "        bbbbb, ccccc, dddd);\n"
     "endmodule\n"
    },
    {
     "module m; initial fffffffff(aaaaaa(sssss(kkkkkkkk,"
     "mm(yyy,iiiiiiiiiiiii(nn(j,k,l),xxxx,ddddd,xxxxx),cc),"
     "nnnnnnn,ooooo),xxxxxxxxxxxx(wwwwwwwwww,eeeeeeeee,qq),"
     "vvvvv,uuuuu),bbbbb,ccccc,dddd); endmodule",
     "module m;\n"
     "  initial\n"
     "    fffffffff(aaaaaa(\n"
     "              sssss(\n"
     "                  kkkkkkkk,\n"
     "                  mm(\n"
     "                      yyy,\n"
     "                      iiiiiiiiiiiii(\n"
     "                          nn(\n"
     "                              j, k, l\n"
     "                          ),\n"
     "                          xxxx,\n"
     "                          ddddd,\n"
     "                          xxxxx\n"
     "                      ),\n"
     "                      cc\n"
     "                  ),\n"
     "                  nnnnnnn,\n"
     "                  ooooo\n"
     "              ),\n"
     "              xxxxxxxxxxxx(\n"
     "                  wwwwwwwwww,\n"
     "                  eeeeeeeee,\n"
     "                  qq\n"
     "              ),\n"
     "              vvvvv,\n"
     "              uuuuu\n"
     "              ), bbbbb, ccccc, dddd);\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    fffffffff(\n"
     "        aaaaaa(\n"
     "            sssss(\n"
     "                kkkkkkkk,\n"
     "                mm(\n"
     "                    yyy,\n"
     "                    iiiiiiiiiiiii(\n"
     "                        nn(\n"
     "                            j,\n"
     "                            k,\n"
     "                            l),\n"
     "                        xxxx,\n"
     "                        ddddd,\n"
     "                        xxxxx),\n"
     "                    cc),\n"
     "                nnnnnnn,\n"
     "                ooooo),\n"
     "            xxxxxxxxxxxx(\n"
     "                wwwwwwwwww,\n"
     "                eeeeeeeee,\n"
     "                qq),\n"
     "            vvvvv, uuuuu),\n"
     "        bbbbb, ccccc, dddd);\n"
     "endmodule\n"
    },
    {
     "module m; initial ffffffffffff(aaaaaaaaaaaaaaaaaaaaa,"
     "bbbbbbbbbbbbbbbbbbbbb(zzzzzzzzzzzzzzzzzzz(qqqqq,wwwwwwwwww,"
     "eeeeeeeeee,rrrrrr),xxxxxxxxxxxxxxxxx,yyyyyyyyyyyyyyyy,"
     "uuuuuuuu(iiiii,jjjjj,kkkkkkk,tttttt)),ccccccccc,dddddddddd,"
     "eeeeeeeeeee,ffffffffffffff(aaaa,bbb,ccc,dddddd(aaa,bbb,cc,"
     "ddd,ee(aaaaa,bbbbb,ccccc(aaa,bbb,ccccc,eeee),dddd,eeee),ffff,"
     "ggg,hhh,iiiii,kkkk,aaaaa,bbbbbbbbbbbbbbbbbb(uuuuuuuuuuuuu,"
     "xxxxxxxxxxxxxxx,uuuuuuuuuuuuu(xxxxxxxxxxxxxxx,xxxxxxxxxx,"
     "xxxxxxxx(uuuuu,yyy,zzz,sss,eeeeeeeee(aaaaa,bbbb,cc,dddd,ee,"
     "ffff),eee,ss,aaa)),xxx)))); endmodule",
     "module m;\n"
     "  initial\n"
     "    ffffffffffff(\n"
     "        aaaaaaaaaaaaaaaaaaaaa,\n"
     "        bbbbbbbbbbbbbbbbbbbbb(\n"
     "        zzzzzzzzzzzzzzzzzzz(\n"
     "            qqqqq,\n"
     "            wwwwwwwwww,\n"
     "            eeeeeeeeee,\n"
     "            rrrrrr\n"
     "        ),\n"
     "        xxxxxxxxxxxxxxxxx,\n"
     "        yyyyyyyyyyyyyyyy,\n"
     "        uuuuuuuu(\n"
     "            iiiii,\n"
     "            jjjjj,\n"
     "            kkkkkkk,\n"
     "            tttttt\n"
     "        )\n"
     "        ), ccccccccc, dddddddddd,\n"
     "        eeeeeeeeeee, ffffffffffffff(\n"
     "        aaaa,\n"
     "        bbb,\n"
     "        ccc,\n"
     "        dddddd(\n"
     "            aaa,\n"
     "            bbb,\n"
     "            cc,\n"
     "            ddd,\n"
     "            ee(\n"
     "                aaaaa,\n"
     "                bbbbb,\n"
     "                ccccc(\n"
     "                    aaa,\n"
     "                    bbb,\n"
     "                    ccccc,\n"
     "                    eeee\n"
     "                ),\n"
     "                dddd,\n"
     "                eeee\n"
     "            ),\n"
     "            ffff,\n"
     "            ggg,\n"
     "            hhh,\n"
     "            iiiii,\n"
     "            kkkk,\n"
     "            aaaaa,\n"
     "            bbbbbbbbbbbbbbbbbb(\n"
     "                uuuuuuuuuuuuu,\n"
     "                xxxxxxxxxxxxxxx,\n"
     "                uuuuuuuuuuuuu(\n"
     "                    xxxxxxxxxxxxxxx,\n"
     "                    xxxxxxxxxx,\n"
     "                    xxxxxxxx(\n"
     "                        uuuuu,\n"
     "                        yyy,\n"
     "                        zzz,\n"
     "                        sss,\n"
     "                        eeeeeeeee(\n"
     "                            aaaaa,\n"
     "                            bbbb,\n"
     "                            cc,\n"
     "                            dddd,\n"
     "                            ee,\n"
     "                            ffff\n"
     "                        ),\n"
     "                        eee,\n"
     "                        ss,\n"
     "                        aaa\n"
     "                    )\n"
     "                ),\n"
     "                xxx\n"
     "            )\n"
     "        )\n"
     "        ));\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    ffffffffffff(\n"
     "        aaaaaaaaaaaaaaaaaaaaa,\n"
     "        bbbbbbbbbbbbbbbbbbbbb(\n"
     "            zzzzzzzzzzzzzzzzzzz(\n"
     "                qqqqq,\n"
     "                wwwwwwwwww,\n"
     "                eeeeeeeeee,\n"
     "                rrrrrr),\n"
     "            xxxxxxxxxxxxxxxxx,\n"
     "            yyyyyyyyyyyyyyyy,\n"
     "            uuuuuuuu(\n"
     "                iiiii,\n"
     "                jjjjj,\n"
     "                kkkkkkk,\n"
     "                tttttt)), ccccccccc,\n"
     "        dddddddddd, eeeeeeeeeee,\n"
     "        ffffffffffffff(\n"
     "            aaaa,\n"
     "            bbb, ccc,\n"
     "            dddddd(\n"
     "                aaa, bbb,\n"
     "                cc, ddd,\n"
     "                ee(\n"
     "                    aaaaa,\n"
     "                    bbbbb,\n"
     "                    ccccc(\n"
     "                        aaa,\n"
     "                        bbb,\n"
     "                        ccccc,\n"
     "                        eeee),\n"
     "                    dddd,\n"
     "                    eeee),\n"
     "                ffff, ggg,\n"
     "                hhh, iiiii,\n"
     "                kkkk, aaaaa,\n"
     "                bbbbbbbbbbbbbbbbbb(\n"
     "                    uuuuuuuuuuuuu,\n"
     "                    xxxxxxxxxxxxxxx,\n"
     "                    uuuuuuuuuuuuu(\n"
     "                        xxxxxxxxxxxxxxx,\n"
     "                        xxxxxxxxxx,\n"
     "                        xxxxxxxx(\n"
     "                            uuuuu,\n"
     "                            yyy,\n"
     "                            zzz,\n"
     "                            sss,\n"
     "                            eeeeeeeee(\n"
     "                                aaaaa,\n"
     "                                bbbb,\n"
     "                                cc,\n"
     "                                dddd,\n"
     "                                ee,\n"
     "                                ffff),\n"
     "                            eee,\n"
     "                            ss,\n"
     "                            aaa)),\n"
     "                    xxx))));\n"
     "endmodule\n"
    },
    {
     "module m; assign aa = ffffffffffffffffffffffffff(aaaaaaaaaaaaaaaaaaaaaaa,"
     "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb,cccccccccccccccccccccccccccccccccc,"
     "ddddddddddddddddddddddddddddddddd,eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee,"
     "fffffffffffffffffffffffffffffffff,gggggggggggggggggggggggggggggggggggggg,"
     "hhhhhhhhhhhhhhhhhhhhhhhhhhhh)+hhhhhhhhhhhhhhhhhhhhh(aaaaaaaaaaaaaaaaaaaaaaaa,"
     "bbbbbbbbbbbbbbbbbbbbbbbb,cccccccccccccccccccc+xxxxxxxxxxxxxxxxx+"
     "zzzzzzzzzzzzzzzzzz+yyyyyyyyyyyyyyyyyyyyyy+ttttttttttttttttttttt,"
     "ddddddddddddddddddddddddd); endmodule",
     "module m;\n"
     "  assign\n"
     "      aa = ffffffffffffffffffffffffff(\n"
     "      aaaaaaaaaaaaaaaaaaaaaaa,\n"
     "      bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb,\n"
     "      cccccccccccccccccccccccccccccccccc\n"
     "          ,\n"
     "      ddddddddddddddddddddddddddddddddd,\n"
     "      eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee,\n"
     "      fffffffffffffffffffffffffffffffff,\n"
     "      gggggggggggggggggggggggggggggggggggggg\n"
     "          ,\n"
     "      hhhhhhhhhhhhhhhhhhhhhhhhhhhh\n"
     "  ) + hhhhhhhhhhhhhhhhhhhhh(\n"
     "      aaaaaaaaaaaaaaaaaaaaaaaa,\n"
     "      bbbbbbbbbbbbbbbbbbbbbbbb,\n"
     "      cccccccccccccccccccc +\n"
     "          xxxxxxxxxxxxxxxxx +\n"
     "          zzzzzzzzzzzzzzzzzz +\n"
     "          yyyyyyyyyyyyyyyyyyyyyy +\n"
     "          ttttttttttttttttttttt,\n"
     "      ddddddddddddddddddddddddd\n"
     "  );\n"
     "endmodule\n",
     "module m;\n"
     "  assign aa =\n"
     "      ffffffffffffffffffffffffff(\n"
     "          aaaaaaaaaaaaaaaaaaaaaaa,\n"
     "          bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n"
     "              ,\n"
     "          cccccccccccccccccccccccccccccccccc\n"
     "              ,\n"
     "          ddddddddddddddddddddddddddddddddd\n"
     "              ,\n"
     "          eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee\n"
     "              ,\n"
     "          fffffffffffffffffffffffffffffffff\n"
     "              ,\n"
     "          gggggggggggggggggggggggggggggggggggggg\n"
     "              ,\n"
     "          hhhhhhhhhhhhhhhhhhhhhhhhhhhh\n"
     "              ) +\n"
     "      hhhhhhhhhhhhhhhhhhhhh(\n"
     "          aaaaaaaaaaaaaaaaaaaaaaaa,\n"
     "          bbbbbbbbbbbbbbbbbbbbbbbb,\n"
     "          cccccccccccccccccccc +\n"
     "          xxxxxxxxxxxxxxxxx +\n"
     "          zzzzzzzzzzzzzzzzzz +\n"
     "          yyyyyyyyyyyyyyyyyyyyyy +\n"
     "          ttttttttttttttttttttt,\n"
     "          ddddddddddddddddddddddddd);\n"
     "endmodule\n"
    },
    {
     "module foo;"
     " assign a = b + c + d + e_call(aaa,bbb+ccc+ddd,eee,fff,ggg) + f + g + h;"
     "endmodule",
     "module foo;\n"
     "  assign a = b + c + d + e_call(\n"
     "      aaa,\n"
     "      bbb + ccc + ddd,\n"
     "      eee,\n"
     "      fff,\n"
     "      ggg\n"
     "  ) + f + g + h;\n"
     "endmodule\n",
     "module foo;\n"
     "  assign a = b + c +\n"
     "             d + e_call(aaa,\n"
     "                        bbb + ccc + ddd,\n"
     "                        eee, fff, ggg) +\n"
     "             f + g + h;\n"
     "endmodule\n"
    },
    {
     "module foo;"
     "assign aaaaa = bbbbbbbbbbbb + cccccccccccccccccc + dddddddddddddddddd +"
     "eeeeeeeeeeeee_call(aaaaaaaaaa,bbbbbbbbbbbb+cccccccccccc+ddddddddddd,"
     "eeeeeeeeeeeee,fffffffffffffff,gggggggggggggg) +"
     " ffffffffffffff + ggggggggggggggg + hhhhhhhhhhhhhhhh;"
     "endmodule",
     "module foo;\n"
     "  assign aaaaa =\n"
     "      bbbbbbbbbbbb + cccccccccccccccccc\n"
     "      + dddddddddddddddddd +\n"
     "      eeeeeeeeeeeee_call(\n"
     "      aaaaaaaaaa,\n"
     "      bbbbbbbbbbbb + cccccccccccc +\n"
     "          ddddddddddd,\n"
     "      eeeeeeeeeeeee,\n"
     "      fffffffffffffff,\n"
     "      gggggggggggggg\n"
     "  ) + ffffffffffffff + ggggggggggggggg +\n"
     "      hhhhhhhhhhhhhhhh;\n"
     "endmodule\n",
     "module foo;\n"
     "  assign aaaaa =\n"
     "      bbbbbbbbbbbb +\n"
     "      cccccccccccccccccc +\n"
     "      dddddddddddddddddd +\n"
     "      eeeeeeeeeeeee_call(\n"
     "          aaaaaaaaaa,\n"
     "          bbbbbbbbbbbb +\n"
     "          cccccccccccc +\n"
     "          ddddddddddd,\n"
     "          eeeeeeeeeeeee,\n"
     "          fffffffffffffff,\n"
     "          gggggggggggggg) +\n"
     "      ffffffffffffff +\n"
     "      ggggggggggggggg +\n"
     "      hhhhhhhhhhhhhhhh;\n"
     "endmodule\n"
    },
};

TEST(FormatterEndToEndTest, OptimalFormatterUnder40TestCases) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  for (const auto& test_case : kTestCasesUnder40) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    {
      std::ostringstream stream, debug_stream;
      ExecutionControl control;
      control.stream = &debug_stream;
      const auto status = FormatVerilog(test_case.input, "<filename>", style,
                                        stream, kEnableAllLines, control);
      EXPECT_OK(status);
      EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
      EXPECT_TRUE(debug_stream.str().empty());
    }
    {
      std::ostringstream stream, debug_stream;
      ExecutionControl control;
      control.stream = &debug_stream;
      auto experimental_style = style;
      experimental_style.enable_experimental_tree_reshaper = true;
      const auto status = FormatVerilog(test_case.input, "<filename>",
                                        experimental_style,
                                        stream, kEnableAllLines, control);
      EXPECT_OK(status);
      EXPECT_EQ(stream.str(), test_case.experimental) << "code:\n" << test_case.input;
      EXPECT_TRUE(debug_stream.str().empty());
    }
  }
}

static constexpr FormatterTestCase kTestCasesUnder60[] = {
    //--------------------- 60 column marker ------------------->|
    {
     "module m;initial ffffffffffff("
     "aaaaaaaaaaaaaaaaaaaaa,bbbbbbbbbbbbbbbbbbbbb("
     "zzzzzzzzzzzzzzzzzzz(qqqqq,wwwwwwwwww,"
     "eeeeeeeeee,rrrrrr))); endmodule",
     "module m;\n"
     "  initial\n"
     "    ffffffffffff(aaaaaaaaaaaaaaaaaaaaa,\n"
     "                 bbbbbbbbbbbbbbbbbbbbb(\n"
     "                 zzzzzzzzzzzzzzzzzzz(\n"
     "                     qqqqq, wwwwwwwwww, eeeeeeeeee, rrrrrr\n"
     "                 )\n"
     "                 ));\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    ffffffffffff(aaaaaaaaaaaaaaaaaaaaa,\n"
     "                 bbbbbbbbbbbbbbbbbbbbb(\n"
     "                     zzzzzzzzzzzzzzzzzzz(\n"
     "                         qqqqq, wwwwwwwwww,\n"
     "                         eeeeeeeeee, rrrrrr)));\n"
     "endmodule\n"
    },
    {
     "module m;initial fffff(eeeeeee,aaaaaaaa,bbbbbbbbbbbbbbb"
     "(kkkkk,gggggg(aaaaaaa,bbbbbbbb,cccccccc,ddddd(uuuuuu,"
     "iiiiiii,yyyyyyyyy,tttttttttt),eeeeeeee),iiiiiiiiiii),"
     "cccccccc,ddddddddd,eeeeeeeeee,fffffffffff(uuuuuuu,"
     "aaaaaaaaaa,cccccccccc,dddddddd),gggggg); endmodule",
     "module m;\n"
     "  initial\n"
     "    fffff(eeeeeee, aaaaaaaa, bbbbbbbbbbbbbbb(\n"
     "          kkkkk,\n"
     "          gggggg(\n"
     "              aaaaaaa,\n"
     "              bbbbbbbb,\n"
     "              cccccccc,\n"
     "              ddddd(\n"
     "                  uuuuuu, iiiiiii, yyyyyyyyy, tttttttttt\n"
     "              ),\n"
     "              eeeeeeee\n"
     "          ),\n"
     "          iiiiiiiiiii\n"
     "          ), cccccccc, ddddddddd, eeeeeeeeee, fffffffffff(\n"
     "          uuuuuuu, aaaaaaaaaa, cccccccccc, dddddddd),\n"
     "          gggggg);\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    fffff(\n"
     "        eeeeeee, aaaaaaaa,\n"
     "        bbbbbbbbbbbbbbb(\n"
     "            kkkkk,\n"
     "            gggggg(\n"
     "                aaaaaaa,\n"
     "                bbbbbbbb, cccccccc,\n"
     "                ddddd(\n"
     "                    uuuuuu, iiiiiii,\n"
     "                    yyyyyyyyy, tttttttttt),\n"
     "                eeeeeeee), iiiiiiiiiii),\n"
     "        cccccccc, ddddddddd, eeeeeeeeee,\n"
     "        fffffffffff(uuuuuuu, aaaaaaaaaa,\n"
     "                    cccccccccc, dddddddd),\n"
     "        gggggg);\n"
     "endmodule\n"
    },
    {
     "module m; initial ffff(aaaaaa(sssss,aaaaa,vvvvv,uuuuu),"
     "bbbbb,ccccc,dddd); endmodule",
     "module m;\n"
     "  initial\n"
     "    ffff(aaaaaa(sssss, aaaaa, vvvvv, uuuuu), bbbbb, ccccc,\n"
     "         dddd);\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    ffff(\n"
     "        aaaaaa(sssss, aaaaa, vvvvv, uuuuu),\n"
     "        bbbbb, ccccc, dddd);\n"
     "endmodule\n"
    },
    {
     "module m; initial fff(aaaaaa(sssss(kkkkkkkk,mm(yyy,cc),"
     "nnnnnnn,ooooo),xx(w,e,qq),vvvvv,uuuuu),bbbbb,ccccc,dddd); endmodule",
     "module m;\n"
     "  initial\n"
     "    fff(aaaaaa(\n"
     "        sssss(\n"
     "            kkkkkkkk, mm(yyy, cc), nnnnnnn, ooooo\n"
     "        ),\n"
     "        xx(\n"
     "            w, e, qq\n"
     "        ),\n"
     "        vvvvv,\n"
     "        uuuuu\n"
     "        ), bbbbb, ccccc, dddd);\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    fff(aaaaaa(sssss(kkkkkkkk, mm(yyy, cc), nnnnnnn, ooooo),\n"
     "               xx(w, e, qq), vvvvv, uuuuu),\n"
     "        bbbbb, ccccc, dddd);\n"
     "endmodule\n"
    },
    {
     "module m; initial fffffffff(aaaaaa(sssss(kkkkkkkk,"
     "mm(yyy,iiiiiiiiiiiiiiiiiiiiiiiiii(nnnn,xxxx,ddddd,"
     "xxxxx),cc),nnnnnnn,ooooo),xxxxxxxxxxxx(wwwwwwwwww,"
     "eeeeeeeee,qq),vvvvv,uuuuu),bbbbb,ccccc,dddd); endmodule",
     "module m;\n"
     "  initial\n"
     "    fffffffff(aaaaaa(\n"
     "              sssss(\n"
     "                  kkkkkkkk,\n"
     "                  mm(\n"
     "                      yyy,\n"
     "                      iiiiiiiiiiiiiiiiiiiiiiiiii(\n"
     "                          nnnn, xxxx, ddddd, xxxxx\n"
     "                      ),\n"
     "                      cc\n"
     "                  ),\n"
     "                  nnnnnnn,\n"
     "                  ooooo\n"
     "              ),\n"
     "              xxxxxxxxxxxx(\n"
     "                  wwwwwwwwww, eeeeeeeee, qq\n"
     "              ),\n"
     "              vvvvv,\n"
     "              uuuuu\n"
     "              ), bbbbb, ccccc, dddd);\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    fffffffff(aaaaaa(sssss(kkkkkkkk,\n"
     "                           mm(yyy,\n"
     "                              iiiiiiiiiiiiiiiiiiiiiiiiii(\n"
     "                                  nnnn, xxxx,\n"
     "                                  ddddd, xxxxx), cc),\n"
     "                           nnnnnnn, ooooo),\n"
     "                     xxxxxxxxxxxx(\n"
     "                         wwwwwwwwww, eeeeeeeee, qq),\n"
     "                     vvvvv, uuuuu), bbbbb, ccccc, dddd);\n"
     "endmodule\n"
    },
    {
     "module m; initial fffffffff(aaaaaa(sssss(kkkkkkkk,"
     "mm(yyy,iiiiiiiiiiiii(nn(j,k,l),xxxx,ddddd,xxxxx),cc),"
     "nnnnnnn,ooooo),xxxxxxxxxxxx(wwwwwwwwww,eeeeeeeee,qq),"
     "vvvvv,uuuuu),bbbbb,ccccc,dddd); endmodule",
     "module m;\n"
     "  initial\n"
     "    fffffffff(aaaaaa(\n"
     "              sssss(\n"
     "                  kkkkkkkk,\n"
     "                  mm(\n"
     "                      yyy,\n"
     "                      iiiiiiiiiiiii(\n"
     "                          nn(j, k, l), xxxx, ddddd, xxxxx\n"
     "                      ),\n"
     "                      cc\n"
     "                  ),\n"
     "                  nnnnnnn,\n"
     "                  ooooo\n"
     "              ),\n"
     "              xxxxxxxxxxxx(\n"
     "                  wwwwwwwwww, eeeeeeeee, qq\n"
     "              ),\n"
     "              vvvvv,\n"
     "              uuuuu\n"
     "              ), bbbbb, ccccc, dddd);\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    fffffffff(aaaaaa(sssss(kkkkkkkk, mm(yyy,\n"
     "                                        iiiiiiiiiiiii(\n"
     "                                            nn(j, k,\n"
     "                                               l),\n"
     "                                            xxxx, ddddd,\n"
     "                                            xxxxx), cc),\n"
     "                           nnnnnnn, ooooo),\n"
     "                     xxxxxxxxxxxx(\n"
     "                         wwwwwwwwww, eeeeeeeee, qq),\n"
     "                     vvvvv, uuuuu), bbbbb, ccccc, dddd);\n"
     "endmodule\n"
    },
    {
     "module m; initial ffffffffffff(aaaaaaaaaaaaaaaaaaaaa,"
     "bbbbbbbbbbbbbbbbbbbbb(zzzzzzzzzzzzzzzzzzz(qqqqq,wwwwwwwwww,"
     "eeeeeeeeee,rrrrrr),xxxxxxxxxxxxxxxxx,yyyyyyyyyyyyyyyy,"
     "uuuuuuuu(iiiii,jjjjj,kkkkkkk,tttttt)),ccccccccc,dddddddddd,"
     "eeeeeeeeeee,ffffffffffffff(aaaa,bbb,ccc,dddddd(aaa,bbb,cc,"
     "ddd,ee(aaaaa,bbbbb,ccccc(aaa,bbb,ccccc,eeee),dddd,eeee),ffff,"
     "ggg,hhh,iiiii,kkkk,aaaaa,bbbbbbbbbbbbbbbbbb(uuuuuuuuuuuuu,"
     "xxxxxxxxxxxxxxx,uuuuuuuuuuuuu(xxxxxxxxxxxxxxx,xxxxxxxxxx,"
     "xxxxxxxx(uuuuu,yyy,zzz,sss,eeeeeeeee(aaaaa,bbbb,cc,dddd,ee,"
     "ffff),eee,ss,aaa)),xxx)))); endmodule",
     "module m;\n"
     "  initial\n"
     "    ffffffffffff(aaaaaaaaaaaaaaaaaaaaa,\n"
     "                 bbbbbbbbbbbbbbbbbbbbb(\n"
     "                 zzzzzzzzzzzzzzzzzzz(\n"
     "                     qqqqq, wwwwwwwwww, eeeeeeeeee, rrrrrr\n"
     "                 ),\n"
     "                 xxxxxxxxxxxxxxxxx,\n"
     "                 yyyyyyyyyyyyyyyy,\n"
     "                 uuuuuuuu(\n"
     "                     iiiii, jjjjj, kkkkkkk, tttttt\n"
     "                 )\n"
     "                 ), ccccccccc, dddddddddd, eeeeeeeeeee,\n"
     "                 ffffffffffffff(\n"
     "                 aaaa,\n"
     "                 bbb,\n"
     "                 ccc,\n"
     "                 dddddd(\n"
     "                     aaa,\n"
     "                     bbb,\n"
     "                     cc,\n"
     "                     ddd,\n"
     "                     ee(\n"
     "                         aaaaa,\n"
     "                         bbbbb,\n"
     "                         ccccc(\n"
     "                             aaa, bbb, ccccc, eeee\n"
     "                         ),\n"
     "                         dddd,\n"
     "                         eeee\n"
     "                     ),\n"
     "                     ffff,\n"
     "                     ggg,\n"
     "                     hhh,\n"
     "                     iiiii,\n"
     "                     kkkk,\n"
     "                     aaaaa,\n"
     "                     bbbbbbbbbbbbbbbbbb(\n"
     "                         uuuuuuuuuuuuu,\n"
     "                         xxxxxxxxxxxxxxx,\n"
     "                         uuuuuuuuuuuuu(\n"
     "                             xxxxxxxxxxxxxxx,\n"
     "                             xxxxxxxxxx,\n"
     "                             xxxxxxxx(\n"
     "                                 uuuuu,\n"
     "                                 yyy,\n"
     "                                 zzz,\n"
     "                                 sss,\n"
     "                                 eeeeeeeee(\n"
     "                                     aaaaa,\n"
     "                                     bbbb,\n"
     "                                     cc,\n"
     "                                     dddd,\n"
     "                                     ee,\n"
     "                                     ffff\n"
     "                                 ),\n"
     "                                 eee,\n"
     "                                 ss,\n"
     "                                 aaa\n"
     "                             )\n"
     "                         ),\n"
     "                         xxx\n"
     "                     )\n"
     "                 )\n"
     "                 ));\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    ffffffffffff(\n"
     "        aaaaaaaaaaaaaaaaaaaaa,\n"
     "        bbbbbbbbbbbbbbbbbbbbb(\n"
     "            zzzzzzzzzzzzzzzzzzz(\n"
     "                qqqqq, wwwwwwwwww,\n"
     "                eeeeeeeeee, rrrrrr),\n"
     "            xxxxxxxxxxxxxxxxx, yyyyyyyyyyyyyyyy,\n"
     "            uuuuuuuu(iiiii, jjjjj,\n"
     "                     kkkkkkk, tttttt)),\n"
     "        ccccccccc, dddddddddd, eeeeeeeeeee,\n"
     "        ffffffffffffff(\n"
     "            aaaa, bbb, ccc,\n"
     "            dddddd(\n"
     "                aaa,\n"
     "                bbb, cc, ddd,\n"
     "                ee(\n"
     "                    aaaaa, bbbbb,\n"
     "                    ccccc(\n"
     "                        aaa, bbb,\n"
     "                        ccccc, eeee),\n"
     "                    dddd, eeee),\n"
     "                ffff, ggg, hhh,\n"
     "                iiiii, kkkk, aaaaa,\n"
     "                bbbbbbbbbbbbbbbbbb(\n"
     "                    uuuuuuuuuuuuu,\n"
     "                    xxxxxxxxxxxxxxx,\n"
     "                    uuuuuuuuuuuuu(\n"
     "                        xxxxxxxxxxxxxxx,\n"
     "                        xxxxxxxxxx,\n"
     "                        xxxxxxxx(\n"
     "                            uuuuu,\n"
     "                            yyy,\n"
     "                            zzz,\n"
     "                            sss,\n"
     "                            eeeeeeeee(\n"
     "                                aaaaa,\n"
     "                                bbbb,\n"
     "                                cc,\n"
     "                                dddd,\n"
     "                                ee,\n"
     "                                ffff),\n"
     "                            eee, ss,\n"
     "                            aaa)), xxx))));\n"
     "endmodule\n"
    },
    {
     "module m; assign aa = ffffffffffffffffffffffffff(aaaaaaaaaaaaaaaaaaaaaaa,"
     "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb,cccccccccccccccccccccccccccccccccc,"
     "ddddddddddddddddddddddddddddddddd,eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee,"
     "fffffffffffffffffffffffffffffffff,gggggggggggggggggggggggggggggggggggggg,"
     "hhhhhhhhhhhhhhhhhhhhhhhhhhhh)+hhhhhhhhhhhhhhhhhhhhh(aaaaaaaaaaaaaaaaaaaaaaaa,"
     "bbbbbbbbbbbbbbbbbbbbbbbb,cccccccccccccccccccc+xxxxxxxxxxxxxxxxx+"
     "zzzzzzzzzzzzzzzzzz+yyyyyyyyyyyyyyyyyyyyyy+ttttttttttttttttttttt,"
     "ddddddddddddddddddddddddd); endmodule",
     "module m;\n"
     "  assign aa = ffffffffffffffffffffffffff(\n"
     "      aaaaaaaaaaaaaaaaaaaaaaa,\n"
     "      bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb,\n"
     "      cccccccccccccccccccccccccccccccccc,\n"
     "      ddddddddddddddddddddddddddddddddd,\n"
     "      eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee,\n"
     "      fffffffffffffffffffffffffffffffff,\n"
     "      gggggggggggggggggggggggggggggggggggggg,\n"
     "      hhhhhhhhhhhhhhhhhhhhhhhhhhhh\n"
     "  ) + hhhhhhhhhhhhhhhhhhhhh(\n"
     "      aaaaaaaaaaaaaaaaaaaaaaaa,\n"
     "      bbbbbbbbbbbbbbbbbbbbbbbb,\n"
     "      cccccccccccccccccccc + xxxxxxxxxxxxxxxxx +\n"
     "          zzzzzzzzzzzzzzzzzz + yyyyyyyyyyyyyyyyyyyyyy +\n"
     "          ttttttttttttttttttttt,\n"
     "      ddddddddddddddddddddddddd\n"
     "  );\n"
     "endmodule\n",
     "module m;\n"
     "  assign aa =\n"
     "      ffffffffffffffffffffffffff(\n"
     "          aaaaaaaaaaaaaaaaaaaaaaa,\n"
     "          bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb,\n"
     "          cccccccccccccccccccccccccccccccccc,\n"
     "          ddddddddddddddddddddddddddddddddd,\n"
     "          eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee,\n"
     "          fffffffffffffffffffffffffffffffff,\n"
     "          gggggggggggggggggggggggggggggggggggggg,\n"
     "          hhhhhhhhhhhhhhhhhhhhhhhhhhhh) +\n"
     "      hhhhhhhhhhhhhhhhhhhhh(\n"
     "          aaaaaaaaaaaaaaaaaaaaaaaa,\n"
     "          bbbbbbbbbbbbbbbbbbbbbbbb,\n"
     "          cccccccccccccccccccc +\n"
     "          xxxxxxxxxxxxxxxxx + zzzzzzzzzzzzzzzzzz +\n"
     "          yyyyyyyyyyyyyyyyyyyyyy +\n"
     "          ttttttttttttttttttttt,\n"
     "          ddddddddddddddddddddddddd);\n"
     "endmodule\n"
   },
};

TEST(FormatterEndToEndTest, OptimalFormatterUnder60TestCases) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 60;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  for (const auto& test_case : kTestCasesUnder60) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    {
      std::ostringstream stream, debug_stream;
      ExecutionControl control;
      control.stream = &debug_stream;
      const auto status = FormatVerilog(test_case.input, "<filename>", style,
                                        stream, kEnableAllLines, control);
      EXPECT_OK(status);
      EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
      EXPECT_TRUE(debug_stream.str().empty());
    }
    {
      std::ostringstream stream, debug_stream;
      ExecutionControl control;
      control.stream = &debug_stream;
      auto experimental_style = style;
      experimental_style.enable_experimental_tree_reshaper = true;
      const auto status = FormatVerilog(test_case.input, "<filename>",
                                        experimental_style,
                                        stream, kEnableAllLines, control);
      EXPECT_OK(status);
      EXPECT_EQ(stream.str(), test_case.experimental) << "code:\n" << test_case.input;
      EXPECT_TRUE(debug_stream.str().empty());
    }
  }
}

static constexpr FormatterTestCase kTestCasesUnder80[] = {
    //------------------------------- 80 column marker ----------------------------->|
    {
     "module m;initial ffffffffffff("
     "aaaaaaaaaaaaaaaaaaaaa,bbbbbbbbbbbbbbbbbbbbb("
     "zzzzzzzzzzzzzzzzzzz(qqqqq,wwwwwwwwww,"
     "eeeeeeeeee,rrrrrr))); endmodule",
     "module m;\n"
     "  initial\n"
     "    ffffffffffff(aaaaaaaaaaaaaaaaaaaaa, bbbbbbbbbbbbbbbbbbbbb(\n"
     "                 zzzzzzzzzzzzzzzzzzz(qqqqq, wwwwwwwwww, eeeeeeeeee, rrrrrr)));\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    ffffffffffff(\n"
     "        aaaaaaaaaaaaaaaaaaaaa,\n"
     "        bbbbbbbbbbbbbbbbbbbbb(zzzzzzzzzzzzzzzzzzz(\n"
     "                                  qqqqq, wwwwwwwwww,\n"
     "                                  eeeeeeeeee, rrrrrr)));\n"
     "endmodule\n"
    },
    {
     "module m;initial fffff(eeeeeee,aaaaaaaa,bbbbbbbbbbbbbbb"
     "(kkkkk,gggggg(aaaaaaa,bbbbbbbb,cccccccc,ddddd(uuuuuu,"
     "iiiiiii,yyyyyyyyy,tttttttttt),eeeeeeee),iiiiiiiiiii),"
     "cccccccc,ddddddddd,eeeeeeeeee,fffffffffff(uuuuuuu,"
     "aaaaaaaaaa,cccccccccc,dddddddd),gggggg); endmodule",
     "module m;\n"
     "  initial\n"
     "    fffff(eeeeeee, aaaaaaaa, bbbbbbbbbbbbbbb(\n"
     "          kkkkk,\n"
     "          gggggg(\n"
     "              aaaaaaa,\n"
     "              bbbbbbbb,\n"
     "              cccccccc,\n"
     "              ddddd(\n"
     "                  uuuuuu, iiiiiii, yyyyyyyyy, tttttttttt\n"
     "              ),\n"
     "              eeeeeeee\n"
     "          ),\n"
     "          iiiiiiiiiii\n"
     "          ), cccccccc, ddddddddd, eeeeeeeeee, fffffffffff(\n"
     "          uuuuuuu, aaaaaaaaaa, cccccccccc, dddddddd), gggggg);\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    fffff(\n"
     "        eeeeeee, aaaaaaaa,\n"
     "        bbbbbbbbbbbbbbb(kkkkk,\n"
     "                        gggggg(aaaaaaa, bbbbbbbb, cccccccc,\n"
     "                               ddddd(uuuuuu, iiiiiii,\n"
     "                                     yyyyyyyyy, tttttttttt),\n"
     "                               eeeeeeee), iiiiiiiiiii),\n"
     "        cccccccc, ddddddddd,\n"
     "        eeeeeeeeee, fffffffffff(uuuuuuu, aaaaaaaaaa,\n"
     "                               cccccccccc, dddddddd), gggggg);\n"
     "endmodule\n"
    },
    {
     "module m; initial ffff(aaaaaa(sssss,aaaaa,vvvvv,uuuuu),"
     "bbbbb,ccccc,dddd); endmodule",
     "module m;\n"
     "  initial ffff(aaaaaa(sssss, aaaaa, vvvvv, uuuuu), bbbbb, ccccc, dddd);\n"
     "endmodule\n",
     "module m;\n"
     "  initial ffff(aaaaaa(sssss, aaaaa, vvvvv, uuuuu), bbbbb, ccccc, dddd);\n"
     "endmodule\n"
    },
    {
     "module m; initial fff(aaaaaa(sssss(kkkkkkkk,mm(yyy,cc),"
     "nnnnnnn,ooooo),xx(w,e,qq),vvvvv,uuuuu),bbbbb,ccccc,dddd); endmodule",
     "module m;\n"
     "  initial\n"
     "    fff(aaaaaa(\n"
     "        sssss(kkkkkkkk, mm(yyy, cc), nnnnnnn, ooooo), xx(w, e, qq), vvvvv, uuuuu\n"
     "        ), bbbbb, ccccc, dddd);\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    fff(aaaaaa(sssss(kkkkkkkk, mm(yyy, cc), nnnnnnn, ooooo),\n"
     "               xx(w, e, qq), vvvvv, uuuuu), bbbbb, ccccc, dddd);\n"
     "endmodule\n"
    },
    {
     "module m; initial fffffffff(aaaaaa(sssss(kkkkkkkk,"
     "mm(yyy,iiiiiiiiiiiiiiiiiiiiiiiiii(nnnn,xxxx,ddddd,"
     "xxxxx),cc),nnnnnnn,ooooo),xxxxxxxxxxxx(wwwwwwwwww,"
     "eeeeeeeee,qq),vvvvv,uuuuu),bbbbb,ccccc,dddd); endmodule",
     "module m;\n"
     "  initial\n"
     "    fffffffff(aaaaaa(\n"
     "              sssss(\n"
     "                  kkkkkkkk,\n"
     "                  mm(\n"
     "                      yyy,\n"
     "                      iiiiiiiiiiiiiiiiiiiiiiiiii(\n"
     "                          nnnn, xxxx, ddddd, xxxxx\n"
     "                      ),\n"
     "                      cc\n"
     "                  ),\n"
     "                  nnnnnnn,\n"
     "                  ooooo\n"
     "              ),\n"
     "              xxxxxxxxxxxx(\n"
     "                  wwwwwwwwww, eeeeeeeee, qq\n"
     "              ),\n"
     "              vvvvv,\n"
     "              uuuuu\n"
     "              ), bbbbb, ccccc, dddd);\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    fffffffff(aaaaaa(sssss(kkkkkkkk,\n"
     "                           mm(yyy, iiiiiiiiiiiiiiiiiiiiiiiiii(nnnn, xxxx,\n"
     "                                                              ddddd, xxxxx),\n"
     "                              cc), nnnnnnn, ooooo),\n"
     "                     xxxxxxxxxxxx(wwwwwwwwww, eeeeeeeee, qq), vvvvv, uuuuu),\n"
     "              bbbbb, ccccc, dddd);\n"
     "endmodule\n"
    },
    {
     "module m; initial fffffffff(aaaaaa(sssss(kkkkkkkk,"
     "mm(yyy,iiiiiiiiiiiii(nn(j,k,l),xxxx,ddddd,xxxxx),cc),"
     "nnnnnnn,ooooo),xxxxxxxxxxxx(wwwwwwwwww,eeeeeeeee,qq),"
     "vvvvv,uuuuu),bbbbb,ccccc,dddd); endmodule",
     "module m;\n"
     "  initial\n"
     "    fffffffff(aaaaaa(\n"
     "              sssss(\n"
     "                  kkkkkkkk,\n"
     "                  mm(\n"
     "                      yyy, iiiiiiiiiiiii(nn(j, k, l), xxxx, ddddd, xxxxx), cc\n"
     "                  ),\n"
     "                  nnnnnnn,\n"
     "                  ooooo\n"
     "              ),\n"
     "              xxxxxxxxxxxx(\n"
     "                  wwwwwwwwww, eeeeeeeee, qq\n"
     "              ),\n"
     "              vvvvv,\n"
     "              uuuuu\n"
     "              ), bbbbb, ccccc, dddd);\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    fffffffff(aaaaaa(sssss(kkkkkkkk,\n"
     "                           mm(\n"
     "                               yyy, iiiiiiiiiiiii(\n"
     "                                       nn(j, k, l),\n"
     "                                       xxxx, ddddd, xxxxx),\n"
     "                               cc), nnnnnnn, ooooo),\n"
     "                     xxxxxxxxxxxx(wwwwwwwwww, eeeeeeeee, qq), vvvvv, uuuuu),\n"
     "              bbbbb, ccccc, dddd);\n"
     "endmodule\n"
    },
    {
     "module m; initial ffffffffffff(aaaaaaaaaaaaaaaaaaaaa,"
     "bbbbbbbbbbbbbbbbbbbbb(zzzzzzzzzzzzzzzzzzz(qqqqq,wwwwwwwwww,"
     "eeeeeeeeee,rrrrrr),xxxxxxxxxxxxxxxxx,yyyyyyyyyyyyyyyy,"
     "uuuuuuuu(iiiii,jjjjj,kkkkkkk,tttttt)),ccccccccc,dddddddddd,"
     "eeeeeeeeeee,ffffffffffffff(aaaa,bbb,ccc,dddddd(aaa,bbb,cc,"
     "ddd,ee(aaaaa,bbbbb,ccccc(aaa,bbb,ccccc,eeee),dddd,eeee),ffff,"
     "ggg,hhh,iiiii,kkkk,aaaaa,bbbbbbbbbbbbbbbbbb(uuuuuuuuuuuuu,"
     "xxxxxxxxxxxxxxx,uuuuuuuuuuuuu(xxxxxxxxxxxxxxx,xxxxxxxxxx,"
     "xxxxxxxx(uuuuu,yyy,zzz,sss,eeeeeeeee(aaaaa,bbbb,cc,dddd,ee,"
     "ffff),eee,ss,aaa)),xxx)))); endmodule",
     "module m;\n"
     "  initial\n"
     "    ffffffffffff(aaaaaaaaaaaaaaaaaaaaa, bbbbbbbbbbbbbbbbbbbbb(\n"
     "                 zzzzzzzzzzzzzzzzzzz(\n"
     "                     qqqqq, wwwwwwwwww, eeeeeeeeee, rrrrrr\n"
     "                 ),\n"
     "                 xxxxxxxxxxxxxxxxx,\n"
     "                 yyyyyyyyyyyyyyyy,\n"
     "                 uuuuuuuu(\n"
     "                     iiiii, jjjjj, kkkkkkk, tttttt\n"
     "                 )\n"
     "                 ), ccccccccc, dddddddddd, eeeeeeeeeee, ffffffffffffff(\n"
     "                 aaaa,\n"
     "                 bbb,\n"
     "                 ccc,\n"
     "                 dddddd(\n"
     "                     aaa,\n"
     "                     bbb,\n"
     "                     cc,\n"
     "                     ddd,\n"
     "                     ee(\n"
     "                         aaaaa, bbbbb, ccccc(aaa, bbb, ccccc, eeee), dddd, eeee\n"
     "                     ),\n"
     "                     ffff,\n"
     "                     ggg,\n"
     "                     hhh,\n"
     "                     iiiii,\n"
     "                     kkkk,\n"
     "                     aaaaa,\n"
     "                     bbbbbbbbbbbbbbbbbb(\n"
     "                         uuuuuuuuuuuuu,\n"
     "                         xxxxxxxxxxxxxxx,\n"
     "                         uuuuuuuuuuuuu(\n"
     "                             xxxxxxxxxxxxxxx,\n"
     "                             xxxxxxxxxx,\n"
     "                             xxxxxxxx(\n"
     "                                 uuuuu,\n"
     "                                 yyy,\n"
     "                                 zzz,\n"
     "                                 sss,\n"
     "                                 eeeeeeeee(\n"
     "                                     aaaaa, bbbb, cc, dddd, ee, ffff\n"
     "                                 ),\n"
     "                                 eee,\n"
     "                                 ss,\n"
     "                                 aaa\n"
     "                             )\n"
     "                         ),\n"
     "                         xxx\n"
     "                     )\n"
     "                 )\n"
     "                 ));\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    ffffffffffff(aaaaaaaaaaaaaaaaaaaaa,\n"
     "                 bbbbbbbbbbbbbbbbbbbbb(zzzzzzzzzzzzzzzzzzz(qqqqq, wwwwwwwwww,\n"
     "                                                           eeeeeeeeee, rrrrrr),\n"
     "                                       xxxxxxxxxxxxxxxxx, yyyyyyyyyyyyyyyy,\n"
     "                                       uuuuuuuu(iiiii, jjjjj, kkkkkkk, tttttt)),\n"
     "                 ccccccccc, dddddddddd, eeeeeeeeeee,\n"
     "                 ffffffffffffff(aaaa, bbb, ccc,\n"
     "                                dddddd(aaa, bbb,\n"
     "                                       cc, ddd, ee(aaaaa, bbbbb,\n"
     "                                                   ccccc(aaa, bbb, ccccc, eeee),\n"
     "                                                   dddd, eeee),\n"
     "                                       ffff, ggg, hhh, iiiii, kkkk, aaaaa,\n"
     "                                       bbbbbbbbbbbbbbbbbb(\n"
     "                                           uuuuuuuuuuuuu, xxxxxxxxxxxxxxx,\n"
     "                                           uuuuuuuuuuuuu(\n"
     "                                               xxxxxxxxxxxxxxx,\n"
     "                                               xxxxxxxxxx,\n"
     "                                               xxxxxxxx(\n"
     "                                                   uuuuu, yyy,\n"
     "                                                   zzz, sss,\n"
     "                                                   eeeeeeeee(\n"
     "                                                       aaaaa,\n"
     "                                                       bbbb, cc,\n"
     "                                                       dddd, ee,\n"
     "                                                       ffff), eee,\n"
     "                                                   ss, aaa)),\n"
     "                                           xxx))));\n"
     "endmodule\n"
    },
    {
     "module m; assign aa = ffffffffffffffffffffffffff(aaaaaaaaaaaaaaaaaaaaaaa,"
     "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb,cccccccccccccccccccccccccccccccccc,"
     "ddddddddddddddddddddddddddddddddd,eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee,"
     "fffffffffffffffffffffffffffffffff,gggggggggggggggggggggggggggggggggggggg,"
     "hhhhhhhhhhhhhhhhhhhhhhhhhhhh)+hhhhhhhhhhhhhhhhhhhhh(aaaaaaaaaaaaaaaaaaaaaaaa,"
     "bbbbbbbbbbbbbbbbbbbbbbbb,cccccccccccccccccccc+xxxxxxxxxxxxxxxxx+"
     "zzzzzzzzzzzzzzzzzz+yyyyyyyyyyyyyyyyyyyyyy+ttttttttttttttttttttt,"
     "ddddddddddddddddddddddddd); endmodule",
     "module m;\n"
     "  assign aa = ffffffffffffffffffffffffff(\n"
     "      aaaaaaaaaaaaaaaaaaaaaaa,\n"
     "      bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb,\n"
     "      cccccccccccccccccccccccccccccccccc,\n"
     "      ddddddddddddddddddddddddddddddddd,\n"
     "      eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee,\n"
     "      fffffffffffffffffffffffffffffffff,\n"
     "      gggggggggggggggggggggggggggggggggggggg,\n"
     "      hhhhhhhhhhhhhhhhhhhhhhhhhhhh\n"
     "  ) + hhhhhhhhhhhhhhhhhhhhh(\n"
     "      aaaaaaaaaaaaaaaaaaaaaaaa,\n"
     "      bbbbbbbbbbbbbbbbbbbbbbbb,\n"
     "      cccccccccccccccccccc + xxxxxxxxxxxxxxxxx + zzzzzzzzzzzzzzzzzz +\n"
     "          yyyyyyyyyyyyyyyyyyyyyy + ttttttttttttttttttttt,\n"
     "      ddddddddddddddddddddddddd\n"
     "  );\n"
     "endmodule\n",
     "module m;\n"
     "  assign aa =\n"
     "      ffffffffffffffffffffffffff(\n"
     "          aaaaaaaaaaaaaaaaaaaaaaa, bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb,\n"
     "          cccccccccccccccccccccccccccccccccc,\n"
     "          ddddddddddddddddddddddddddddddddd,\n"
     "          eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee,\n"
     "          fffffffffffffffffffffffffffffffff,\n"
     "          gggggggggggggggggggggggggggggggggggggg,\n"
     "          hhhhhhhhhhhhhhhhhhhhhhhhhhhh) +\n"
     "      hhhhhhhhhhhhhhhhhhhhh(\n"
     "          aaaaaaaaaaaaaaaaaaaaaaaa, bbbbbbbbbbbbbbbbbbbbbbbb,\n"
     "          cccccccccccccccccccc +\n"
     "          xxxxxxxxxxxxxxxxx + zzzzzzzzzzzzzzzzzz +\n"
     "          yyyyyyyyyyyyyyyyyyyyyy + ttttttttttttttttttttt,\n"
     "          ddddddddddddddddddddddddd);\n"
     "endmodule\n"
   },
};

TEST(FormatterEndToEndTest, OptimalFormatterUnder80TestCases) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 80;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  for (const auto& test_case : kTestCasesUnder80) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    {
      std::ostringstream stream, debug_stream;
      ExecutionControl control;
      control.stream = &debug_stream;
      const auto status = FormatVerilog(test_case.input, "<filename>", style,
                                        stream, kEnableAllLines, control);
      EXPECT_OK(status);
      EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
      EXPECT_TRUE(debug_stream.str().empty());
    }
    {
      std::ostringstream stream, debug_stream;
      ExecutionControl control;
      control.stream = &debug_stream;
      auto experimental_style = style;
      experimental_style.enable_experimental_tree_reshaper = true;
      const auto status = FormatVerilog(test_case.input, "<filename>",
                                        experimental_style,
                                        stream, kEnableAllLines, control);
      EXPECT_OK(status);
      EXPECT_EQ(stream.str(), test_case.experimental) << "code:\n" << test_case.input;
      EXPECT_TRUE(debug_stream.str().empty());
    }
  }
}

static constexpr FormatterTestCase kTestCasesUnder100[] = {
    //----------------------------------------- 100 column marker --------------------------------------->|
    {
     "module m;initial ffffffffffff("
     "aaaaaaaaaaaaaaaaaaaaa,bbbbbbbbbbbbbbbbbbbbb("
     "zzzzzzzzzzzzzzzzzzz(qqqqq,wwwwwwwwww,"
     "eeeeeeeeee,rrrrrr))); endmodule",
     "module m;\n"
     "  initial\n"
     "    ffffffffffff(aaaaaaaaaaaaaaaaaaaaa, bbbbbbbbbbbbbbbbbbbbb(\n"
     "                 zzzzzzzzzzzzzzzzzzz(qqqqq, wwwwwwwwww, eeeeeeeeee, rrrrrr)));\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    ffffffffffff(aaaaaaaaaaaaaaaaaaaaa,\n"
     "                 bbbbbbbbbbbbbbbbbbbbb(zzzzzzzzzzzzzzzzzzz(qqqqq, wwwwwwwwww, eeeeeeeeee, rrrrrr)));\n"
     "endmodule\n"
    },
    {
     "module m;initial fffff(eeeeeee,aaaaaaaa,bbbbbbbbbbbbbbb"
     "(kkkkk,gggggg(aaaaaaa,bbbbbbbb,cccccccc,ddddd(uuuuuu,"
     "iiiiiii,yyyyyyyyy,tttttttttt),eeeeeeee),iiiiiiiiiii),"
     "cccccccc,ddddddddd,eeeeeeeeee,fffffffffff(uuuuuuu,"
     "aaaaaaaaaa,cccccccccc,dddddddd),gggggg); endmodule",
     "module m;\n"
     "  initial\n"
     "    fffff(eeeeeee, aaaaaaaa, bbbbbbbbbbbbbbb(\n"
     "          kkkkk,\n"
     "          gggggg(\n"
     "              aaaaaaa, bbbbbbbb, cccccccc, ddddd(uuuuuu, iiiiiii, yyyyyyyyy, tttttttttt), eeeeeeee\n"
     "          ),\n"
     "          iiiiiiiiiii\n"
     "          ), cccccccc, ddddddddd, eeeeeeeeee, fffffffffff(uuuuuuu, aaaaaaaaaa, cccccccccc, dddddddd\n"
     "          ), gggggg);\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    fffff(eeeeeee, aaaaaaaa,\n"
     "          bbbbbbbbbbbbbbb(kkkkk, gggggg(aaaaaaa, bbbbbbbb, cccccccc,\n"
     "                                        ddddd(uuuuuu, iiiiiii, yyyyyyyyy, tttttttttt), eeeeeeee),\n"
     "                          iiiiiiiiiii), cccccccc,\n"
     "          ddddddddd, eeeeeeeeee, fffffffffff(uuuuuuu, aaaaaaaaaa, cccccccccc, dddddddd), gggggg);\n"
     "endmodule\n"
    },
    {
     "module m; initial ffff(aaaaaa(sssss,aaaaa,vvvvv,uuuuu),"
     "bbbbb,ccccc,dddd); endmodule",
     "module m;\n"
     "  initial ffff(aaaaaa(sssss, aaaaa, vvvvv, uuuuu), bbbbb, ccccc, dddd);\n"
     "endmodule\n",
     "module m;\n"
     "  initial ffff(aaaaaa(sssss, aaaaa, vvvvv, uuuuu), bbbbb, ccccc, dddd);\n"
     "endmodule\n"
    },
    {
     "module m; initial fff(aaaaaa(sssss(kkkkkkkk,mm(yyy,cc),"
     "nnnnnnn,ooooo),xx(w,e,qq),vvvvv,uuuuu),bbbbb,ccccc,dddd); endmodule",
     "module m;\n"
     "  initial\n"
     "    fff(aaaaaa(sssss(kkkkkkkk, mm(yyy, cc), nnnnnnn, ooooo), xx(w, e, qq), vvvvv, uuuuu), bbbbb,\n"
     "        ccccc, dddd);\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    fff(aaaaaa(sssss(kkkkkkkk, mm(yyy, cc), nnnnnnn, ooooo), xx(w, e, qq), vvvvv, uuuuu),\n"
     "        bbbbb, ccccc, dddd);\n"
     "endmodule\n"
    },
    {
     "module m; initial fffffffff(aaaaaa(sssss(kkkkkkkk,"
     "mm(yyy,iiiiiiiiiiiiiiiiiiiiiiiiii(nnnn,xxxx,ddddd,"
     "xxxxx),cc),nnnnnnn,ooooo),xxxxxxxxxxxx(wwwwwwwwww,"
     "eeeeeeeee,qq),vvvvv,uuuuu),bbbbb,ccccc,dddd); endmodule",
     "module m;\n"
     "  initial\n"
     "    fffffffff(aaaaaa(\n"
     "              sssss(\n"
     "                  kkkkkkkk,\n"
     "                  mm(\n"
     "                      yyy, iiiiiiiiiiiiiiiiiiiiiiiiii(nnnn, xxxx, ddddd, xxxxx), cc\n"
     "                  ),\n"
     "                  nnnnnnn,\n"
     "                  ooooo\n"
     "              ),\n"
     "              xxxxxxxxxxxx(\n"
     "                  wwwwwwwwww, eeeeeeeee, qq\n"
     "              ),\n"
     "              vvvvv,\n"
     "              uuuuu\n"
     "              ), bbbbb, ccccc, dddd);\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    fffffffff(aaaaaa(sssss(kkkkkkkk,\n"
     "                           mm(yyy, iiiiiiiiiiiiiiiiiiiiiiiiii(nnnn, xxxx, ddddd, xxxxx), cc),\n"
     "                           nnnnnnn, ooooo), xxxxxxxxxxxx(wwwwwwwwww, eeeeeeeee, qq), vvvvv, uuuuu),\n"
     "              bbbbb, ccccc, dddd);\n"
     "endmodule\n"
    },
    {
     "module m; initial fffffffff(aaaaaa(sssss(kkkkkkkk,"
     "mm(yyy,iiiiiiiiiiiii(nn(j,k,l),xxxx,ddddd,xxxxx),cc),"
     "nnnnnnn,ooooo),xxxxxxxxxxxx(wwwwwwwwww,eeeeeeeee,qq),"
     "vvvvv,uuuuu),bbbbb,ccccc,dddd); endmodule",
     "module m;\n"
     "  initial\n"
     "    fffffffff(aaaaaa(\n"
     "              sssss(\n"
     "                  kkkkkkkk,\n"
     "                  mm(\n"
     "                      yyy, iiiiiiiiiiiii(nn(j, k, l), xxxx, ddddd, xxxxx), cc\n"
     "                  ),\n"
     "                  nnnnnnn,\n"
     "                  ooooo\n"
     "              ),\n"
     "              xxxxxxxxxxxx(\n"
     "                  wwwwwwwwww, eeeeeeeee, qq\n"
     "              ),\n"
     "              vvvvv,\n"
     "              uuuuu\n"
     "              ), bbbbb, ccccc, dddd);\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    fffffffff(\n"
     "        aaaaaa(sssss(kkkkkkkk,\n"
     "                     mm(yyy, iiiiiiiiiiiii(nn(j, k, l),\n"
     "                                          xxxx, ddddd, xxxxx), cc),\n"
     "                     nnnnnnn, ooooo),\n"
     "               xxxxxxxxxxxx(wwwwwwwwww, eeeeeeeee, qq), vvvvv, uuuuu),\n"
     "        bbbbb, ccccc, dddd);\n"
     "endmodule\n"
    },
    {
     "module m; initial ffffffffffff(aaaaaaaaaaaaaaaaaaaaa,"
     "bbbbbbbbbbbbbbbbbbbbb(zzzzzzzzzzzzzzzzzzz(qqqqq,wwwwwwwwww,"
     "eeeeeeeeee,rrrrrr),xxxxxxxxxxxxxxxxx,yyyyyyyyyyyyyyyy,"
     "uuuuuuuu(iiiii,jjjjj,kkkkkkk,tttttt)),ccccccccc,dddddddddd,"
     "eeeeeeeeeee,ffffffffffffff(aaaa,bbb,ccc,dddddd(aaa,bbb,cc,"
     "ddd,ee(aaaaa,bbbbb,ccccc(aaa,bbb,ccccc,eeee),dddd,eeee),ffff,"
     "ggg,hhh,iiiii,kkkk,aaaaa,bbbbbbbbbbbbbbbbbb(uuuuuuuuuuuuu,"
     "xxxxxxxxxxxxxxx,uuuuuuuuuuuuu(xxxxxxxxxxxxxxx,xxxxxxxxxx,"
     "xxxxxxxx(uuuuu,yyy,zzz,sss,eeeeeeeee(aaaaa,bbbb,cc,dddd,ee,"
     "ffff),eee,ss,aaa)),xxx)))); endmodule",
     "module m;\n"
     "  initial\n"
     "    ffffffffffff(aaaaaaaaaaaaaaaaaaaaa, bbbbbbbbbbbbbbbbbbbbb(\n"
     "                 zzzzzzzzzzzzzzzzzzz(\n"
     "                     qqqqq, wwwwwwwwww, eeeeeeeeee, rrrrrr\n"
     "                 ),\n"
     "                 xxxxxxxxxxxxxxxxx,\n"
     "                 yyyyyyyyyyyyyyyy,\n"
     "                 uuuuuuuu(\n"
     "                     iiiii, jjjjj, kkkkkkk, tttttt\n"
     "                 )\n"
     "                 ), ccccccccc, dddddddddd, eeeeeeeeeee, ffffffffffffff(\n"
     "                 aaaa,\n"
     "                 bbb,\n"
     "                 ccc,\n"
     "                 dddddd(\n"
     "                     aaa,\n"
     "                     bbb,\n"
     "                     cc,\n"
     "                     ddd,\n"
     "                     ee(\n"
     "                         aaaaa, bbbbb, ccccc(aaa, bbb, ccccc, eeee), dddd, eeee\n"
     "                     ),\n"
     "                     ffff,\n"
     "                     ggg,\n"
     "                     hhh,\n"
     "                     iiiii,\n"
     "                     kkkk,\n"
     "                     aaaaa,\n"
     "                     bbbbbbbbbbbbbbbbbb(\n"
     "                         uuuuuuuuuuuuu,\n"
     "                         xxxxxxxxxxxxxxx,\n"
     "                         uuuuuuuuuuuuu(\n"
     "                             xxxxxxxxxxxxxxx,\n"
     "                             xxxxxxxxxx,\n"
     "                             xxxxxxxx(\n"
     "                                 uuuuu,\n"
     "                                 yyy,\n"
     "                                 zzz,\n"
     "                                 sss,\n"
     "                                 eeeeeeeee(\n"
     "                                     aaaaa, bbbb, cc, dddd, ee, ffff\n"
     "                                 ),\n"
     "                                 eee,\n"
     "                                 ss,\n"
     "                                 aaa\n"
     "                             )\n"
     "                         ),\n"
     "                         xxx\n"
     "                     )\n"
     "                 )\n"
     "                 ));\n"
     "endmodule\n",
     "module m;\n"
     "  initial\n"
     "    ffffffffffff(aaaaaaaaaaaaaaaaaaaaa,\n"
     "                 bbbbbbbbbbbbbbbbbbbbb(zzzzzzzzzzzzzzzzzzz(qqqqq, wwwwwwwwww, eeeeeeeeee, rrrrrr),\n"
     "                                       xxxxxxxxxxxxxxxxx,\n"
     "                                       yyyyyyyyyyyyyyyy, uuuuuuuu(iiiii, jjjjj, kkkkkkk, tttttt)),\n"
     "                 ccccccccc, dddddddddd, eeeeeeeeeee,\n"
     "                 ffffffffffffff(aaaa, bbb, ccc,\n"
     "                                dddddd(aaa, bbb, cc, ddd,\n"
     "                                       ee(aaaaa, bbbbb, ccccc(aaa, bbb, ccccc, eeee), dddd, eeee),\n"
     "                                       ffff, ggg, hhh, iiiii, kkkk, aaaaa,\n"
     "                                       bbbbbbbbbbbbbbbbbb(uuuuuuuuuuuuu, xxxxxxxxxxxxxxx,\n"
     "                                                          uuuuuuuuuuuuu(xxxxxxxxxxxxxxx, xxxxxxxxxx,\n"
     "                                                                        xxxxxxxx(uuuuu,\n"
     "                                                                                 yyy, zzz, sss,\n"
     "                                                                                 eeeeeeeee(aaaaa,\n"
     "                                                                                           bbbb, cc,\n"
     "                                                                                           dddd, ee,\n"
     "                                                                                           ffff),\n"
     "                                                                                 eee, ss, aaa)),\n"
     "                                                          xxx))));\n"
     "endmodule\n"
    },
    {
     "module m; assign aa = ffffffffffffffffffffffffff(aaaaaaaaaaaaaaaaaaaaaaa,"
     "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb,cccccccccccccccccccccccccccccccccc,"
     "ddddddddddddddddddddddddddddddddd,eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee,"
     "fffffffffffffffffffffffffffffffff,gggggggggggggggggggggggggggggggggggggg,"
     "hhhhhhhhhhhhhhhhhhhhhhhhhhhh)+hhhhhhhhhhhhhhhhhhhhh(aaaaaaaaaaaaaaaaaaaaaaaa,"
     "bbbbbbbbbbbbbbbbbbbbbbbb,cccccccccccccccccccc+xxxxxxxxxxxxxxxxx+"
     "zzzzzzzzzzzzzzzzzz+yyyyyyyyyyyyyyyyyyyyyy+ttttttttttttttttttttt,"
     "ddddddddddddddddddddddddd); endmodule",
     "module m;\n"
     "  assign aa = ffffffffffffffffffffffffff(\n"
     "      aaaaaaaaaaaaaaaaaaaaaaa,\n"
     "      bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb,\n"
     "      cccccccccccccccccccccccccccccccccc,\n"
     "      ddddddddddddddddddddddddddddddddd,\n"
     "      eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee,\n"
     "      fffffffffffffffffffffffffffffffff,\n"
     "      gggggggggggggggggggggggggggggggggggggg,\n"
     "      hhhhhhhhhhhhhhhhhhhhhhhhhhhh\n"
     "  ) + hhhhhhhhhhhhhhhhhhhhh(\n"
     "      aaaaaaaaaaaaaaaaaaaaaaaa,\n"
     "      bbbbbbbbbbbbbbbbbbbbbbbb,\n"
     "      cccccccccccccccccccc + xxxxxxxxxxxxxxxxx + zzzzzzzzzzzzzzzzzz + yyyyyyyyyyyyyyyyyyyyyy +\n"
     "          ttttttttttttttttttttt,\n"
     "      ddddddddddddddddddddddddd\n"
     "  );\n"
     "endmodule\n",
     "module m;\n"
     "  assign aa = ffffffffffffffffffffffffff(\n"
     "                  aaaaaaaaaaaaaaaaaaaaaaa, bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb,\n"
     "                  cccccccccccccccccccccccccccccccccc, ddddddddddddddddddddddddddddddddd,\n"
     "                  eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee, fffffffffffffffffffffffffffffffff,\n"
     "                  gggggggggggggggggggggggggggggggggggggg, hhhhhhhhhhhhhhhhhhhhhhhhhhhh) +\n"
     "              hhhhhhhhhhhhhhhhhhhhh(aaaaaaaaaaaaaaaaaaaaaaaa, bbbbbbbbbbbbbbbbbbbbbbbb,\n"
     "                                    cccccccccccccccccccc + xxxxxxxxxxxxxxxxx + zzzzzzzzzzzzzzzzzz +\n"
     "                                    yyyyyyyyyyyyyyyyyyyyyy + ttttttttttttttttttttt,\n"
     "                                    ddddddddddddddddddddddddd);\n"
     "endmodule\n"
    },
};

TEST(FormatterEndToEndTest, OptimalFormatterUnder100TestCases) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 100;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  for (const auto& test_case : kTestCasesUnder100) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    {
      std::ostringstream stream, debug_stream;
      ExecutionControl control;
      control.stream = &debug_stream;
      const auto status = FormatVerilog(test_case.input, "<filename>", style,
                                        stream, kEnableAllLines, control);
      EXPECT_OK(status);
      EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
      EXPECT_TRUE(debug_stream.str().empty());
    }
    {
      std::ostringstream stream, debug_stream;
      ExecutionControl control;
      control.stream = &debug_stream;
      auto experimental_style = style;
      experimental_style.enable_experimental_tree_reshaper = true;
      const auto status = FormatVerilog(test_case.input, "<filename>",
                                        experimental_style,
                                        stream, kEnableAllLines, control);
      EXPECT_OK(status);
      EXPECT_EQ(stream.str(), test_case.experimental) << "code:\n" << test_case.input;
      EXPECT_TRUE(debug_stream.str().empty());
    }
  }
}

using verible::UnwrappedLine;

// Contains the expected token sequence and indentation for an UnwrappedLine
struct ExpectedUnwrappedLine {
  int indentation_spaces;
  std::vector<absl::string_view> tokens;  // includes comments

  explicit ExpectedUnwrappedLine(int s) : indentation_spaces(s), tokens() {}

  ExpectedUnwrappedLine(
      int s, std::initializer_list<absl::string_view> expected_tokens)
      : indentation_spaces(s), tokens(expected_tokens) {}

  void ShowUnwrappedLineDifference(std::ostream* stream,
                                   const UnwrappedLine& uwline) const;
  bool EqualsUnwrappedLine(std::ostream* stream,
                           const UnwrappedLine& uwline) const;
};

// Human readable ExpectedUnwrappedLined which outputs indentation and line.
// Mimic operator << (ostream&, const UnwrappedLine&).
std::ostream& operator<<(std::ostream& stream,
                         const ExpectedUnwrappedLine& expected_uwline) {
  stream << verible::Spacer(expected_uwline.indentation_spaces,
                            UnwrappedLine::kIndentationMarker)
         << '[';
  if (expected_uwline.tokens.empty()) {
    // Empty really means don't-care -- this is not a leaf level
    // UnwrappedLine, but rather, an enclosing level.
    stream << "<auto>";
  } else {
    stream << absl::StrJoin(expected_uwline.tokens.begin(),
                            expected_uwline.tokens.end(), " ",
                            absl::StreamFormatter());
  }
  return stream << ']';
}

// Outputs the unwrapped line followed by this expected unwrapped line
void ExpectedUnwrappedLine::ShowUnwrappedLineDifference(
    std::ostream* stream, const UnwrappedLine& uwline) const {
  *stream << std::endl
          << "unwrapped line: " << std::endl
          << '\"' << uwline << '\"' << std::endl;
  *stream << "expected: " << std::endl;
  *stream << '\"' << *this << '\"' << std::endl;
}

// Helper method to compare ExpectedUnwrappedLine to UnwrappedLine by checking
// sizes (number of tokens), each token sequentially, and indentation.
// Outputs differences to stream
bool ExpectedUnwrappedLine::EqualsUnwrappedLine(
    std::ostream* stream, const UnwrappedLine& uwline) const {
  VLOG(4) << __FUNCTION__;
  bool equal = true;
  // If the expected token array is empty, don't check because tokens
  // are expected in children nodes.
  if (!tokens.empty()) {
    // Check that the size of the UnwrappedLine (number of tokens) is correct.
    if (uwline.Size() != tokens.size()) {
      *stream << "error: unwrapped line size incorrect" << std::endl;
      *stream << "unwrapped line has: " << uwline.Size() << " tokens, ";
      *stream << "expected: " << tokens.size() << std::endl;
      ShowUnwrappedLineDifference(stream, uwline);
      equal = false;
    } else {
      // Only compare the text of each token, and none of the other TokenInfo
      // fields. Stops at first unmatched token
      // TODO(fangism): rewrite this using std::mismatch
      for (size_t i = 0; i < uwline.Size(); i++) {
        absl::string_view uwline_token = uwline.TokensRange()[i].Text();
        absl::string_view expected_token = tokens[i];
        if (uwline_token != expected_token) {
          *stream << "error: unwrapped line token #" << i + 1
                  << " does not match expected token" << std::endl;
          *stream << "unwrapped line token is: \"" << uwline_token << "\""
                  << std::endl;
          *stream << "expected: \"" << expected_token << "\"" << std::endl;
          equal = false;
        }
      }
    }
  }

  // Check that the indentation spaces of the UnwrappedLine is correct
  if (uwline.IndentationSpaces() != indentation_spaces) {
    *stream << "error: unwrapped line indentation incorrect" << std::endl;
    *stream << "indentation spaces: " << uwline.IndentationSpaces()
            << std::endl;
    *stream << "expected indentation spaces: " << indentation_spaces
            << std::endl;
    equal = false;
  }
  if (!equal) {
    ShowUnwrappedLineDifference(stream, uwline);
    return false;
  }
  return true;
}

typedef verible::VectorTree<ExpectedUnwrappedLine> ExpectedUnwrappedLineTree;

// N is for node
template <typename... Args>
ExpectedUnwrappedLineTree N(int spaces, Args&&... nodes) {
  return ExpectedUnwrappedLineTree(ExpectedUnwrappedLine(spaces),
                                   std::forward<Args>(nodes)...);
}

// L is for leaf, which is the only type of node that should list tokens
ExpectedUnwrappedLineTree L(int spaces,
                            std::initializer_list<absl::string_view> tokens) {
  return ExpectedUnwrappedLineTree(ExpectedUnwrappedLine(spaces, tokens));
}

// Test fixture used to handle the VerilogAnalyzer which produces the
// concrete syntax tree and token stream that TreeUnwrapper uses to produce
// UnwrappedLines
class TreeUnwrapperTest : public ::testing::Test {
 protected:
  TreeUnwrapperTest() {
    style_.indentation_spaces = 1;
    style_.wrap_spaces = 2;
  }

  // Takes a string representation of a verilog file and creates a
  // VerilogAnalyzer which holds a concrete syntax tree and token stream view
  // of the file.
  void MakeTree(absl::string_view content) {
    analyzer_ = absl::make_unique<VerilogAnalyzer>(content, "TEST_FILE");
    absl::Status status = ABSL_DIE_IF_NULL(analyzer_)->Analyze();
    EXPECT_OK(status) << "Rejected code: " << std::endl << content;

    // Since source code is required to be valid, this error-handling is just
    // to help debug the test case construction
    if (!status.ok()) {
      const std::vector<std::string> syntax_error_messages(
          analyzer_->LinterTokenErrorMessages());
      for (const auto& message : syntax_error_messages) {
        std::cout << message << std::endl;
      }
    }
  }
  // Creates a TreeUnwrapper populated with a concrete syntax tree and
  // token stream view from the file input
  std::unique_ptr<TreeUnwrapper> CreateTreeUnwrapper(
      absl::string_view source_code) {
    MakeTree(source_code);
    const verible::TextStructureView& text_structure_view = analyzer_->Data();
    unwrapper_data_ =
        absl::make_unique<UnwrapperData>(text_structure_view.TokenStream());

    return absl::make_unique<TreeUnwrapper>(
        text_structure_view, style_, unwrapper_data_->preformatted_tokens);
  }

  // The VerilogAnalyzer to produce a concrete syntax tree of raw Verilog code
  std::unique_ptr<VerilogAnalyzer> analyzer_;

  // Support data that needs to outlive the TreeUnwrappers that use it.
  std::unique_ptr<UnwrapperData> unwrapper_data_;

  // Style configuration.
  FormatStyle style_;
};

void ValidateExpectedTreeNode(const ExpectedUnwrappedLineTree& etree) {
  // At each tree node, there should either be expected tokens in the node's
  // value, or node's children, but not both.
  CHECK(etree.Value().tokens.empty() != etree.is_leaf())
      << "Node should not contain both tokens and children @"
      << verible::NodePath(etree);
}

// Make sure the expect-tree is well-formed.
void ValidateExpectedTree(const ExpectedUnwrappedLineTree& etree) {
  etree.ApplyPreOrder(ValidateExpectedTreeNode);
}

// Contains test cases for files with the UnwrappedLines that should be
// produced from TreeUnwrapper.Unwrap()
struct TreeUnwrapperTestData {
  const char* test_name;

  // The source code for testing must be syntactically correct.
  absl::string_view source_code;

  // The reference values and structure of UnwrappedLines to expect.
  ExpectedUnwrappedLineTree expected_unwrapped_lines;

  template <typename... Args>
  TreeUnwrapperTestData(const char* name, absl::string_view code,
                        Args&&... nodes)
      : test_name(name),
        source_code(code),
        expected_unwrapped_lines(ExpectedUnwrappedLine{0},
                                 std::forward<Args>(nodes)...) {
    // The root node is always at level 0.
    ValidateExpectedTree(expected_unwrapped_lines);
  }
};

// Iterates through UnwrappedLines and expected lines and verifies that they
// are equal
bool VerifyUnwrappedLines(std::ostream* stream,
                          const verible::VectorTree<UnwrappedLine>& uwlines,
                          const TreeUnwrapperTestData& test_case) {
  std::ostringstream first_diff_stream;
  const auto diff = verible::DeepEqual(
      uwlines, test_case.expected_unwrapped_lines,
      [&first_diff_stream](const UnwrappedLine& actual,
                           const ExpectedUnwrappedLine& expect) {
        return expect.EqualsUnwrappedLine(&first_diff_stream, actual);
      });

  if (diff.left != nullptr) {
    *stream << "error: test case: " << test_case.test_name << std::endl;
    *stream << "first difference at subnode " << verible::NodePath(*diff.left)
            << std::endl;
    *stream << "expected:\n" << *diff.right << std::endl;
    *stream << "but got :\n"
            << verible::TokenPartitionTreePrinter(*diff.left) << std::endl;
    const auto left_children = diff.left->Children().size();
    const auto right_children = diff.right->Children().size();
    EXPECT_EQ(left_children, right_children) << "code:\n"
                                             << test_case.source_code;
    if (first_diff_stream.str().length()) {
      // The Value()s at these nodes are different.
      *stream << "value difference: " << first_diff_stream.str();
    }
    return false;
  }
  return true;
}

const TreeUnwrapperTestData kFunctionCallTests[] = {
    {
        "single function call",
        "module foo;"
        "  initial foo(aaa,bbb,ccc);"
        "endmodule",
        N(0,
          L(0, {"module", "foo", ";"}),
          N(1,
            L(1, {"initial"}),
            N(2,
              L(2, {"foo", "("}),
              N(2,
                L(2, {"aaa", ","}),
                L(2, {"bbb", ","}),
                L(2, {"ccc", ")", ";"})))),
          L(0, {"endmodule"})),
    },
    {
        "nested function call",
        "module foo;"
        "  initial foo(aaa,bbb(zzz,xxx,yyy),ccc);"
        "endmodule",
        N(0,
          L(0, {"module", "foo", ";"}),
          N(1,
            L(1, {"initial"}),
            N(2,
              L(2, {"foo", "("}),
              N(2,
                L(2, {"aaa", ","}),
                N(2,
                  L(2, {"bbb", "("}),
                  N(2,
                    L(2, {"zzz", ","}),
                    L(2, {"xxx", ","}),
                    L(2, {"yyy", ")", ","}))),
                L(2, {"ccc", ")", ";"})))),
          L(0, {"endmodule"})),
    },
};

// Test that TreeUnwrapper produces correct UnwrappedLines from package tests
TEST_F(TreeUnwrapperTest, FunctionCallTests) {
  style_.enable_experimental_tree_reshaper = true;
  for (const auto& test_case : kFunctionCallTests) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto* uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

const TreeUnwrapperTestData kSimpleTestCases[] = {
    {
        "if statement",
        "module foo;"
        "  initial if (a < (b + c(xxx,yyy,zzz) + d + e)) ;"
        "endmodule",
        N(0,
          L(0, {"module", "foo", ";"}),
          N(1,
            L(1, {"initial"}),
            N(2,
              L(2, {"if", "("}),
              N(4,
                L(4, {"a"}),
                L(4, {"<"}),
                N(5,
                  L(5, {"("}),
                  N(5,
                    L(5, {"b", "+"}),
                    N(5,
                      L(5, {"c", "("}),
                      N(5,
                        L(5, {"xxx", ","}),
                        L(5, {"yyy", ","}),
                        L(5, {"zzz", ")", "+"}))),
                    L(5, {"d", "+"}),
                    L(5, {"e", ")"})))),
              L(4, {")", ";"}))),
          L(0, {"endmodule"}))
    },
    {
     "continuous assignment with binary expressions",
     "module foo; assign aaaaaa = bbbbb + ccccc + dddd + eeee + ffff; endmodule",
     N(0,
       L(0, {"module", "foo", ";"}),
       N(1,
         L(1, {"assign", "aaaaaa", "="}),
         N(2,
           L(2, {"bbbbb", "+"}),
           L(2, {"ccccc", "+"}),
           L(2, {"dddd", "+"}),
           L(2, {"eeee", "+"}),
           L(2, {"ffff", ";"}))),
       L(0, {"endmodule"})),
    },
    {
     "continuous assign with binary expression and function call",
     "module m; assign a = b + c + d +"
     "e_call(aaa,bbb+ccc+ddd,eee,fff,ggg) + f + g + h;endmodule",
     N(0,
       L(0, {"module", "m", ";"}),
       N(1,
         L(1, {"assign", "a", "="}),
         N(2,
           L(2, {"b", "+"}),
           L(2, {"c", "+"}),
           L(2, {"d", "+"}),
           N(2,
             L(2, {"e_call", "("}),
             N(2,
               L(2, {"aaa", ","}),
               N(2,
                 L(2, {"bbb", "+"}),
                 L(2, {"ccc", "+"}),
                 L(2, {"ddd", ","})),
               L(2, {"eee", ","}),
               L(2, {"fff", ","}),
               L(2, {"ggg", ")", "+"}))),
           L(2, {"f", "+"}),
           L(2, {"g", "+"}),
           L(2, {"h", ";"}))),
       L(0, {"endmodule"}))
    },
    {
     "simple continuous assignment",
     "module foo; assign aaaaaa = bbbbb; endmodule",
     N(0,
       L(0, {"module", "foo", ";"}),
       N(1,
         L(1, {"assign", "aaaaaa", "="}),
         L(2, {"bbbbb", ";"})),
       L(0, {"endmodule"})),
    },
};

// Test that TreeUnwrapper produces correct UnwrappedLines from package tests
TEST_F(TreeUnwrapperTest, IfStatementsTests) {
  style_.enable_experimental_tree_reshaper = true;
  style_.indentation_spaces = 1;
  style_.wrap_spaces = 2;
  for (const auto& test_case : kSimpleTestCases) {
    auto tree_unwrapper = CreateTreeUnwrapper(test_case.source_code);
    const auto* uwline_tree = tree_unwrapper->Unwrap();
    EXPECT_TRUE(VerifyUnwrappedLines(&std::cout, *ABSL_DIE_IF_NULL(uwline_tree),
                                     test_case));
  }
}

static constexpr FormatterTestCase kSmallTestCases[] = {
    // 20 column marker  >|
    {
     "module foo; assign aaaaaa = bbbbb + ccccc + dddd + eeee + ffff; endmodule",
     "module foo;\n"
     "  assign aaaaaa =\n"
     "      bbbbb +\n"
     "      ccccc + dddd +\n"
     "      eeee + ffff;\n"
     "endmodule\n",
     "module foo;\n"
     "  assign aaaaaa =\n"
     "      bbbbb +\n"
     "      ccccc +\n"
     "      dddd +\n"
     "      eeee +\n"
     "      ffff;\n"
     "endmodule\n"
    },
    {
     "module m; assign aa = foo(aaaa + bbbb, cccc) + hhhh + foo2(aaa,bbb,cccc,ddddd); endmodule",
     "module m;\n"
     "  assign aa = foo(\n"
     "      aaaa + bbbb,\n"
     "      cccc\n"
     "  ) + hhhh + foo2(\n"
     "      aaa,\n"
     "      bbb,\n"
     "      cccc,\n"
     "      ddddd\n"
     "  );\n"
     "endmodule\n",
     "module m;\n"
     "  assign aa =\n"
     "      foo(\n"
     "          aaaa +\n"
     "          bbbb,\n"
     "          cccc) +\n"
     "      hhhh +\n"
     "      foo2(\n"
     "          aaa,\n"
     "          bbb,\n"
     "          cccc,\n"
     "          ddddd);\n"
     "endmodule\n"
    },
};

TEST(FormatterEndToEndTest, SmallTestCases) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 20;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  for (const auto& test_case : kSmallTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    {
      std::ostringstream stream, debug_stream;
      ExecutionControl control;
      control.stream = &debug_stream;
      const auto status = FormatVerilog(test_case.input, "<filename>", style,
                                        stream, kEnableAllLines, control);
      EXPECT_OK(status);
      EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
      EXPECT_TRUE(debug_stream.str().empty());
    }
    {
      std::ostringstream stream, debug_stream;
      ExecutionControl control;
      control.stream = &debug_stream;
      auto experimental_style = style;
      experimental_style.enable_experimental_tree_reshaper = true;
      const auto status = FormatVerilog(test_case.input, "<filename>",
                                        experimental_style,
                                        stream, kEnableAllLines, control);
      EXPECT_OK(status);
      EXPECT_EQ(stream.str(), test_case.experimental) << "code:\n" << test_case.input;
      EXPECT_TRUE(debug_stream.str().empty());
    }
  }
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
