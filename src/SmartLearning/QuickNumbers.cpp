#include "QuickNumbers.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <set>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>

#include "ChineseNumbers.h"

namespace McBopomofo::SmartLearning {

namespace {

constexpr const char* kDigits[] = {"〇", "一", "二", "三", "四",
                                   "五", "六", "七", "八", "九"};
constexpr const char* kUppercaseDigits[] = {"零", "壹", "貳", "參", "肆",
                                            "伍", "陸", "柒", "捌", "玖"};
constexpr const char* kWeekdays[] = {"日", "一", "二", "三", "四", "五", "六"};

// Full-width digits and operators, typed in the Chinese mode, and their
// half-width ones.
constexpr std::pair<std::string_view, char> kHalfWidth[] = {
    {"（", '('}, {"）", ')'}, {"＋", '+'}, {"－", '-'}, {"−", '-'},
    {"＊", '*'}, {"×", '*'},  {"／", '/'}, {"÷", '/'},  {"．", '.'},
    {"：", ':'}, {"＝", '='}, {"０", '0'}, {"１", '1'}, {"２", '2'},
    {"３", '3'}, {"４", '4'}, {"５", '5'}, {"６", '6'}, {"７", '7'},
    {"８", '8'}, {"９", '9'},
};

bool IsAsciiDigit(char c) { return c >= '0' && c <= '9'; }

bool IsAllDigits(std::string_view text) {
  for (char c : text) {
    if (!IsAsciiDigit(c)) {
      return false;
    }
  }
  return true;
}

bool Contains(std::string_view set, char c) {
  return set.find(c) != std::string_view::npos;
}

// The UTF-8 text split into code points; a malformed byte is one on its own.
std::vector<std::string> SplitCodePoints(const std::string& text) {
  std::vector<std::string> result;
  size_t i = 0;
  while (i < text.size()) {
    auto lead = static_cast<unsigned char>(text[i]);
    size_t length = 1;
    if (lead >= 0xf0 && lead < 0xf8) {
      length = 4;
    } else if (lead >= 0xe0) {
      length = 3;
    } else if (lead >= 0xc0) {
      length = 2;
    }
    if (lead >= 0xf8 || i + length > text.size()) {
      length = 1;
    }
    for (size_t j = 1; j < length; j++) {
      if ((static_cast<unsigned char>(text[i + j]) & 0xc0) != 0x80) {
        length = 1;
        break;
      }
    }
    result.push_back(text.substr(i, length));
    i += length;
  }
  return result;
}

bool IsAsciiDigitCodePoint(const std::string& c) {
  return c.size() == 1 && IsAsciiDigit(c[0]);
}

std::vector<std::string> NormalizedCodePoints(const std::string& text) {
  std::vector<std::string> characters = SplitCodePoints(text);
  for (auto& c : characters) {
    for (const auto& [full, half] : kHalfWidth) {
      if (c == full) {
        c = std::string(1, half);
        break;
      }
    }
  }
  // "." typed between digits in the Chinese mode gives "。", like 3。5.
  for (size_t i = 1; i + 1 < characters.size(); i++) {
    if ((characters[i] == "。" || characters[i] == "‧") &&
        IsAsciiDigitCodePoint(characters[i - 1]) &&
        IsAsciiDigitCodePoint(characters[i + 1])) {
      characters[i] = ".";
    }
  }
  return characters;
}

std::string Join(const std::vector<std::string>& parts, size_t from = 0) {
  std::string result;
  for (size_t i = from; i < parts.size(); i++) {
    result += parts[i];
  }
  return result;
}

std::vector<std::string> Split(std::string_view text,
                               std::string_view separators,
                               bool omitEmpty) {
  std::vector<std::string> parts;
  std::string current;
  for (char c : text) {
    if (Contains(separators, c)) {
      if (!omitEmpty || !current.empty()) {
        parts.push_back(current);
      }
      current.clear();
    } else {
      current += c;
    }
  }
  if (!omitEmpty || !current.empty()) {
    parts.push_back(current);
  }
  return parts;
}

// A 64-bit integer from digits, or nullopt when it does not fit.
std::optional<int64_t> ParseInt(const std::string& digits) {
  int64_t value = 0;
  auto [ptr, ec] =
      std::from_chars(digits.data(), digits.data() + digits.size(), value);
  if (ec != std::errc() || ptr != digits.data() + digits.size()) {
    return std::nullopt;
  }
  return value;
}

// 1 to 99 in Chinese, such as 十二 and 三十.
std::string ChineseNumber(int number) {
  if (number < 10) {
    return kDigits[number];
  }
  int tens = number / 10;
  int ones = number % 10;
  return std::string(tens == 1 ? "" : kDigits[tens]) + "十" +
         (ones == 0 ? "" : kDigits[ones]);
}

std::string DigitByDigit(const std::string& digits,
                         const char* const* table) {
  std::string result;
  for (char c : digits) {
    result += table[c - '0'];
  }
  return result;
}

std::string GroupedNumber(const std::string& intPart) {
  std::string result;
  size_t length = intPart.size();
  for (size_t i = 0; i < length; i++) {
    if (i > 0 && (length - i) % 3 == 0) {
      result += ',';
    }
    result += intPart[i];
  }
  return result;
}

std::string Formatted(double value) {
  if (value == std::round(value) && std::fabs(value) < 1e16) {
    return std::to_string(static_cast<int64_t>(value));
  }
  char buffer[512];
  std::snprintf(buffer, sizeof(buffer), "%.6f", value);
  std::string text(buffer);
  while (!text.empty() && text.back() == '0') {
    text.pop_back();
  }
  return text;
}

std::string Format(const char* format, int a, int b) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), format, a, b);
  return buffer;
}

std::string Format(const char* format, int a, int b, int c) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), format, a, b, c);
  return buffer;
}

// The days since an epoch for a date in the calendar used for dates: the
// Julian calendar before 1582-10-15 and the Gregorian one from it, the days
// in between not existing. nullopt for a date that does not exist, such as
// 2/30.
std::optional<int64_t> JulianDayNumber(int64_t year, int month, int day) {
  auto beforeOrOn = [&](int y, int m, int d) {
    return std::make_tuple(year, month, day) <=
           std::make_tuple(static_cast<int64_t>(y), m, d);
  };
  bool julian = beforeOrOn(1582, 10, 4);
  if (!julian && beforeOrOn(1582, 10, 14)) {
    return std::nullopt;
  }
  bool leap = julian ? year % 4 == 0
                     : (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
  static constexpr int kDaysInMonth[] = {31, 28, 31, 30, 31, 30,
                                         31, 31, 30, 31, 30, 31};
  int days = kDaysInMonth[month - 1] + (month == 2 && leap ? 1 : 0);
  if (day > days) {
    return std::nullopt;
  }
  int64_t a = (14 - month) / 12;
  int64_t y = year + 4800 - a;
  int64_t m = month + 12 * a - 3;
  int64_t base = day + (153 * m + 2) / 5 + 365 * y + y / 4;
  return julian ? base - 32083 : base - y / 100 + y / 400 - 32045;
}

int CurrentYear(std::time_t now) {
  std::tm local{};
  localtime_r(&now, &local);
  return local.tm_year + 1900;
}

std::vector<std::string> Date(const std::string& code, std::time_t now) {
  std::vector<std::string> parts = Split(code, "./-", /*omitEmpty=*/true);
  std::string separators;
  for (char c : code) {
    if (Contains("./-", c)) {
      separators += c;
    }
  }
  if (separators.empty() ||
      separators.find_first_not_of(separators[0]) != std::string::npos) {
    return {};
  }
  for (const auto& part : parts) {
    if (part.empty() || !IsAllDigits(part)) {
      return {};
    }
  }
  int64_t year = 0;
  bool hasYear = true;
  std::optional<int64_t> month;
  std::optional<int64_t> day;
  if (parts.size() == 3) {
    auto y = ParseInt(parts[0]);
    month = ParseInt(parts[1]);
    day = ParseInt(parts[2]);
    if (!y || !month || !day) {
      return {};
    }
    // A year of the Republic of China, such as 112.9.7.
    year = parts[0].size() <= 3 ? *y + 1911 : *y;
  } else if (parts.size() == 2 && separators == "/") {
    // Like 9/7, which is also a division.
    month = ParseInt(parts[0]);
    day = ParseInt(parts[1]);
    if (!month || !day) {
      return {};
    }
    year = CurrentYear(now);
    hasYear = false;
  } else {
    return {};
  }
  if (*month < 1 || *month > 12 || *day < 1 || *day > 31 || year < 1 ||
      year > 9999) {
    return {};
  }
  int m = static_cast<int>(*month);
  int d = static_cast<int>(*day);
  auto julianDay = JulianDayNumber(year, m, d);
  if (!julianDay) {
    return {};
  }
  std::string weekday =
      std::string("（星期") + kWeekdays[(*julianDay + 1) % 7] + "）";
  std::string monthDay = std::to_string(m) + "月" + std::to_string(d) + "日";
  std::string chineseMonthDay =
      ChineseNumber(m) + "月" + ChineseNumber(d) + "日";
  std::vector<std::string> results;
  if (hasYear) {
    std::string yearText = std::to_string(year);
    std::string republicYear = std::to_string(year - 1911);
    int y = static_cast<int>(year);
    results = {
        yearText + "年" + monthDay + weekday,
        DigitByDigit(yearText, kDigits) + "年" + chineseMonthDay + weekday,
        "中華民國" + republicYear + "年" + monthDay + weekday,
        "民國" + republicYear + "年" + monthDay,
        "西元" + yearText + "年" + monthDay + weekday,
        Format("%04d/%02d/%02d", y, m, d),
        Format("%04d-%02d-%02d", y, m, d),
    };
  }
  results.push_back(monthDay + weekday);
  results.push_back(chineseMonthDay + weekday);
  return results;
}

std::vector<std::string> Time(const std::string& code) {
  std::vector<std::string> parts = Split(code, ":", /*omitEmpty=*/false);
  if (parts.size() != 2) {
    return {};
  }
  for (const auto& part : parts) {
    if (part.empty() || !IsAllDigits(part)) {
      return {};
    }
  }
  if (parts[1].size() != 2) {
    return {};
  }
  auto hourValue = ParseInt(parts[0]);
  auto minuteValue = ParseInt(parts[1]);
  if (!hourValue || !minuteValue || *hourValue < 0 || *hourValue > 23 ||
      *minuteValue < 0 || *minuteValue > 59) {
    return {};
  }
  int hour = static_cast<int>(*hourValue);
  int minute = static_cast<int>(*minuteValue);
  std::string period = hour < 12 ? "上午" : "下午";
  int hour12 = hour % 12 == 0 ? 12 : hour % 12;
  std::string ampm = hour < 12 ? "AM" : "PM";
  std::string chineseMinute =
      minute == 0
          ? ""
          : std::string(minute < 10 ? "零" : "") + ChineseNumber(minute) + "分";
  std::string minuteText = minute == 0 ? "" : std::to_string(minute) + "分";
  std::string hhmm = Format("%d:%02d", hour12, minute);
  return {
      std::to_string(hour) + "時" + std::to_string(minute) + "分",
      period + ChineseNumber(hour12) + "點" + chineseMinute,
      period + std::to_string(hour12) + "點" + minuteText,
      hhmm + " " + ampm,
      ampm + " " + hhmm,
      Format("%02d:%02d", hour, minute),
  };
}

std::string Chinese(const std::string& intPart, const std::string& decPart,
                    ChineseNumbers::ChineseNumberCase digitCase) {
  return ChineseNumbers::Generate(intPart, decPart, digitCase);
}

std::vector<std::string> Amount(const std::string& code) {
  std::vector<std::string> parts = Split(code, ".", /*omitEmpty=*/false);
  if (parts.size() > 2 || parts[0].empty() || parts[0].size() > 16) {
    return {};
  }
  const std::string& intPart = parts[0];
  std::string decPart = parts.size() == 2 ? parts[1] : "";
  std::string grouped =
      GroupedNumber(intPart) + (decPart.empty() ? "" : "." + decPart);
  std::string lowercase =
      Chinese(intPart, decPart, ChineseNumbers::ChineseNumberCase::LOWERCASE);
  std::string uppercase =
      Chinese(intPart, decPart, ChineseNumbers::ChineseNumberCase::UPPERCASE);
  if (decPart.empty()) {
    return {
        grouped,
        uppercase + "元整",
        lowercase,
        grouped + "元",
        "NT$" + grouped,
        DigitByDigit(intPart, kDigits),
        DigitByDigit(intPart, kUppercaseDigits),
    };
  }
  return {grouped, lowercase, uppercase, grouped + "元"};
}

std::string ReplaceAll(std::string text, char from, const std::string& to) {
  std::string result;
  for (char c : text) {
    if (c == from) {
      result += to;
    } else {
      result += c;
    }
  }
  return result;
}

std::vector<std::string> Arithmetic(const std::string& code, double value) {
  std::string result = Formatted(value);
  std::string pretty = ReplaceAll(ReplaceAll(code, '*', "×"), '/', "÷");
  std::vector<std::string> results = {result, code + "=" + result,
                                       pretty + "=" + result};
  if (value >= 0 && value == std::round(value) && value < 1e16) {
    std::string intPart = std::to_string(static_cast<int64_t>(value));
    results.push_back(GroupedNumber(intPart) + "元");
    results.push_back(
        Chinese(intPart, "", ChineseNumbers::ChineseNumberCase::UPPERCASE) +
        "元整");
    results.push_back(
        Chinese(intPart, "", ChineseNumbers::ChineseNumberCase::LOWERCASE));
  }
  return results;
}

// Digits with at most one decimal point, such as 3.5, .5 or 5., as a double.
// A number too large for a double is infinity, and one too small 0.
std::optional<double> ParseNumber(std::string_view text) {
  if (text.empty() || text == ".") {
    return std::nullopt;
  }
  double value = 0;
  auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(),
                                   value, std::chars_format::fixed);
  if (ptr != text.data() + text.size()) {
    return std::nullopt;
  }
  if (ec == std::errc::result_out_of_range) {
    size_t point = text.find('.');
    std::string_view intPart = text.substr(0, point);
    bool large = intPart.find_first_not_of('0') != std::string_view::npos;
    return large ? HUGE_VAL : 0.0;
  }
  if (ec != std::errc()) {
    return std::nullopt;
  }
  return value;
}

// expression = term (('+' | '-') term)*
// term       = factor (('*' | '/') factor)*
// factor     = '-' factor | '(' expression ')' | number
class Parser {
 public:
  explicit Parser(std::string_view text) : text_(text) {}

  std::optional<double> parse() {
    auto value = expression();
    if (!value || index_ != text_.size() || !std::isfinite(*value)) {
      return std::nullopt;
    }
    return value;
  }

 private:
  std::optional<double> expression() {
    auto value = term();
    if (!value) {
      return std::nullopt;
    }
    while (index_ < text_.size() && Contains("+-", text_[index_])) {
      char op = text_[index_++];
      auto rhs = term();
      if (!rhs) {
        return std::nullopt;
      }
      *value = op == '+' ? *value + *rhs : *value - *rhs;
    }
    return value;
  }

  std::optional<double> term() {
    auto value = factor();
    if (!value) {
      return std::nullopt;
    }
    while (index_ < text_.size() && Contains("*/", text_[index_])) {
      char op = text_[index_++];
      auto rhs = factor();
      if (!rhs) {
        return std::nullopt;
      }
      if (op == '/' && *rhs == 0) {
        return std::nullopt;
      }
      *value = op == '*' ? *value * *rhs : *value / *rhs;
    }
    return value;
  }

  std::optional<double> factor() {
    if (index_ >= text_.size()) {
      return std::nullopt;
    }
    if (text_[index_] == '-') {
      index_++;
      auto value = factor();
      if (!value) {
        return std::nullopt;
      }
      return -*value;
    }
    if (text_[index_] == '(') {
      index_++;
      auto value = expression();
      if (!value || index_ >= text_.size() || text_[index_] != ')') {
        return std::nullopt;
      }
      index_++;
      return value;
    }
    size_t start = index_;
    while (index_ < text_.size() &&
           (IsAsciiDigit(text_[index_]) || text_[index_] == '.')) {
      index_++;
    }
    return ParseNumber(text_.substr(start, index_ - start));
  }

  std::string_view text_;
  size_t index_ = 0;
};

}  // namespace

std::string NormalizeNumberCode(const std::string& text) {
  return Join(NormalizedCodePoints(text));
}

bool IsNumberCode(const std::string& code) {
  std::string normalized = NormalizeNumberCode(code);
  if (normalized.empty() ||
      !(IsAsciiDigit(normalized[0]) || normalized[0] == '(')) {
    return false;
  }
  for (char c : normalized) {
    if (!IsAsciiDigit(c) && !Contains("./-:+*()=", c)) {
      return false;
    }
  }
  return true;
}

std::vector<std::string> QuickNumberCandidates(const std::string& code,
                                               std::time_t now) {
  if (!IsNumberCode(code)) {
    return {};
  }
  // An expression may end with "=", like (2+3)*3=.
  std::string normalized = NormalizeNumberCode(code);
  while (!normalized.empty() && normalized.back() == '=') {
    normalized.pop_back();
  }
  std::vector<std::string> results = Date(normalized, now);
  for (auto& time : Time(normalized)) {
    results.push_back(std::move(time));
  }
  bool numberOnly = normalized.find_first_not_of("0123456789.") ==
                    std::string::npos;
  if (numberOnly) {
    for (auto& amount : Amount(normalized)) {
      results.push_back(std::move(amount));
    }
  } else if (auto value = EvaluateArithmetic(normalized)) {
    for (auto& text : Arithmetic(normalized, *value)) {
      results.push_back(std::move(text));
    }
  }
  std::vector<std::string> unique;
  std::set<std::string> seen;
  for (auto& result : results) {
    if (seen.insert(result).second) {
      unique.push_back(std::move(result));
    }
  }
  return unique;
}

std::optional<std::string> ArithmeticResult(
    const std::string& expressionBeforeEquals) {
  std::vector<std::string> characters =
      NormalizedCodePoints(expressionBeforeEquals);
  size_t start = characters.size();
  while (start > 0) {
    const std::string& c = characters[start - 1];
    if (c.size() != 1 || !(IsAsciiDigit(c[0]) || Contains("./-+*()", c[0]))) {
      break;
    }
    start--;
  }
  // The longest expression that makes sense, such as 2+3 in "(2+3".
  for (size_t i = start; i < characters.size(); i++) {
    if (auto value = EvaluateArithmetic(Join(characters, i))) {
      return Formatted(*value);
    }
  }
  return std::nullopt;
}

std::optional<double> EvaluateArithmetic(const std::string& expression) {
  // At least one operator, or it is just a number.
  if (expression.find_first_of("+-*/") == std::string::npos) {
    return std::nullopt;
  }
  return Parser(expression).parse();
}

}  // namespace McBopomofo::SmartLearning
