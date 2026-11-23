#ifndef SRC_SMARTLEARNING_QUICKNUMBERS_H_
#define SRC_SMARTLEARNING_QUICKNUMBERS_H_

#include <ctime>
#include <optional>
#include <string>
#include <vector>

namespace McBopomofo::SmartLearning {

// Turns the full-width digits and operators typed in the Chinese mode, such
// as （２＋３）＊３＝, into half-width ones, and a "。" or "‧" between two
// digits into a decimal point. Other characters are kept. UTF-8.
std::string NormalizeNumberCode(const std::string& text);

// Whether the code looks like a number (a date, a time, an amount or an
// expression), so that it is not a text template: after normalizing, it
// starts with a digit or "(", and has only digits and "./-:+*()=".
bool IsNumberCode(const std::string& code);

// The formats of a number code, such as the dates for 2023.09.07, the times
// for 12:30, the amounts for 6548 or the result of 10*15/6, in the order
// they are listed, without duplicates. Empty when it is not a number code.
// The current year, used for a date without one such as 9/7, comes from
// `now` in the local time zone.
std::vector<std::string> QuickNumberCandidates(const std::string& code,
                                               std::time_t now);

// The result of the arithmetic expression at the end of the text before the
// "=" just typed, such as "15" for "我算（2＋3）＊3", or nullopt if there is
// none (no operator, division by zero, a malformed expression).
std::optional<std::string> ArithmeticResult(
    const std::string& expressionBeforeEquals);

// Evaluates +, -, *, / and parentheses on half-width text, such as
// (2+3)*3. The text must have at least one operator and be used up;
// division by zero or a result that is not finite gives nullopt.
std::optional<double> EvaluateArithmetic(const std::string& expression);

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_QUICKNUMBERS_H_
