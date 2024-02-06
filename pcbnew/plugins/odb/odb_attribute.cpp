#include "odb_attribute.h"
#include <sstream>
#include <iomanip>


// std::string make_legal_string_attribute(const std::string &n)
// {
//     std::string out;
//     out.reserve(n.size());
//     for (auto c : utf8_to_ascii(n)) {
//         if (isgraph(c) || c == ' ')
//             ;
//         else if (isspace(c))
//             c = ' ';
//         else
//             c = '_';
//         out.append(1, c);
//     }

//     return out;
// }


std::string AttributeProvider::double_to_string(double v, unsigned int n)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(n) << v;
    return oss.str();
}


static unsigned int get_or_create_text(std::map<std::string, unsigned int> &m, const std::string &t)
{
    if (m.count(t))
    {
        return m.at(t);
    }
    else {
        auto n = m.size();
        m.emplace(t, n);
        return n;
    }
}

unsigned int AttributeProvider::get_or_create_attribute_name(const std::string &name)
{
    return get_or_create_text(attribute_names, name);
}

unsigned int AttributeProvider::get_or_create_attribute_text(const std::string &name)
{
    return get_or_create_text(attribute_texts, name);
}

void ATTR_RECORD_WRITER::write_attributes(std::ostream &ost) const
{
    Once once;
    for(const auto &attr : attributes)
    {
        if (once())
            ost << " ;";
        else
            ost << ",";
        ost << attr.first;
        if (attr.second.size())
            ost << "=" << attr.second;
    }
}

void AttributeProvider::WriteAttributesName(std::ostream &ost, const std::string &prefix) const
{
    for (const auto &[name, n] : attribute_names) {
        ost << prefix << "@" << n << " " << name << std::endl;
    }
}

void AttributeProvider::WriteAttributesText(std::ostream &ost, const std::string &prefix) const
{
    for (const auto &[name, n] : attribute_texts) {
        ost << prefix << "&" << n << " " << name << std::endl;
    }
}

void AttributeProvider::write_attributes(std::ostream &ost, const std::string &prefix) const
{
    ost << std::endl << "#\n#Feature attribute names\n#" << std::endl;
    WriteAttributesName( ost );

    ost << std::endl << "#\n#Feature attribute text strings\n#" << std::endl;
    WriteAttributesText( ost );
}
