#include "QuickNumbers.h"

#include <gtest/gtest.h>

#include <ctime>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace McBopomofo::SmartLearning {
namespace {

// 2026-06-15 12:00 UTC: the year is 2026 in every time zone.
constexpr std::time_t kNow = 1781524800;

using Strings = std::vector<std::string>;

TEST(QuickNumbersTest, NormalizesFullWidthDigitsAndOperators) {
  EXPECT_EQ(NormalizeNumberCode("（２＋３）＊３＝"), "(2+3)*3=");
  EXPECT_EQ(NormalizeNumberCode("６−１×２÷１／１－０"), "6-1*2/1/1-0");
  EXPECT_EQ(NormalizeNumberCode("１２：３０"), "12:30");
  EXPECT_EQ(NormalizeNumberCode("２０２３．９．７"), "2023.9.7");
  // "。" or "‧" only between two digits.
  EXPECT_EQ(NormalizeNumberCode("3。5＊2"), "3.5*2");
  EXPECT_EQ(NormalizeNumberCode("５‧５"), "5.5");
  EXPECT_EQ(NormalizeNumberCode("好。5"), "好。5");
  EXPECT_EQ(NormalizeNumberCode("5。"), "5。");
  EXPECT_EQ(NormalizeNumberCode("我算（2＋3"), "我算(2+3");
}

TEST(QuickNumbersTest, NumberCodes) {
  EXPECT_TRUE(IsNumberCode("2023.09.07"));
  EXPECT_TRUE(IsNumberCode("12:30"));
  EXPECT_TRUE(IsNumberCode("(2+3)*3="));
  EXPECT_TRUE(IsNumberCode("（２＋３）"));
  EXPECT_TRUE(IsNumberCode("3。5"));
  EXPECT_FALSE(IsNumberCode(""));
  EXPECT_FALSE(IsNumberCode("-5"));
  EXPECT_FALSE(IsNumberCode(".5"));
  EXPECT_FALSE(IsNumberCode("6,548"));
  EXPECT_FALSE(IsNumberCode("1e5"));
  EXPECT_FALSE(IsNumberCode("sig"));
  EXPECT_FALSE(IsNumberCode("12好"));
}

TEST(QuickNumbersTest, Date) {
  Strings expected = {
      "2023年9月7日（星期四）",       "二〇二三年九月七日（星期四）",
      "中華民國112年9月7日（星期四）", "民國112年9月7日",
      "西元2023年9月7日（星期四）",   "2023/09/07",
      "2023-09-07",                   "9月7日（星期四）",
      "九月七日（星期四）"};
  EXPECT_EQ(QuickNumberCandidates("2023.09.07", kNow), expected);
  // A year of the Republic of China.
  EXPECT_EQ(QuickNumberCandidates("112.9.7", kNow), expected);
  EXPECT_EQ(QuickNumberCandidates("2023/9/7", kNow).size(), 12u);
}

TEST(QuickNumbersTest, MonthAndDayIsAlsoADivision) {
  // 2026-09-07 is a Monday.
  EXPECT_EQ(QuickNumberCandidates("9/7", kNow),
            (Strings{"9月7日（星期一）", "九月七日（星期一）", "1.285714",
                     "9/7=1.285714", "9÷7=1.285714"}));
  // No 2/30, so only the division.
  EXPECT_EQ(QuickNumberCandidates("2/30", kNow),
            (Strings{"0.066667", "2/30=0.066667", "2÷30=0.066667"}));
  // The year comes from now: 2/29 is a date in 2028, not in 2026.
  EXPECT_EQ(QuickNumberCandidates("2/29", kNow).size(), 3u);
  auto leapYear = QuickNumberCandidates("2/29", 1844683200);
  ASSERT_FALSE(leapYear.empty());
  EXPECT_EQ(leapYear.front(), "2月29日（星期二）");
}

TEST(QuickNumbersTest, Time) {
  EXPECT_EQ(QuickNumberCandidates("12:30", kNow),
            (Strings{"12時30分", "下午十二點三十分", "下午12點30分",
                     "12:30 PM", "PM 12:30", "12:30"}));
  EXPECT_EQ(QuickNumberCandidates("9:05", kNow),
            (Strings{"9時5分", "上午九點零五分", "上午9點5分", "9:05 AM",
                     "AM 9:05", "09:05"}));
  EXPECT_EQ(QuickNumberCandidates("0:00", kNow),
            (Strings{"0時0分", "上午十二點", "上午12點", "12:00 AM",
                     "AM 12:00", "00:00"}));
  EXPECT_TRUE(QuickNumberCandidates("24:00", kNow).empty());
  EXPECT_TRUE(QuickNumberCandidates("12:5", kNow).empty());
}

TEST(QuickNumbersTest, Amount) {
  EXPECT_EQ(QuickNumberCandidates("6548", kNow),
            (Strings{"6,548", "陸仟伍佰肆拾捌元整", "六千五百四十八",
                     "6,548元", "NT$6,548", "六五四八", "陸伍肆捌"}));
  EXPECT_EQ(QuickNumberCandidates("6548.5", kNow),
            (Strings{"6,548.5", "六千五百四十八點五", "陸仟伍佰肆拾捌點伍",
                     "6,548.5元"}));
  // At most 16 digits.
  EXPECT_TRUE(QuickNumberCandidates("12345678901234567", kNow).empty());
}

TEST(QuickNumbersTest, Arithmetic) {
  EXPECT_EQ(QuickNumberCandidates("10*15/6", kNow),
            (Strings{"25", "10*15/6=25", "10×15÷6=25", "25元", "貳拾伍元整",
                     "二十五"}));
  EXPECT_EQ(QuickNumberCandidates("(2+3)*3=", kNow),
            (Strings{"15", "(2+3)*3=15", "(2+3)×3=15", "15元", "壹拾伍元整",
                     "一十五"}));
  EXPECT_TRUE(QuickNumberCandidates("1/0", kNow).empty());
  // The separators of a date must be the same.
  EXPECT_EQ(QuickNumberCandidates("2023.9/7", kNow),
            (Strings{"289.128571", "2023.9/7=289.128571",
                     "2023.9÷7=289.128571"}));
}

TEST(QuickNumbersTest, Evaluate) {
  EXPECT_EQ(EvaluateArithmetic("(2+3)*3"), 15.0);
  EXPECT_EQ(EvaluateArithmetic("1-(-1)"), 2.0);
  EXPECT_EQ(EvaluateArithmetic("5.+1"), 6.0);
  EXPECT_EQ(EvaluateArithmetic("1+.5"), 1.5);
  EXPECT_EQ(EvaluateArithmetic("2*3+4*5"), 26.0);
  EXPECT_EQ(EvaluateArithmetic("10-4-3"), 3.0);
  EXPECT_EQ(EvaluateArithmetic("12"), std::nullopt);
  EXPECT_EQ(EvaluateArithmetic("1/0"), std::nullopt);
  EXPECT_EQ(EvaluateArithmetic("1/(2-2)"), std::nullopt);
  EXPECT_EQ(EvaluateArithmetic("1+"), std::nullopt);
  EXPECT_EQ(EvaluateArithmetic("(1+2"), std::nullopt);
  EXPECT_EQ(EvaluateArithmetic("1+2)"), std::nullopt);
  EXPECT_EQ(EvaluateArithmetic("1..2+1"), std::nullopt);
  EXPECT_EQ(EvaluateArithmetic("1+.+1"), std::nullopt);
  EXPECT_EQ(EvaluateArithmetic("1 + 2"), std::nullopt);
}

TEST(QuickNumbersTest, NumbersTooLongForADouble) {
  std::string huge = "1" + std::string(400, '0');
  // 1 over infinity is 0; infinity times 0 is not a number.
  EXPECT_EQ(EvaluateArithmetic("1/" + huge), 0.0);
  EXPECT_EQ(EvaluateArithmetic(huge + "*0"), std::nullopt);
  EXPECT_EQ(EvaluateArithmetic(huge + "+1"), std::nullopt);
  EXPECT_EQ(EvaluateArithmetic("0." + std::string(400, '0') + "1+1"), 1.0);
}

TEST(QuickNumbersTest, ArithmeticResultInTheChineseMode) {
  EXPECT_EQ(ArithmeticResult("（2＋3）＊3"), "15");
  EXPECT_EQ(ArithmeticResult("3。5＊2"), "7");
  // The longest expression that makes sense.
  EXPECT_EQ(ArithmeticResult("我算（2＋3"), "5");
  EXPECT_EQ(ArithmeticResult("1+1=2"), std::nullopt);
  EXPECT_EQ(ArithmeticResult("我"), std::nullopt);
  EXPECT_EQ(ArithmeticResult("1/0"), std::nullopt);
}

struct MacCase {
  std::string code;
  bool isNumberCode;
  Strings candidates;
};

// The outputs of the macOS version for the same codes, with 2026 as the
// current year.
const std::vector<MacCase> kMacCases = {
    {"2023.09.07", true, {"2023年9月7日（星期四）", "二〇二三年九月七日（星期四）", "中華民國112年9月7日（星期四）", "民國112年9月7日", "西元2023年9月7日（星期四）", "2023/09/07", "2023-09-07", "9月7日（星期四）", "九月七日（星期四）"}},
    {"112.9.7", true, {"2023年9月7日（星期四）", "二〇二三年九月七日（星期四）", "中華民國112年9月7日（星期四）", "民國112年9月7日", "西元2023年9月7日（星期四）", "2023/09/07", "2023-09-07", "9月7日（星期四）", "九月七日（星期四）"}},
    {"9/7", true, {"9月7日（星期一）", "九月七日（星期一）", "1.285714", "9/7=1.285714", "9÷7=1.285714"}},
    {"2023/9/7", true, {"2023年9月7日（星期四）", "二〇二三年九月七日（星期四）", "中華民國112年9月7日（星期四）", "民國112年9月7日", "西元2023年9月7日（星期四）", "2023/09/07", "2023-09-07", "9月7日（星期四）", "九月七日（星期四）", "32.111111", "2023/9/7=32.111111", "2023÷9÷7=32.111111"}},
    {"2023-9-7", true, {"2023年9月7日（星期四）", "二〇二三年九月七日（星期四）", "中華民國112年9月7日（星期四）", "民國112年9月7日", "西元2023年9月7日（星期四）", "2023/09/07", "2023-09-07", "9月7日（星期四）", "九月七日（星期四）", "2007", "2023-9-7=2007", "2,007元", "貳仟零柒元整", "二千〇七"}},
    {"12:30", true, {"12時30分", "下午十二點三十分", "下午12點30分", "12:30 PM", "PM 12:30", "12:30"}},
    {"9:05", true, {"9時5分", "上午九點零五分", "上午9點5分", "9:05 AM", "AM 9:05", "09:05"}},
    {"0:00", true, {"0時0分", "上午十二點", "上午12點", "12:00 AM", "AM 12:00", "00:00"}},
    {"1:05", true, {"1時5分", "上午一點零五分", "上午1點5分", "1:05 AM", "AM 1:05", "01:05"}},
    {"23:59", true, {"23時59分", "下午十一點五十九分", "下午11點59分", "11:59 PM", "PM 11:59", "23:59"}},
    {"24:00", true, {}},
    {"12:5", true, {}},
    {"012:30", true, {"12時30分", "下午十二點三十分", "下午12點30分", "12:30 PM", "PM 12:30", "12:30"}},
    {"12:60", true, {}},
    {"1:2:3", true, {}},
    {"6548", true, {"6,548", "陸仟伍佰肆拾捌元整", "六千五百四十八", "6,548元", "NT$6,548", "六五四八", "陸伍肆捌"}},
    {"6548.5", true, {"6,548.5", "六千五百四十八點五", "陸仟伍佰肆拾捌點伍", "6,548.5元"}},
    {"6548.", true, {"6,548", "陸仟伍佰肆拾捌元整", "六千五百四十八", "6,548元", "NT$6,548", "六五四八", "陸伍肆捌"}},
    {"10000.0", true, {"10,000.0", "一萬", "壹萬", "10,000.0元"}},
    {"0.50", true, {"0.50", "〇點五", "零點伍", "0.50元"}},
    {"0", true, {"0", "零元整", "〇", "0元", "NT$0", "零"}},
    {"007", true, {"007", "柒元整", "七", "007元", "NT$007", "〇〇七", "零零柒"}},
    {"10000", true, {"10,000", "壹萬元整", "一萬", "10,000元", "NT$10,000", "一〇〇〇〇", "壹零零零零"}},
    {"100000001", true, {"100,000,001", "壹億零壹元整", "一億〇一", "100,000,001元", "NT$100,000,001", "一〇〇〇〇〇〇〇一", "壹零零零零零零零壹"}},
    {"1234567890123456", true, {"1,234,567,890,123,456", "壹仟貳佰參拾肆兆伍仟陸佰柒拾捌億玖仟零壹拾貳萬參仟肆佰伍拾陸元整", "一千二百三十四兆五千六百七十八億九千〇一十二萬三千四百五十六", "1,234,567,890,123,456元", "NT$1,234,567,890,123,456", "一二三四五六七八九〇一二三四五六", "壹貳參肆伍陸柒捌玖零壹貳參肆伍陸"}},
    {"12345678901234567", true, {}},
    {".5", false, {}},
    {"-5", false, {}},
    {"6,548", false, {}},
    {"1e5", false, {}},
    {"", false, {}},
    {"10*15/6", true, {"25", "10*15/6=25", "10×15÷6=25", "25元", "貳拾伍元整", "二十五"}},
    {"(2+3)*3", true, {"15", "(2+3)*3=15", "(2+3)×3=15", "15元", "壹拾伍元整", "一十五"}},
    {"(2+3)*3=", true, {"15", "(2+3)*3=15", "(2+3)×3=15", "15元", "壹拾伍元整", "一十五"}},
    {"1/0", true, {}},
    {"2/30", true, {"0.066667", "2/30=0.066667", "2÷30=0.066667"}},
    {"2/29", true, {"0.068966", "2/29=0.068966", "2÷29=0.068966"}},
    {"2/3", true, {"2月3日（星期二）", "二月三日（星期二）", "0.666667", "2/3=0.666667", "2÷3=0.666667"}},
    {"1/3", true, {"1月3日（星期六）", "一月三日（星期六）", "0.333333", "1/3=0.333333", "1÷3=0.333333"}},
    {"2023.9/7", true, {"289.128571", "2023.9/7=289.128571", "2023.9÷7=289.128571"}},
    {"2/3*3", true, {"2", "2/3*3=2", "2÷3×3=2", "2元", "貳元整", "二"}},
    {"0.1+0.2", true, {"0.3", "0.1+0.2=0.3"}},
    {"99999999*99999999", true, {"9999999800000000", "99999999*99999999=9999999800000000", "99999999×99999999=9999999800000000", "9,999,999,800,000,000元", "玖仟玖佰玖拾玖兆玖仟玖佰玖拾捌億元整", "九千九百九十九兆九千九百九十八億"}},
    {"0*-1", true, {"0", "0*-1=0", "0×-1=0", "0元", "零元整", "〇"}},
    {"3.5*2", true, {"7", "3.5*2=7", "3.5×2=7", "7元", "柒元整", "七"}},
    {"1.2.3", true, {"1912年2月3日（星期六）", "一九一二年二月三日（星期六）", "中華民國1年2月3日（星期六）", "民國1年2月3日", "西元1912年2月3日（星期六）", "1912/02/03", "1912-02-03", "2月3日（星期六）", "二月三日（星期六）"}},
    {"1..2", true, {}},
    {"0.1.1", true, {"1911年1月1日（星期日）", "一九一一年一月一日（星期日）", "中華民國0年1月1日（星期日）", "民國0年1月1日", "西元1911年1月1日（星期日）", "1911/01/01", "1911-01-01", "1月1日（星期日）", "一月一日（星期日）"}},
    {"1/1/1", true, {"1912年1月1日（星期一）", "一九一二年一月一日（星期一）", "中華民國1年1月1日（星期一）", "民國1年1月1日", "西元1912年1月1日（星期一）", "1912/01/01", "1912-01-01", "1月1日（星期一）", "一月一日（星期一）", "1", "1/1/1=1", "1÷1÷1=1", "1元", "壹元整", "一"}},
    {"13/1", true, {"13", "13/1=13", "13÷1=13", "13元", "壹拾參元整", "一十三"}},
    {"2024.2.29", true, {"2024年2月29日（星期四）", "二〇二四年二月二十九日（星期四）", "中華民國113年2月29日（星期四）", "民國113年2月29日", "西元2024年2月29日（星期四）", "2024/02/29", "2024-02-29", "2月29日（星期四）", "二月二十九日（星期四）"}},
    {"1500.2.29", true, {"1500年2月29日（星期六）", "一五〇〇年二月二十九日（星期六）", "中華民國-411年2月29日（星期六）", "民國-411年2月29日", "西元1500年2月29日（星期六）", "1500/02/29", "1500-02-29", "2月29日（星期六）", "二月二十九日（星期六）"}},
    {"1700.2.29", true, {}},
    {"1582.10.4", true, {"1582年10月4日（星期四）", "一五八二年十月四日（星期四）", "中華民國-329年10月4日（星期四）", "民國-329年10月4日", "西元1582年10月4日（星期四）", "1582/10/04", "1582-10-04", "10月4日（星期四）", "十月四日（星期四）"}},
    {"1582.10.10", true, {}},
    {"1582.10.15", true, {"1582年10月15日（星期五）", "一五八二年十月十五日（星期五）", "中華民國-329年10月15日（星期五）", "民國-329年10月15日", "西元1582年10月15日（星期五）", "1582/10/15", "1582-10-15", "10月15日（星期五）", "十月十五日（星期五）"}},
    {"1000.1.1", true, {"1000年1月1日（星期一）", "一〇〇〇年一月一日（星期一）", "中華民國-911年1月1日（星期一）", "民國-911年1月1日", "西元1000年1月1日（星期一）", "1000/01/01", "1000-01-01", "1月1日（星期一）", "一月一日（星期一）"}},
    {"0999.1.1", true, {"999年1月1日（星期日）", "九九九年一月一日（星期日）", "中華民國-912年1月1日（星期日）", "民國-912年1月1日", "西元999年1月1日（星期日）", "0999/01/01", "0999-01-01", "1月1日（星期日）", "一月一日（星期日）"}},
    {"9999.12.31", true, {"9999年12月31日（星期五）", "九九九九年十二月三十一日（星期五）", "中華民國8088年12月31日（星期五）", "民國8088年12月31日", "西元9999年12月31日（星期五）", "9999/12/31", "9999-12-31", "12月31日（星期五）", "十二月三十一日（星期五）"}},
    {"10000.1.1", true, {}},
    {"0000.1.1", true, {}},
    {"99999999999999999999.1.1", true, {}},
    {"99999999999999999999:00", true, {}},
    {"9/7=", true, {"9月7日（星期一）", "九月七日（星期一）", "1.285714", "9/7=1.285714", "9÷7=1.285714"}},
    {"9/7==", true, {"9月7日（星期一）", "九月七日（星期一）", "1.285714", "9/7=1.285714", "9÷7=1.285714"}},
    {"（2＋３）＊3＝", true, {"15", "(2+3)*3=15", "(2+3)×3=15", "15元", "壹拾伍元整", "一十五"}},
    {"３。５＊２", true, {"7", "3.5*2=7", "3.5×2=7", "7元", "柒元整", "七"}},
    {"1。5", true, {"1.5", "一點五", "壹點伍", "1.5元"}},
    {"12：30", true, {"12時30分", "下午十二點三十分", "下午12點30分", "12:30 PM", "PM 12:30", "12:30"}},
    {"2023．9．7", true, {"2023年9月7日（星期四）", "二〇二三年九月七日（星期四）", "中華民國112年9月7日（星期四）", "民國112年9月7日", "西元2023年9月7日（星期四）", "2023/09/07", "2023-09-07", "9月7日（星期四）", "九月七日（星期四）"}},
    {"5.+1", true, {"6", "5.+1=6", "6元", "陸元整", "六"}},
    {"1+.5", true, {"1.5", "1+.5=1.5"}},
    {"(1", true, {}},
    {"((2))", true, {}},
    {"1-(-1)", true, {"2", "1-(-1)=2", "2元", "貳元整", "二"}},
    {"1+", true, {}},
    {"1/10000000", true, {"0.", "1/10000000=0.", "1÷10000000=0."}},
    {"1/1000000", true, {"0.000001", "1/1000000=0.000001", "1÷1000000=0.000001"}},
    {"123456789*987654321", true, {"121932631112635264.", "123456789*987654321=121932631112635264.", "123456789×987654321=121932631112635264."}},
    {"1000000000000000*10", true, {"10000000000000000.", "1000000000000000*10=10000000000000000.", "1000000000000000×10=10000000000000000."}},
    {"10−2", true, {"8", "10-2=8", "8元", "捌元整", "八"}},
    {"6×7", true, {"42", "6*7=42", "6×7=42", "42元", "肆拾貳元整", "四十二"}},
    {"8÷2", true, {"8月2日（星期日）", "八月二日（星期日）", "4", "8/2=4", "8÷2=4", "4元", "肆元整", "四"}},
    {"２０２６／１／２", true, {"2026年1月2日（星期五）", "二〇二六年一月二日（星期五）", "中華民國115年1月2日（星期五）", "民國115年1月2日", "西元2026年1月2日（星期五）", "2026/01/02", "2026-01-02", "1月2日（星期五）", "一月二日（星期五）", "1013", "2026/1/2=1013", "2026÷1÷2=1013", "1,013元", "壹仟零壹拾參元整", "一千〇一十三"}},
    {"1‧5", true, {"1.5", "一點五", "壹點伍", "1.5元"}},
};
const std::vector<std::pair<std::string, std::optional<std::string>>> kMacArithmetic = {
    {"（2＋3）＊3", "15"},
    {"3。5＊2", "7"},
    {"我算（2＋3", "5"},
    {"1+1=2", std::nullopt},
    {"abc", std::nullopt},
    {"12", std::nullopt},
    {"1/0", std::nullopt},
    {"9/7", "1.285714"},
    {"-3*2", "-6"},
    {"12:30+1", "31"},
    {"2023.9.7", std::nullopt},
    {"1-2-3", "-4"},
    {"(((", std::nullopt},
    {"1+2)", std::nullopt},
    {"2.5.1+1", "6.1"},
    {"1。5＋1", "2.5"},
    {"５‧５＋1", "6.5"},
    {"1+‧5", std::nullopt},
    {"", std::nullopt},
    {"我", std::nullopt},
};

TEST(QuickNumbersTest, SameAsMac) {
  for (const auto& c : kMacCases) {
    EXPECT_EQ(IsNumberCode(c.code), c.isNumberCode) << c.code;
    EXPECT_EQ(QuickNumberCandidates(c.code, kNow), c.candidates) << c.code;
  }
  for (const auto& [text, result] : kMacArithmetic) {
    EXPECT_EQ(ArithmeticResult(text), result) << text;
  }
}

}  // namespace
}  // namespace McBopomofo::SmartLearning
