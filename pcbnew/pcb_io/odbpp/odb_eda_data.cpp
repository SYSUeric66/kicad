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

void EDAData::Net::write(std::ostream &ost) const
{
    ost << "NET " << name;
    write_attributes(ost);
    ost << std::endl;

    for (const auto &subnet : subnets) {
        subnet->write(ost);
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


void EDAData::Subnet::write(std::ostream &ost) const
{
    ost << "SNT ";
    write_subnet(ost);
    ost << std::endl;
    for (const auto &fid : feature_ids)
    {
        fid.write(ost);
    }
}

void EDAData::FeatureID::write( std::ostream &ost ) const
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

// static std::unique_ptr<EDAData::Outline> poly_as_rectangle_or_square(const Polygon &poly)
// {
//     if (!poly.is_rect())
//         return nullptr;
//     const auto &p0 = poly.vertices.at(0).position;
//     const auto &p1 = poly.vertices.at(1).position;
//     const auto &p2 = poly.vertices.at(2).position;
//     const auto &p3 = poly.vertices.at(3).position;
//     const auto v0 = p1 - p0;
//     const auto v1 = p2 - p1;
//     const Coordi bottom_left = Coordi::min(p0, Coordi::min(p1, Coordi::min(p2, p3)));
//     uint64_t w;
//     uint64_t h;
//     if (v0.y == 0) {
//         assert(v1.x == 0);
//         w = std::abs(v0.x);
//         h = std::abs(v1.y);
//     }
//     else if (v0.x == 0) {
//         assert(v1.y == 0);
//         w = std::abs(v1.x);
//         h = std::abs(v0.y);
//     }
//     else {
//         assert(false);
//     }
//     if (w == h) {
//         const auto hs = w / 2;
//         return std::make_unique<EDAData::OutlineSquare>(bottom_left + Coordi(hs, hs), hs);
//     }
//     else {
//         return std::make_unique<EDAData::OutlineRectangle>(bottom_left, w, h);
//     }
// }

// static std::unique_ptr<EDAData::Outline> outline_contour_from_polygon(const Polygon &poly)
// {
//     auto r = std::make_unique<EDAData::OutlineContour>();
//     r->data.append_polygon_auto_orientation(poly);
//     return r;
// }

// static std::unique_ptr<EDAData::Outline> outline_from_polygon(const Polygon &poly)
// {
//     if (auto x = poly_as_rectangle_or_square(poly))
//         return x;
//     return outline_contour_from_polygon(poly);
// }

// void EDAData::OutlineSquare::write(std::ostream &ost) const
// {
//     ost << "SQ " << center << " " << Dim{half_side} << endl;
// }

// void EDAData::OutlineCircle::write(std::ostream &ost) const
// {
//     ost << "CR " << center << " " << Dim{radius} << endl;
// }

// void EDAData::OutlineRectangle::write(std::ostream &ost) const
// {
//     ost << "RC " << lower << " " << Dim{width} << " " << Dim{height} << endl;
// }

// void EDAData::OutlineContour::write(std::ostream &ost) const
// {
//     ost << "CT" << endl;
//     data.write(ost);
//     ost << "CE" << endl;
// }


EDAData::Package &EDAData::add_package( FOOTPRINT* aFp )
{
    wxString fp_name = aFp->GetFPID().GetLibItemName().wx_str();
    auto &x = packages_map.emplace(
        std::piecewise_construct,
        std::forward_as_tuple( hash_fp_item( aFp, HASH_POS | REL_COORD ) ),
        std::forward_as_tuple( packages.size(), fp_name ) )
            .first->second;

    packages.push_back( &x );
    // SHAPE_POLY_SET bb = aFp->GetBoundingHull();
    

    // const SHAPE_POLY_SET& courtyard = aFp->GetCourtyard( F_CrtYd );
    // const SHAPE_POLY_SET& courtyard_back = aFp->GetCourtyard( B_CrtYd );

    // if( courtyard.OutlineCount() > 0 )
    //     addOutlineNode( packageNode, courtyard, courtyard.Outline( 0 ).Width(), LINE_STYLE::SOLID );

    // if( courtyard_back.OutlineCount() > 0 )
    // {
    //     otherSideViewNode = appendNode( packageNode, "OtherSideView" );
    //     addOutlineNode( otherSideViewNode, courtyard_back, courtyard_back.Outline( 0 ).Width(), LINE_STYLE::SOLID );
    // }

    // if( !courtyard.OutlineCount() && !courtyard_back.OutlineCount() )
    // {
    //     SHAPE_POLY_SET bbox = fp->GetBoundingHull();
    //     addOutlineNode( packageNode, bbox );
    // }


    // std::map<PCB_LAYER_ID, std::map<bool, std::vector<BOARD_ITEM*>>> elements;

    // for( BOARD_ITEM* item : fp->GraphicalItems() )
    // {
    //     PCB_LAYER_ID layer = item->GetLayer();

    //     /// IPC2581 only supports the documentation layers for production and post-production
    //     /// All other layers are ignored
    //     /// TODO: Decide if we should place the other layers from footprints on the board
    //     if( layer != F_SilkS && layer != B_SilkS && layer != F_Fab && layer != B_Fab )
    //         continue;

    //     bool is_abs = true;

    //     if( item->Type() == PCB_SHAPE_T )
    //     {
    //         PCB_SHAPE* shape = static_cast<PCB_SHAPE*>( item );

    //         // Circles and Rectanges only have size information so we need to place them in
    //         // a separate node that has a location
    //         if( shape->GetShape() == SHAPE_T::CIRCLE || shape->GetShape() == SHAPE_T::RECTANGLE )
    //             is_abs = false;
    //     }

    //     elements[item->GetLayer()][is_abs].push_back( item );
    // }

    // auto add_base_node = [&]( PCB_LAYER_ID aLayer ) -> wxXmlNode*
    // {
    //     wxXmlNode* parent = packageNode;
    //     bool is_back = aLayer == B_SilkS || aLayer == B_Fab;

    //     if( is_back )
    //     {
    //         if( !otherSideViewNode )
    //             otherSideViewNode = appendNode( packageNode, "OtherSideView" );

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
    //         if( elements[layer][true].size() > 0 )
    //             layer_bbox[layer] = elements[layer][true][0]->GetBoundingBox();
    //         else if( elements[layer][false].size() > 0 )
    //             layer_bbox[layer] = elements[layer][false][0]->GetBoundingBox();
    //     }
    // }

    // for( auto& [layer, map] : elements )
    // {
    //     wxXmlNode* layer_node = add_base_node( layer );
    //     wxXmlNode* marking_node = add_marking_node( layer_node );
    //     wxXmlNode* group_node = appendNode( marking_node, "UserSpecial" );
    //     bool update_bbox = false;

    //     if( layer == F_Fab || layer == B_Fab )
    //     {
    //         layer_nodes[layer] = layer_node;
    //         update_bbox = true;
    //     }

    //     for( auto& [is_abs, vec] : map )
    //     {
    //         for( BOARD_ITEM* item : vec )
    //         {
    //             wxXmlNode* output_node = nullptr;

    //             if( update_bbox )
    //                 layer_bbox[layer].Merge( item->GetBoundingBox() );

    //             if( !is_abs )
    //                 output_node = add_marking_node( layer_node );
    //             else
    //                 output_node = group_node;

    //             switch( item->Type() )
    //             {
    //             case PCB_TEXT_T:
    //             {
    //                 PCB_TEXT* text = static_cast<PCB_TEXT*>( item );
    //                 addText( output_node, text, text->GetFontMetrics() );
    //                 break;
    //             }

    //             case PCB_TEXTBOX_T:
    //             {
    //                 PCB_TEXTBOX* text = static_cast<PCB_TEXTBOX*>( item );
    //                 addText( output_node, text, text->GetFontMetrics() );

    //                 // We want to force this to be a polygon to get absolute coordinates
    //                 if( text->IsBorderEnabled() )
    //                 {
    //                     SHAPE_POLY_SET poly_set;
    //                     text->GetEffectiveShape()->TransformToPolygon( poly_set, 0, ERROR_INSIDE );
    //                     addContourNode( output_node, poly_set, 0, FILL_T::NO_FILL,
    //                                     text->GetBorderWidth() );
    //                 }

    //                 break;
    //             }

    //             case PCB_SHAPE_T:
    //             {
    //                 if( !is_abs )
    //                     addLocationNode( output_node, static_cast<PCB_SHAPE*>( item ) );

    //                 addShape( output_node, *static_cast<PCB_SHAPE*>( item ) );

    //                 break;
    //             }

    //             default: break;
    //             }
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
    //         wxXmlNode* outlineNode = insertNode( layer_nodes[layer], "Outline" );

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

    BOX2I bbox = aFp->GetBoundingBox();
    x.xmin = bbox.GetPosition().x;
    x.ymin = bbox.GetPosition().y;
    x.xmax = bbox.GetEnd().x;
    x.ymax = bbox.GetEnd().y;
    x.pitch = 1000000;
    // if (aFp.pads.size() < 2)
    //     x.pitch = 1_mm; // placeholder value to not break anything

    // for (auto it = aFp.pads.begin(); it != aFp.pads.end(); it++) {
    //     auto it2 = it;
    //     it2++;
    //     for (; it2 != aFp.pads.end(); it2++) {
    //         const uint64_t pin_dist = Coordd(it->second.placement.shift - it2->second.placement.shift).mag();
    //         x.pitch = std::min(x.pitch, pin_dist);
    //     }
    // }

    // std::set<const Polygon *> outline_polys;
    // for (const auto &[uu, poly] : aFp.polygons) {
    //     // check both layers since we might be looking at a flipped package
    //     if ((poly.layer == BoardLayers::TOP_PACKAGE) || (poly.layer == BoardLayers::BOTTOM_PACKAGE)) {
    //         outline_polys.insert(&poly);
    //     }
    // }

    // if (outline_polys.size() == 1) {
    //     x.outline.push_back(outline_from_polygon(**outline_polys.begin()));
    // }
    // else if (outline_polys.size() > 1) {
    //     for (auto poly : outline_polys)
    //         x.outline.push_back(outline_contour_from_polygon(*poly));
    // }
    // else {
    //     x.outline.push_back(std::make_unique<OutlineRectangle>(bb));
    // }


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

void EDAData::Pin::write( std::ostream &ost ) const
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
    //     ol->write(ost);
    // }
}


void EDAData::Package::write( std::ostream &ost ) const
{
    ost << "PKG " << name << " "
        << ODB::Float2StrVal( m_ODBScale * pitch ) << " "
        << ODB::Float2StrVal( m_ODBScale * xmin ) << " "
        << ODB::Float2StrVal( m_ODBScale * ymin ) << " "
        << ODB::Float2StrVal( m_ODBScale * xmax ) << " "
        << ODB::Float2StrVal( m_ODBScale * ymax ) << std::endl;

    // for (const auto &ol : outline)
    // {
    //     ol->write(ost);
    // }

    for (const auto &pin : pins)
    {
        pin->write(ost);
    }
}

void EDAData::write( std::ostream &ost ) const
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
        net->write(ost);
    }
    
    size_t i = 0;
    for (const auto pkg : packages)
    {
        ost << "# PKG " << i << std::endl;
        i++;
        pkg->write(ost);
        ost << "#" << std::endl;
    }
}

