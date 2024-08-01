/**
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2023 KiCad Developers, see AUTHORS.txt for contributors.
 * Author: SYSUEric <jzzhuang666@gmail.com>.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "odb_attribute.h"
#include <sstream>
#include <iomanip>


std::string ATTR_MANAGER::double_to_string(double v, unsigned int n)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(n) << v;
    return oss.str();
}


static unsigned int get_or_create_text(std::map<std::string, unsigned int> &m, const std::string &t)
{
    if( m.count(t) )
    {
        return m.at(t);
    }
    else
    {
        auto n = m.size();
        m.emplace(t, n);
        return n;
    }
}

unsigned int ATTR_MANAGER::get_or_create_attribute_name( const wxString& name )
{
    return get_or_create_text( attribute_names, name.Lower().ToStdString() );
}

unsigned int ATTR_MANAGER::get_or_create_attribute_text( const wxString& name )
{
    return get_or_create_text( attribute_texts, name.Upper().ToStdString() );
}

void ATTR_RECORD_WRITER::write_attributes(std::ostream &ost) const
{
    Once once;
    for(const auto &attr : attributes)
    {
        if (once())
            ost << ";";
        else
            ost << ",";
        ost << attr.first;
        if (attr.second.size())
            ost << "=" << attr.second;
    }
}

void ATTR_MANAGER::WriteAttributesName(std::ostream &ost, const std::string &prefix) const
{
    for(const auto &[name, n] : attribute_names)
    {
        ost << prefix << "@" << n << " " << name << std::endl;
    }
}

void ATTR_MANAGER::WriteAttributesText(std::ostream &ost, const std::string &prefix) const
{
    for(const auto &[name, n] : attribute_texts)
    {
        ost << prefix << "&" << n << " " << name << std::endl;
    }
}

void ATTR_MANAGER::write_attributes(std::ostream &ost, const std::string &prefix) const
{
    ost << std::endl << "#\n#Feature attribute names\n#" << std::endl;
    WriteAttributesName( ost );

    ost << std::endl << "#\n#Feature attribute text strings\n#" << std::endl;
    WriteAttributesText( ost );
}
