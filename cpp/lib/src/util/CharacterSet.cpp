#include "CharacterSet.h"

#include <boost/assign/list_of.hpp>
#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>

namespace opendnp3 {

    template <typename Enumeration>
    constexpr auto EnumAsInt(const Enumeration value)
        -> typename std::underlying_type<Enumeration>::type
    {
        static_assert(std::is_enum<Enumeration>::value, "EnumAsInt(): argument should be type of: enum or enum class");
        return static_cast<typename std::underlying_type<Enumeration>::type>(value);
    }

    using Relation_t = boost::bimap<
        boost::bimaps::unordered_set_of<std::string>,
        boost::bimaps::unordered_set_of<CharacterSet>
    >;

    static const Relation_t Relation = boost::assign::list_of<Relation_t::relation>
        ("UTF8", CharacterSet::UTF8)
        ("CP1251", CharacterSet::CP1251)
    ;

    CharacterSet FromString(const std::string& value, CharacterSet defaultValue) {
        const auto it = Relation.left.find(value);
        return it != Relation.left.end() ? it->second : defaultValue;
    }

    std::string ToString(CharacterSet value) {
        const auto it = Relation.right.find(value);
        if (it != Relation.right.end()) {
            return it->second;
        }
        return std::to_string(EnumAsInt(value));
    }

    std::ostream& operator<<(std::ostream& out, CharacterSet rhs) {
        out << ToString(rhs);
        return out;
    }

}