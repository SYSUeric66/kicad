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
    static const std::map<FillType, std::string> fill_type_map = {
            {FillType::SOLID, "S"},
    };
    static const std::map<CutoutType, std::string> cutout_type_map = {
            {CutoutType::CIRCLE, "C"},
    };
    ost << "PLN " << fill_type_map.at( fill_type ) << " " << cutout_type_map.at(cutout_type) << " " << fill_size;
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
                         const wxString &layer, unsigned int feature_id )
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
    // r->m_surfaces->append_polygon_auto_orientation( aPolygon);
    return r;
}

static std::unique_ptr<PKG_OUTLINE> InitOutlineByPolygon( const SHAPE_POLY_SET::POLYGON& aPolygon )
{
    return InitOutlineContourByPolygon( aPolygon );
}

// void OutlineSquare::Write(std::ostream &ost) const
// {
//     ost << "SQ " << center << " " << Dim{half_side} << endl;
// }

// void OutlineCircle::Write(std::ostream &ost) const
// {
//     ost << "CR " << center << " " << Dim{radius} << endl;
// }

// void OutlineRectangle::Write( std::ostream &ost ) const
// {
//     ost << "RC " << lower << " " << Dim{width} << " " << Dim{height} << endl;
// }

void OUTLINE_CONTOUR::Write( std::ostream &ost ) const
{
    ost << "CT" << std::endl;
    m_surfaces->Write(ost);
    ost << "CE" << std::endl;
}


EDAData::Package &EDAData::add_package( FOOTPRINT* aFp )
{
    wxString fp_name = aFp->GetFPID().GetLibItemName().wx_str();
    auto &x = packages_map.emplace(
        std::piecewise_construct,
        std::forward_as_tuple( hash_fp_item( aFp, HASH_POS | REL_COORD ) ),
        std::forward_as_tuple( packages.size(), fp_name ) )
            .first->second;

    packages.push_back( &x );

    // SHAPE_POLY_SET& zone_shape = *zone->GetFilledPolysList( aLayer );

    // for( int ii = 0; ii < zone_shape.OutlineCount(); ++ii )
    // {
    //     AddContour( zone_shape, ii );
    // }

    // std::map<PCB_LAYER_ID, std::vector<BOARD_ITEM*>> elements;

    // for( BOARD_ITEM* item : aFp->GraphicalItems() )
    // {
    //     PCB_LAYER_ID layer = item->GetLayer();

    //     /// only supports the documentation layers for production and post-production
    //     /// All other layers are ignored
    //     if( layer != F_Fab && layer != B_Fab )
    //         continue;

    //     // if( item->Type() == PCB_SHAPE_T )
    //     // {
    //     //     PCB_SHAPE* shape = static_cast<PCB_SHAPE*>( item );

    //     //     // Circles and Rectanges only have size information so we need to place them in
    //     //     // a separate node that has a location
    //     //     if( shape->GetShape() == SHAPE_T::CIRCLE || shape->GetShape() == SHAPE_T::RECTANGLE )
    //     //         is_abs = false;
    //     // }

    //     elements[item->GetLayer()].push_back( item );
    // }

    // auto add_base_node = [&]( PCB_LAYER_ID aLayer ) -> wxXmlNode*
    // {
    //     wxXmlNode* parent = packageNode;
    //     bool is_back = aLayer == B_SilkS || aLayer == B_Fab;

    //     if( is_back )
    //     {
    //         if( !otherSideViewNode )
    //             otherSideViewNode = new wxXmlNode( wxXML_ELEMENT_NODE, "OtherSideView" );

    //         parent = otherSideViewNode;
    //     }

    //     wxString name;

    //     if( aLayer == F_SilkS || aLayer == B_SilkS )
    //         name = "SilkScreen";
    //     else if( aLayer == F_Fab || aLayer == B_Fab )
    //         name = "AssemblyDrawing";
    //     else
    //         wxASSERT( false );

    //     wxXmlNode* new_node = appendNode( parent, name );
    //     return new_node;
    // };


    // std::map<PCB_LAYER_ID, wxXmlNode*> layer_nodes;
    // std::map<PCB_LAYER_ID, BOX2I> layer_bbox;

    // for( auto layer : { F_Fab, B_Fab } )
    // {
    //     if( elements.find( layer ) != elements.end() )
    //     {
    //         if( elements[layer].size() > 0 )
    //             layer_bbox[layer] = elements[layer][0]->GetBoundingBox();

    //     }
    // }

    // for( auto& [layer, vec] : elements )
    // {
    //     for( BOARD_ITEM* item : vec )
    //     {
    //         layer_bbox[layer].Merge( item->GetBoundingBox() );

    //         switch( item->Type() )
    //         {
    //         case PCB_TEXT_T:
    //         {
    //             PCB_TEXT* text = static_cast<PCB_TEXT*>( item );
    //             addText( output_node, text, text->GetFontMetrics() );
    //             break;
    //         }

    //         case PCB_TEXTBOX_T:
    //         {
    //             PCB_TEXTBOX* text = static_cast<PCB_TEXTBOX*>( item );
    //             addText( output_node, text, text->GetFontMetrics() );

    //             // We want to force this to be a polygon to get absolute coordinates
    //             if( text->IsBorderEnabled() )
    //             {
    //                 SHAPE_POLY_SET poly_set;
    //                 text->GetEffectiveShape()->TransformToPolygon( poly_set, 0, ERROR_INSIDE );
    //                 addContourNode( output_node, poly_set, 0, FILL_T::NO_FILL,
    //                                 text->GetBorderWidth() );
    //             }

    //             break;
    //         }

    //         case PCB_SHAPE_T:
    //         {
    //             if( !is_abs )
    //                 addLocationNode( output_node, *static_cast<PCB_SHAPE*>( item ) );

    //             addShape( output_node, *static_cast<PCB_SHAPE*>( item ) );

    //             break;
    //         }

    //         default: break;
    //         }
    //     }
    

    //     if( group_node->GetChildren() == nullptr )
    //     {
    //         marking_node->RemoveChild( group_node );
    //         layer_node->RemoveChild( marking_node );
    //         delete group_node;
    //         delete marking_node;
    //     }
    // }

    // for( auto&[layer, bbox] : layer_bbox)
    // {
    //     if( bbox.GetWidth() > 0 )
    //     {
    //         wxXmlNode* outlineNode = insertNode( layer_nodes[layer], "PKG_OUTLINE" );

    //         SHAPE_POLY_SET::POLYGON outline( 1 );
    //         std::vector<VECTOR2I> points( 4 );
    //         points[0] = bbox.GetPosition();
    //         points[2] = bbox.GetEnd();
    //         points[1].x = points[0].x;
    //         points[1].y = points[2].y;
    //         points[3].x = points[2].x;
    //         points[3].y = points[0].y;

    //         outline[0].Append( points );
    //         addPolygonNode( outlineNode, outline, FILL_T::NO_FILL, 0 );
    //         addLineDesc( outlineNode, 0, LINE_STYLE::SOLID );
    //     }
    // }

    for( size_t ii = 0; ii < fp->Pads().size(); ++ii )
    {
        PAD* pad = fp->Pads()[ii];
        wxXmlNode* pinNode = appendNode( packageNode, "Pin" );
        wxString name = pinName( pad );

        addAttribute( pinNode,  "number", name );

        m_net_pin_dict[pad->GetNetCode()].emplace_back(
                genString( fp->GetReference(), "CMP" ), name );

        if( pad->GetAttribute() == PAD_ATTRIB::NPTH )
            addAttribute( pinNode,  "electricalType", "MECHANICAL" );
        else if( pad->IsOnCopperLayer() )
            addAttribute( pinNode,  "electricalType", "ELECTRICAL" );
        else
            addAttribute( pinNode,  "electricalType", "UNDEFINED" );

        if( pad->HasHole() )
            addAttribute( pinNode,  "type", "THRU" );
        else
            addAttribute( pinNode,  "type", "SURFACE" );

        if( pad->GetFPRelativeOrientation() != ANGLE_0 )
        {
            wxXmlNode* xformNode = appendNode( pinNode, "Xform" );
            xformNode->AddAttribute(
                    "rotation",
                    floatVal( pad->GetFPRelativeOrientation().Normalize().AsDegrees() ) );
        }

        addLocationNode( pinNode, *pad, true );
        addShape( pinNode, *pad, pad->GetLayer() );

        // We just need the padstack, we don't need the reference here.  The reference will be created
        // in the LayerFeature set
        wxXmlNode dummy;
        addPadStack( &dummy, pad );
    }


    BOX2I bbox = aFp->GetBoundingBox();
    x.xmin = bbox.GetPosition().x;
    x.ymin = bbox.GetPosition().y;
    x.xmax = bbox.GetEnd().x;
    x.ymax = bbox.GetEnd().y;
    if( aFp->Pads().size() < 2 )
        x.pitch = pcbIUScale.mmToIU( 1.0 ); // placeholder value

    for( auto it = aFp->Pads().begin(); it != aFp->Pads().end(); it++ )
    {
        auto it2 = it;
        it2++;
        for(; it2 != aFp.pads.end(); it2++)
        {
            const uint64_t pin_dist = Coordd(it->second.placement.shift - it2->second.placement.shift).mag();
            x.pitch = std::min(x.pitch, pin_dist);
        }
    }

    SHAPE_POLY_SET& pkg_polygons = aFp->GetBoundingHull();

    // TODO: Here we put rect, square, and circle all as polygon,
    // we need to add them when OutlineCount() equals to 1
    if( pkg_polygons.OutlineCount() >= 1 )
    {
        for( int ii = 0; ii < pkg_polygons.OutlineCount(); ++ii )
        {
            x.m_pkgOutlines.push_back( InitOutlineByPolygon( pkg_polygons.Polygon(ii) ) );
        }
    }
    else
    {
        x.outline.push_back(std::make_unique<OutlineRectangle>( bbox ));
    }
    

    for( size_t ii = 0; ii < aFp->Pads().size(); ++ii )
    {
        PAD* pad = aFp->Pads()[ii];
        x.add_pin( pad, ii );
    }

    return x;
}

EDAData::Pin::Pin(unsigned int i, const wxString &n) : name(n), index(i)
{
}

EDAData::Pin &EDAData::Package::add_pin( PAD* pad, size_t ii )
{
    wxString name = pad->GetNumber();

    // Pins are required to have names, so if our pad doesn't have a name, we need to
    // generate one that is unique
    if( pad->GetAttribute() == PAD_ATTRIB::NPTH )
        name = wxString::Format( "NPTH%zu", ii );
    else if( name.empty() )
        name = wxString::Format( "PAD%zu", ii );

    size_t hash = hash_fp_item( pad, 0 );

    auto &pin = pins_map.emplace(
        std::piecewise_construct, std::forward_as_tuple( hash ),
        std::forward_as_tuple( pins.size(), name ))
            .first->second;

    pins.push_back(&pin);
    pin.center = ODB::AddXY( pad->GetCenter() );

    if( pad->HasHole() )
    {
        pin.type = Pin::Type::THROUGH_HOLE;
        pin.mtype = Pin::MountType::THROUGH_HOLE;
    }
    else
    {
        pin.type = Pin::Type::SURFACE;
        pin.mtype = Pin::MountType::SMT;
    }

    if( pad->GetAttribute() == PAD_ATTRIB::NPTH )
        pin.etype = Pin::ElectricalType::MECHANICAL;
    else if( pad->IsOnCopperLayer() )
        pin.etype = Pin::ElectricalType::ELECTRICAL;
    else
        pin.etype = Pin::ElectricalType::UNDEFINED;



    // const auto &ps = pad.padstack;
    // std::set<const Shape *> shapes_top;
    // for (const auto &[uu, sh] : ps.shapes) {
    //     if (sh.layer == BoardLayers::TOP_COPPER)
    //         shapes_top.insert(&sh);
    // }
    // const auto n_polys_top = std::count_if(ps.polygons.begin(), ps.polygons.end(),
    //                                        [](auto &it) { return it.second.layer == BoardLayers::TOP_COPPER; });
    // if (shapes_top.size() == 1 && n_polys_top == 0 && (*shapes_top.begin())->form == Shape::Form::CIRCLE) {
    //     const auto &sh = **shapes_top.begin();
    //     pin.outline.push_back(
    //             std::make_unique<OutlineCircle>(pad.placement.transform(sh.placement.shift), sh.params.at(0) / 2));
    // }
    // else {
    //     const auto bb = ps.get_bbox(true);
    //     pin.outline.push_back(std::make_unique<OutlineRectangle>(pad.placement.transform_bb(bb)));
    // }
    return pin;
}

void EDAData::Pin::Write( std::ostream &ost ) const
{
    static const std::map<Type, std::string> type_map = {
            {Type::SURFACE, "S"},
            {Type::THROUGH_HOLE, "T"},
            {Type::BLIND, "B"},
    };

    static const std::map<ElectricalType, std::string> etype_map = {
            {ElectricalType::ELECTRICAL, "E"},
            {ElectricalType::MECHANICAL, "M"},
            {ElectricalType::UNDEFINED, "U"},
    };
    static const std::map<MountType, std::string> mtype_map = {
            {MountType::THROUGH_HOLE, "T"},
            {MountType::SMT, "S"},
            {MountType::UNDEFINED, "U"},
    };

    ost << "PIN " << name << " " << type_map.at( type ) << " "
        << center.first << " " << center.second
        << " 0 " << etype_map.at( etype ) << " "
        << mtype_map.at( mtype ) << std::endl;

    // for (const auto &ol : outline) {
    //     ol->Write(ost);
    // }
}


void EDAData::Package::Write( std::ostream &ost ) const
{
    ost << "PKG " << name << " "
        << ODB::Float2StrVal( m_ODBScale * pitch ) << " "
        << ODB::Float2StrVal( m_ODBScale * xmin ) << " "
        << ODB::Float2StrVal( m_ODBScale * ymin ) << " "
        << ODB::Float2StrVal( m_ODBScale * xmax ) << " "
        << ODB::Float2StrVal( m_ODBScale * ymax ) << std::endl;

    // for (const auto &ol : outline)
    // {
    //     ol->Write(ost);
    // }

    for (const auto &pin : pins)
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
    for (const auto pkg : packages)
    {
        ost << "# PKG " << i << std::endl;
        i++;
        pkg->Write(ost);
        ost << "#" << std::endl;
    }
}

