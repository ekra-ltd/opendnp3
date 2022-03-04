#include "EncodingConverter.h"

#include <boost/locale/encoding.hpp>
#include <boost/locale/util.hpp>

namespace opendnp3 {

    std::string EncodingConverter::FromSystem(const std::string& str, CharacterSet to) {
        return boost::locale::conv::between(str, ToString(to), GetSystemEncoding());
    }

    std::string EncodingConverter::ToSystem(const std::string& str, CharacterSet from) {
        return boost::locale::conv::between(str, GetSystemEncoding(), ToString(from));
    }

    const std::string& EncodingConverter::GetSystemEncoding() {
        static auto systemEncoding = []() {
            const auto sysLoc = boost::locale::util::get_system_locale();
            const auto i = sysLoc.find('.');
            if (i != std::string::npos) {
                return sysLoc.substr(i + 1);
            }
            else {
#ifdef _WINDOWS
                return ToString(CharacterSet::CP1251);
#else
                return ToString(CharacterSet::UTF8);
#endif
            }
        }();
        return systemEncoding;
    }

}