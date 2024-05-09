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

#include "odb_component.h"
#include "odb_util.h"
#include "hash_eda.h"
// #include "board/board_package.hpp"

ODB_COMPONENT& COMPONENTS_MANAGER::AddComponent( FOOTPRINT* aFp, EDAData& eda_data )
{
    size_t hash = hash_fp_item( aFp, HASH_POS | REL_COORD );

    auto &pkg = eda_data.get_package( hash );

    auto &comp = m_compList.emplace_back( m_compList.size(), pkg.index );

    comp.m_center = ODB::AddXY( aFp->GetPosition() );

    if( aFp->GetOrientation() != ANGLE_0 )
    {
        comp.m_rot = ODB::Float2StrVal( 
                     aFp->GetOrientation().Normalize().AsDegrees() );
    }

    if( aFp->GetLayer() != F_Cu )
    {
        comp.m_mirror = wxT( "M" );
    }
        
    comp.m_comp_name = aFp->GetReference().ToAscii();
    comp.m_part_name = wxString::Format( "%s_%s_%s",
                                aFp->GetFPID().GetFullLibraryName(),
                                aFp->GetFPID().GetLibItemName().wx_str(),
                                aFp->GetValue() );
            
    return comp;
}
void COMPONENTS_MANAGER::Write(std::ostream &ost) const
{
    ost << "UNITS=MM" << std::endl;
    write_attributes(ost);
    for (const auto &comp : m_compList) {
        comp.Write(ost);
    }
}

void ODB_COMPONENT::Write( std::ostream &ost ) const
{
    ost << "# CMP " << m_index << std::endl;
    ost << "CMP " << m_pkg_ref << " " <<  m_center.first << " "
        << m_center.second << " " << m_rot << " "
        << m_mirror << " " << m_comp_name << " " << m_part_name;

    write_attributes(ost);

    ost << std::endl;

    for (const auto &toep : toeprints) {
        toep.Write(ost);
    }
    ost << "#" << std::endl;
}

void ODB_COMPONENT::Toeprint::Write( std::ostream &ost ) const
{
    ost << "TOP " << pin_num << " " << m_center.first << " "
        << m_center.second << " " << m_rot << " "
        << m_mirror << " " << net_num << " "
        << subnet_num << " " << toeprint_name << std::endl;
}

