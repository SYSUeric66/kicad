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

    if( aFp->GetOrientation() != EDA_ANGLE::m_Angle0 )
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
void COMPONENTS_MANAGER::write(std::ostream &ost) const
{
    ost << "UNITS=MM" << std::endl;
    write_attributes(ost);
    for (const auto &comp : m_compList) {
        comp.write(ost);
    }
}

void ODB_COMPONENT::write( std::ostream &ost ) const
{
    ost << "# CMP " << m_index << std::endl;
    ost << "CMP " << m_pkg_ref << " " <<  m_center.first << " "
        << m_center.second << " " << m_rot << " "
        << m_mirror << " " << m_comp_name << " " << m_part_name;

    write_attributes(ost);

    ost << std::endl;

    for (const auto &toep : toeprints) {
        toep.write(ost);
    }
    ost << "#" << std::endl;
}

void ODB_COMPONENT::Toeprint::write( std::ostream &ost ) const
{
    ost << "TOP " << pin_num << " " << m_center.first << " "
        << m_center.second << " " << m_rot << " "
        << m_mirror << " " << net_num << " "
        << subnet_num << " " << toeprint_name << std::endl;
}

