// utils.cpp — утилита проверки данных для мессенджера Nova.
//
// Содержит те же правила, по которым работает вход в приложении:
//   * normalizePhone  — приведение номера к виду «+код» + цифры;
//   * samePhone       — сравнение номеров по цифрам (точное или по окончанию);
//   * isOwnerPhone    — признак номера владельца;
//   * normalizeUsername / isValidUsername — юзернейм: без @, нижний регистр,
//     только латиница, цифры и «_», длина 3–20 символов.
//
// Сборка:   g++ -std=c++17 -Wall -Wextra -O2 utils.cpp -o utils
// Запуск:   ./utils test                      — встроенные тесты
//           ./utils phone <код> <номер>       — нормализация номера
//           ./utils same <номер1> <номер2>    — совпадают ли номера
//           ./utils user <юзернейм>           — проверка юзернейма

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace nova {

// Оставляет только цифры строки: пробелы, скобки, дефисы и плюсы удаляются.
std::string digitsOnly(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (std::isdigit(c)) out.push_back(static_cast<char>(c));
    }
    return out;
}

// Проверяет, начинается ли строка с префикса.
bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

// Проверяет, заканчивается ли строка суффиксом.
bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Приводит номер к единому виду "+код" + цифры.
//
// Правила:
//   1. Если номер введён с «+», код берётся из самого номера.
//   2. Иначе к цифрам номера добавляется код страны из выпадающего списка.
//   3. Если номер уже начинается с кода и цифр явно больше обычного,
//      код не дублируется: для «+7» обычный номер — 10 цифр, для остальных — 9.
std::string normalizePhone(const std::string& countryCode, const std::string& raw) {
    // Пробелы по краям не влияют на результат.
    std::string trimmed = raw;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);

    if (!trimmed.empty() && trimmed[0] == '+') {
        return "+" + digitsOnly(trimmed);
    }

    const std::string code = digitsOnly(countryCode);
    std::string digits = digitsOnly(trimmed);

    const std::size_t national = (code == "7") ? 10 : 9;
    if (!code.empty() && startsWith(digits, code) &&
        digits.size() >= code.size() + national) {
        digits.erase(0, code.size());
    }
    return "+" + code + digits;
}

// Сравнивает два номера по цифрам.
//
// Номера совпадают, если цифры равны. Если одна запись короче, она должна
// быть окончанием другой и содержать не менее 9 цифр — так находится номер,
// записанный с лишним кодом страны.
bool samePhone(const std::string& a, const std::string& b) {
    const std::string x = digitsOnly(a);
    const std::string y = digitsOnly(b);
    if (x.empty() || y.empty()) return false;
    if (x == y) return true;

    const std::string& shorter = x.size() < y.size() ? x : y;
    const std::string& longer = x.size() < y.size() ? y : x;
    return shorter.size() >= 9 && endsWith(longer, shorter);
}

// Признак номера владельца: цифры номера оканчиваются на известный хвост.
// Хвост задаётся здесь, а не в пользовательских данных.
bool isOwnerPhone(const std::string& phone) {
    const std::string d = digitsOnly(phone);
    return endsWith(d, "777111111") || endsWith(d, "7771111111");
}

// Приводит юзернейм: убирает пробелы по краям и ведущие «@», переводит
// латиницу в нижний регистр.
std::string normalizeUsername(const std::string& raw) {
    std::string s = raw;
    s.erase(0, s.find_first_not_of(" \t"));
    s.erase(s.find_last_not_of(" \t") + 1);
    while (!s.empty() && s[0] == '@') s.erase(0, 1);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

// Проверяет юзернейм после нормализации: от 3 до 20 символов,
// только латинские буквы, цифры и «_».
bool isValidUsername(const std::string& raw) {
    const std::string s = normalizeUsername(raw);
    if (s.size() < 3 || s.size() > 20) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    });
}

}  // namespace nova

// ----- Встроенные тесты -------------------------------------------------------

namespace {

int failures = 0;

void check(bool ok, const std::string& name) {
    std::cout << (ok ? "[ok]   " : "[FAIL] ") << name << '\n';
    if (!ok) ++failures;
}

void runTests() {
    using namespace nova;

    // Нормализация номера.
    check(normalizePhone("+7", "777111111") == "+7777111111",
          "номер +7 + 777111111 даёт +7777111111");
    check(normalizePhone("+7", "+7 (777) 111-111") == "+7777111111",
          "номер с пробелами, скобками и дефисами очищается");
    check(normalizePhone("+7", "77777111111") == "+77777111111",
          "код +7 не дублируется, если номер длиннее обычного");
    check(normalizePhone("+380", "50 123-45-67") == "+380501234567",
          "украинский номер без пробелов");
    check(normalizePhone("+380", "380501234567") == "+380501234567",
          "код +380 не дублируется");
    check(normalizePhone("+777", "111111") == "+777111111",
          "особый код +777 сохраняется");

    // Сравнение номеров.
    check(samePhone("+777777111111", "+7777111111"),
          "номер с лишней семёркой находится по окончанию");
    check(samePhone("+380501234567", "+380 50 123 45 67"),
          "одинаковые номера с разным оформлением совпадают");
    check(!samePhone("+380501234567", "+7777111111"),
          "разные номера не совпадают");
    check(!samePhone("", "+7777111111"), "пустой номер ни с чем не совпадает");
    check(!samePhone("+12345678", "+1234"),
          "слишком короткий хвост (меньше 9 цифр) не считается совпадением");

    // Владелец.
    check(isOwnerPhone("+777777111111"), "номер владельца распознаётся");
    check(isOwnerPhone("+7771111111"), "второй вариант номера владельца распознаётся");
    check(!isOwnerPhone("+380501234567"), "чужой номер не считается владельцем");

    // Юзернейм.
    check(normalizeUsername("  @ArbUxiv ") == "arbuxiv", "юзернейм: @, пробелы и регистр убираются");
    check(isValidUsername("arbuxiv"), "юзернейм arbuxiv допустим");
    check(isValidUsername("@User_1"), "юзернейм с @ и цифрами допустим");
    check(!isValidUsername("ab"), "юзернейм короче 3 символов не допускается");
    check(!isValidUsername(std::string(21, 'a')), "юзернейм длиннее 20 символов не допускается");
    check(!isValidUsername("имя"), "юзернейм из кириллицы не допускается");
    check(!isValidUsername("bad-name"), "дефис в юзернейме не допускается");
}

void printUsage() {
    std::cerr << "Использование:\n"
              << "  utils test\n"
              << "  utils phone <код> <номер>\n"
              << "  utils same <номер1> <номер2>\n"
              << "  utils user <юзернейм>\n";
}

}  // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) {
        printUsage();
        return 2;
    }

    const std::string& cmd = args[0];
    if (cmd == "test") {
        runTests();
        std::cout << (failures == 0 ? "Все тесты пройдены.\n"
                                    : "Есть проваленные тесты: " + std::to_string(failures) + "\n");
        return failures == 0 ? 0 : 1;
    }
    if (cmd == "phone" && args.size() == 3) {
        std::cout << nova::normalizePhone(args[1], args[2]) << '\n';
        return 0;
    }
    if (cmd == "same" && args.size() == 3) {
        std::cout << (nova::samePhone(args[1], args[2]) ? "совпадают" : "не совпадают") << '\n';
        return 0;
    }
    if (cmd == "user" && args.size() == 2) {
        const bool ok = nova::isValidUsername(args[1]);
        std::cout << nova::normalizeUsername(args[1]) << (ok ? " — допустим\n" : " — недопустим\n");
        return ok ? 0 : 1;
    }

    printUsage();
    return 2;
}
