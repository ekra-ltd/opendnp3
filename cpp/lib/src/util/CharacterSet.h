#pragma once

#include <cstdint>
#include <string>

namespace opendnp3 {

    enum class CharacterSet : uint8_t {
        UTF8,
        CP1251
    };

    /** \brief Получение значения перечисления из строкового представления.
     *  \param[in] value        строковое представление значения;
     *  \param[in] defaultValue значение по умолчанию.
     *  \return значение перечисления, если преобразование возможно,
                иначе значение по умолчанию. */
    CharacterSet FromString(const std::string& value, CharacterSet defaultValue);

    /** \brief Преобразование перечисления в строковое представление.
     *  \param[in] value значение.
     *  \return строковое представление значения. */
    std::string ToString(CharacterSet value);

    /** \brief Вывод строкового представления значения перечисления в поток.
     *  \param[in] out поток для вывода;
     *  \param[in] rhs выводимое значение.
     *  \return ссылка на поток. */
    std::ostream& operator<<(std::ostream& out, CharacterSet rhs);

}