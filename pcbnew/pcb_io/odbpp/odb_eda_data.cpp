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

#include "odb_eda_data.h"
#include "hash_eda.h"
#include "netinfo.h"
#include "odb_feature.h"
#include "base_units.h"


EDAData::EDAData()
{
    auto &x = nets_map.emplace( std::piecewise_construct, std::forward_as_tuple( 0 ),
                               std::forward_as_tuple( nets.size(), "$NONE$" ) )
                      .first->second;
    nets.push_back( &x );
}

EDAData::Net::Net(unsigned int i, const wxString &n) : index( i ), name( n )
{
}

void EDAData::Net::Write(std::ostream &ost) const
{
    ost << "NET " << name;
    write_attributes(ost);
    ost << std::endl;

    for (const auto &subnet : subnets) {
        subnet->Write(ost);
    }
}

// static std::string get_net_name(const Net &net)
// {
//     std::string net_name;
//     if (net.is_named())
//         return net.name;
//     else
//         return "$" + static_cast<std::string>(net.uuid);
// }

void EDAData::AddNET( const NETINFO_ITEM* aNet )
{
    if( nets_map.end() == nets_map.find( aNet->GetNetCode() ) )
    {
        auto &net = nets_map.emplace( std::piecewise_construct,
                    std::forward_as_tuple( aNet->GetNetCode() ),
                    std::forward_as_tuple( nets.size(), aNet->GetNetname() ) )
                        .first->second;

        nets.push_back( &net );
        //TODO: netname check
    }

}


void EDAData::Subnet::Write(std::ostream &ost) const
{
    ost << "SNT ";
    write_subnet(ost);
    ost << std::endl;
    for (const auto &fid : feature_ids)
    {
        fid.Write(ost);
    }
}

void EDAData::FeatureID::Write( std::ostream &ost ) const
{
    static const std::map<Type, std::string> type_map = {
            {Type::COPPER, "C"},
            {Type::HOLE, "H"},
    };
    ost << "FID " << type_map.at( type ) << " " << layer << " " << feature_id << std::endl;
}

void EDAData::SubnetVia::write_subnet(std::ostream &ost) const
{
    ost << "VIA";
}

void EDAData::SubnetTrace::write_subnet(std::ostream &ost) const
{
    ost << "TRC";
}

void EDAData::SubnetPlane::write_subnet(std::ostream &ost) const
{
    static const std::map<FillType, std::string> fill_type_map = 
    {
        {FillType::SOLID, "S"},
        {FillType::OUTLINE, "O"}
    };

    static const std::map<CutoutType, std::string> cutout_type_map = 
    {
        {CutoutType::CIRCLE, "C"},
        {CutoutType::RECT, "R"},
        {CutoutType::OCTAGON, "O"},
        {CutoutType::EXACT, "E"}
    };

    ost << "PLN " << fill_type_map.at( fill_type ) << " "
        << cutout_type_map.at( cutout_type ) << " " << fill_size;
}

void EDAData::SubnetToeprint::write_subnet( std::ostream &ost ) const
{
    static const std::map<Side, std::string> side_map = {
            {Side::BOTTOM, "B"},
            {Side::TOP, "T"},
    };
    ost << "TOP " << side_map.at( side ) << " " << comp_num << " " << toep_num;
}

void EDAData::Subnet::AddFeatureID( FeatureID::Type type,
                         const wxString& layer, unsigned int feature_id )
{
    feature_ids.emplace_back( type, m_edadata->GetLyrIdx( layer ), feature_id );
}


unsigned int EDAData::GetLyrIdx( const wxString& l )
{
    if( layers_map.count( l ) )
    {
        return layers_map.at(l);
    }
    else
    {
        auto n = layers_map.size();
        layers_map.emplace( l, n );
        layers.push_back( l );
        assert( layers.size() == layers_map.size() );
        return n;
    }
}


static std::unique_ptr<PKG_OUTLINE> InitOutlineContourByPolygon( const SHAPE_POLY_SET::POLYGON& aPolygon )
{
    auto r = std::make_unique<OUTLINE_CONTOUR>( aPolygon );

    // TODO: to check if need mirror and reverse ploygon when fp flip

    return r;
}

static std::unique_ptr<PKG_OUTLINE> InitOutlineByPolygon( const SHAPE_POLY_SET::POLYGON& aPolygon )
{
    return InitOutlineContourByPolygon( aPolygon );
}

void OUTLINE_SQUARE::Write( std::ostream &ost ) const
{                                     
    ost << "SQ " << ODB::Float2StrVal( m_ODBScale * m_center.x )
        << " " << ODB::Float2StrVal( m_ODBScale * m_center.y )
        << " " << ODB::Float2StrVal( m_ODBScale * m_halfSide ) << std::endl;
}

void OUTLINE_CIRCLE::Write( std::ostream &ost ) const
{
    ost << "CR " << ODB::Float2StrVal( m_ODBScale * m_center.x )
        << " " << ODB::Float2StrVal( m_ODBScale * m_center.y )
        << " " << ODB::Float2StrVal( m_ODBScale * m_radius ) << std::endl;
}

void OUTLINE_RECT::Write( std::ostream &ost ) const
{
    ost << "RC " << ODB::Float2StrVal( m_ODBScale * m_lower_left.x )
        << " " << ODB::Float2StrVal( m_ODBScale * m_lower_left.y )
        << " " << ODB::Float2StrVal( m_ODBScale * m_width )
        << " " << ODB::Float2StrVal( m_ODBScale * m_height ) << std::endl;
}

void OUTLINE_CONTOUR::Write( std::ostream &ost ) const
{
    ost << "CT" << std::endl;
    m_surfaces->WriteData( ost );
    ost << "CE" << std::endl;
}


void EDAData::AddPackage( const FOOTPRINT* aFp )
{
    // ODBPP only need unique Package in PKG record in eda/data file.
    // the PKG index can repeat to be ref in CMP record in component file.
    std::unique_ptr<FOOTPRINT> fp( static_cast<FOOTPRINT*>( aFp->Clone() ) );
    fp->SetParentGroup( nullptr );
    fp->SetPosition( { 0, 0 } );

    if( fp->GetLayer() != F_Cu )
        fp->Flip( fp->GetPosition(), false );

    fp->SetOrientation( ANGLE_0 );

    size_t hash = hash_fp_item( fp.get(), HASH_POS | REL_COORD );
    size_t pkg_index = packages_map.size();
    wxString fp_name = fp->GetFPID().GetLibItemName().wx_str();
    
    auto [ iter, success ] = packages_map.emplace( hash, Package( pkg_index, fp_name ) );
    if( !success )
    {
        return;
    }

    Package* pkg = &( iter->second );

    packages.push_back( pkg );

    BOX2I bbox = fp->GetBoundingBox();
    pkg->m_xmin = bbox.GetPosition().x;
    pkg->m_ymin = bbox.GetPosition().y;
    pkg->m_xmax = bbox.GetEnd().x;
    pkg->m_ymax = bbox.GetEnd().y;
    pkg->m_pitch = UINT64_MAX;

    if( fp->Pads().size() < 2 )
        pkg->m_pitch = pcbIUScale.mmToIU( 1.0 ); // placeholder value

    for( size_t i = 0; i < fp->Pads().size(); ++i )
    {
        const PAD* pad1 = fp->Pads()[i];
        for( size_t j = i + 1; j < fp->Pads().size(); ++j )
        {
            const PAD* pad2 = fp->Pads()[j];
            const uint64_t pin_dist = KiROUND( EuclideanNorm( pad1->GetCenter() - pad2->GetCenter() ) );
            pkg->m_pitch = std::min( pkg->m_pitch, pin_dist );
        }
    }

    const SHAPE_POLY_SET& courtyard = fp->GetCourtyard( F_CrtYd );
    const SHAPE_POLY_SET& courtyard_back = fp->GetCourtyard( B_CrtYd );
    SHAPE_POLY_SET pkg_outline;
    if( courtyard.OutlineCount() > 0 )
        pkg_outline = courtyard;

    if( courtyard_back.OutlineCount() > 0 )
    {
        pkg_outline = courtyard_back;
    }

    if( !courtyard.OutlineCount() && !courtyard_back.OutlineCount() )
    {
        pkg_outline = fp->GetBoundingHull();
    }

    // TODO: Here we put rect, square, and circle all as polygon
    if( pkg_outline.OutlineCount() > 0 )
    {
        for( int ii = 0; ii < pkg_outline.OutlineCount(); ++ii )
        {
            pkg->m_pkgOutlines.push_back( std::make_unique<OUTLINE_CONTOUR>( pkg_outline.Polygon(ii) ) );
        }
    }
    
    
    for( size_t i = 0; i < fp->Pads().size(); ++i )
    {
        const PAD* pad = fp->Pads()[i];
        pkg->AddPin( pad, i );
    }

    return;
}


EDAData::Pin& EDAData::Package::AddPin( const PAD* aPad, size_t aPinNum )
{
    wxString name = aPad->GetNumber();

    // Pins are required to have names, so if our pad doesn't have a name, we need to
    // generate one that is unique
    if( aPad->GetAttribute() == PAD_ATTRIB::NPTH )
        name = wxString::Format( "NPTH%zu", aPinNum );
    else if( name.empty() )
        name = wxString::Format( "PAD%zu", aPinNum );

    size_t hash = hash_fp_item( aPad, HASH_POS | REL_COORD );

    auto& pin = m_pinsMap.emplace(
        std::piecewise_construct, std::forward_as_tuple( hash ),
        std::forward_as_tuple( m_pinsList.size(), name ) )
            .first->second;

    // // for SNT record, pad, net, pin
    // m_net_pin_dict[aPad->GetNetCode()].emplace_back(
    //         genString( fp->GetReference(), "CMP" ), name );

    m_pinsList.push_back( &pin );

    VECTOR2D relpos = aPad->GetFPRelativePosition();

    // TODO: is odb pkg pin center means center of pad hole or center of pad shape?
    if( aPad->GetOffset().x != 0 || aPad->GetOffset().y != 0 )
        relpos += aPad->GetOffset();

    pin.m_center = ODB::AddXY( relpos );

    if( aPad->HasHole() )
    {
        pin.type = Pin::Type::THROUGH_HOLE;
    }
    else
    {
        pin.type = Pin::Type::SURFACE;
    }

    if( aPad->GetAttribute() == PAD_ATTRIB::NPTH )
        pin.etype = Pin::ElectricalType::MECHANICAL;
    else if( aPad->IsOnCopperLayer() )
        pin.etype = Pin::ElectricalType::ELECTRICAL;
    else
        pin.etype = Pin::ElectricalType::UNDEFINED;


    if( ( aPad->HasHole() && aPad->IsOnCopperLayer() ) || aPad->GetAttribute() == PAD_ATTRIB::PTH )
    {
        pin.mtype = Pin::MountType::THROUGH_HOLE;
    }
    else if( aPad->HasHole() && aPad->GetAttribute() == PAD_ATTRIB::NPTH )
    {
        pin.mtype = Pin::MountType::HOLE;
    }
    else if( aPad->GetAttribute() == PAD_ATTRIB::SMD )
    {
        pin.mtype = Pin::MountType::SMT;
    }
    else
    {
        pin.mtype = Pin::MountType::UNDEFINED;
    }

    // int maxError = m_board->GetDesignSettings().m_MaxError;

    // VECTOR2I expansion{ 0, 0 };

    // if( LSET( 2, F_Mask, B_Mask ).Contains( aPad->GetLayer() ) )
    //     expansion.x = expansion.y = 2 * aPad->GetSolderMaskExpansion();

    // if( LSET( 2, F_Paste, B_Paste ).Contains( aPad->GetLayer() ) )
    //     expansion = 2 * aPad->GetSolderPasteMargin();


    SHAPE_POLY_SET polygons;
    aPad->MergePrimitivesAsPolygon( &polygons );

    // if( expansion != VECTOR2I( 0, 0 ) )
    // {
    //     shape.InflateWithLinkedHoles( std::max( expansion.x, expansion.y ),
    //                                     CORNER_STRATEGY::ROUND_ALL_CORNERS, maxError,
    //                                     SHAPE_POLY_SET::PM_FAST );
    // }

    // TODO: Here we put all pad shapes as polygonl, we should switch by pad shape
    // Note:pad only use polygons->Polygon(0),
    if( polygons.OutlineCount() > 0 )
    {
        pin.m_pinOutlines.push_back( 
            std::make_unique<OUTLINE_CONTOUR>( polygons.Polygon( 0 ) ) );
    }

    return pin;
}

void EDAData::Pin::Write( std::ostream &ost ) const
{
    static const std::map<Type, std::string> type_map =
    {
            {Type::SURFACE, "S"},
            {Type::THROUGH_HOLE, "T"},
            {Type::BLIND, "B"},
    };

    static const std::map<ElectricalType, std::string> etype_map =
    {
            {ElectricalType::ELECTRICAL, "E"},
            {ElectricalType::MECHANICAL, "M"},
            {ElectricalType::UNDEFINED, "U"},
    };
    static const std::map<MountType, std::string> mtype_map =
    {
            {MountType::THROUGH_HOLE, "T"},
            {MountType::HOLE, "H"},
            {MountType::SMT, "S"},
            {MountType::UNDEFINED, "U"},
    };

    ost << "PIN " << m_name << " " << type_map.at( type ) << " "
        << m_center.first << " " << m_center.second
        << " 0 " << etype_map.at( etype ) << " "
        << mtype_map.at( mtype ) << std::endl;

    for( const auto& outline : m_pinOutlines )
    {
        outline->Write(ost);
    }
}


void EDAData::Package::Write( std::ostream &ost ) const
{
    ost << "PKG " << m_name << " "
        << ODB::Float2StrVal( m_ODBScale * m_pitch ) << " "
        << ODB::Float2StrVal( m_ODBScale * m_xmin ) << " "
        << ODB::Float2StrVal( m_ODBScale * m_ymin ) << " "
        << ODB::Float2StrVal( m_ODBScale * m_xmax ) << " "
        << ODB::Float2StrVal( m_ODBScale * m_ymax ) << std::endl;

    for ( const auto& outline : m_pkgOutlines )
    {
        outline->Write(ost);
    }

    for ( const auto &pin : m_pinsList )
    {
        pin->Write(ost);
    }
}

void EDAData::Write( std::ostream &ost ) const
{
    ost << "# " << wxDateTime::Now().FormatISOCombined() << std::endl;
    ost << "UNITS=MM" << std::endl;
    ost << "LYR";

    for ( const auto &layer : layers ) {

        ost << " " << layer;
    }

    ost << std::endl;

    write_attributes(ost, "#");

    for (const auto &net : nets)
    {
        ost << "#NET " << net->index << std::endl;
        net->Write(ost);
    }
    
    size_t i = 0;
    for( const auto* pkg : packages )
    {
        ost << "# PKG " << i << std::endl;
        i++;
        pkg->Write(ost);
        ost << "#" << std::endl;
    }
}

