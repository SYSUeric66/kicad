#include "odb_feature.h"
#include "odb_util.h"
#include <sstream>
#include <pcb_shape.h>
#include "odb_defines.h"
#include "convert_basic_shapes_to_polygon.h"
#include <math/util.h>    // for KiROUND
#include "pcb_track.h"
#include "pcb_textbox.h"
#include "zone.h"
#include "board.h"
#include "board_design_settings.h"
#include "geometry/eda_angle.h"

ODB_LINE& FEATURES_MANAGER::AddFeatureLine( const VECTOR2I& aStart,
                                            const VECTOR2I& aEnd,
                                            uint64_t aWidth )
{
    return AddFeature<ODB_LINE>( ODB::AddXY( aStart ),
                                 ODB::AddXY( aEnd ),
                                 AddCircleSymbol( std::to_string( aWidth ) ) );
}

ODB_ARC& FEATURES_MANAGER::AddFeatureArc( const VECTOR2I& aStart,
                                          const VECTOR2I& aEnd,
                                          const VECTOR2I& aCenter,
                                          uint64_t aWidth,
                                          ODB_DIRECTION aDirection )
{
    return AddFeature<ODB_ARC>( ODB::AddXY( aStart ),
                                ODB::AddXY( aEnd ),
                                ODB::AddXY( aCenter ),
                                AddCircleSymbol( std::to_string( aWidth ) ),
                                aDirection );
}

ODB_PAD& FEATURES_MANAGER::AddFeaturePad( const VECTOR2I& aCenter,
                                          const wxString& aSym,
                                          const EDA_ANGLE& aAngle,
                                          bool aMirror )
{
    return AddFeature<ODB_PAD>( ODB::AddXY( aCenter ),
                                AddPadSymbol( aSym ),
                                aAngle, aMirror );
}

ODB_PAD& FEATURES_MANAGER::AddPadCircle( const VECTOR2I& aCenter,
                                         uint64_t aDiameter,
                                         const EDA_ANGLE& aAngle,
                                         bool aMirror, double aResize = 1.0 )
{
    return AddFeature<ODB_PAD>( ODB::AddXY( aCenter ),
                                AddCircleSymbol( std::to_string( aDiameter ) ),
                                aAngle, aMirror, aResize );
}


// bool FEATURES_MANAGER::AddPolygon( const SHAPE_POLY_SET::POLYGON& aPolygon,
//                                    FILL_T aFillType, int aWidth, LINE_STYLE aDashType )
// {
//     if( aPolygon.empty() || aPolygon[0].PointCount() < 3 )
//         return false;

//     // auto make_node =
//     //         [&]()
//     // {

//     //     const std::vector<VECTOR2I>& pts = aPolygon[0].CPoints();
//     //     addXY( polybeginNode, pts[0] );

//     //     for( size_t ii = 1; ii < pts.size(); ++ii )
//     //     {
//     //         wxXmlNode* polyNode = appendNode( polygonNode, "PolyStepSegment" );
//     //         addXY( polyNode, pts[ii] );
//     //     }

//     //     wxXmlNode* polyendNode = appendNode( polygonNode, "PolyStepSegment" );
//     //     addXY( polyendNode, pts[0] );
//     // };

//     // Allow the case where we don't want line/fill information in the polygon
//     if( aFillType == FILL_T::NO_FILL )
//     {
//         AddFeatureSurface( aPolygon[0] );
//         // If we specify a line width, we need to add a LineDescRef node and
//         // since this is only valid for a non-filled polygon, we need to create
//         // the fillNode as well
//         if( aWidth > 0 )
//             // addLineDesc( polygonNode, aWidth, aDashType, true ); TODO
//     }
//     else
//     {
//         wxCHECK( aWidth == 0, false );
//         AddFeatureSurface( aPolygon[0] );
//     }

//     // addFillDesc( polygonNode, aFillType );

//     return true;
// }

// bool FEATURES_MANAGER::AddPolygonCutouts( const SHAPE_POLY_SET::POLYGON& aPolygon )
// {
//     for( size_t ii = 1; ii < aPolygon.size(); ++ii )
//     {
//         wxCHECK2( aPolygon[ii].PointCount() >= 3, continue );

//         const std::vector<VECTOR2I>& hole = aPolygon[ii].CPoints();
//         addXY( polybeginNode, hole.back() );

//         for( size_t jj = 0; jj < hole.size(); ++jj )
//         {
//             addXY( polyNode, hole[jj] );
//         }

//     }

//     return true;
// }


bool FEATURES_MANAGER::AddContour( const SHAPE_POLY_SET& aPolySet, int aOutline = 0,
                            FILL_T aFillType = FILL_T::FILLED_SHAPE )
{
    // todo: args modify aPolySet.Polygon( aOutline ) instead of aPolySet
    if( aPolySet.OutlineCount() < ( aOutline + 1 ) )
        return false;
    
    AddFeatureSurface( aPolySet.Polygon( aOutline ), aFillType );

    return true;
}


// bool FEATURES_MANAGER::AddPolygon( const SHAPE_POLY_SET::POLYGON& aPolygon,
//                                    FILL_T aFillType, int aWidth, LINE_STYLE aDashType )
// {
//     wxXmlNode* polygonNode = nullptr;

//     if( aPolygon.empty() || aPolygon[0].PointCount() < 3 )
//         return false;

//     auto make_node =
//             [&]()
//     {
//         polygonNode = appendNode( aParentNode, "Polygon" );
//         wxXmlNode* polybeginNode = appendNode( polygonNode, "PolyBegin" );

//         const std::vector<VECTOR2I>& pts = aPolygon[0].CPoints();
//         addXY( polybeginNode, pts[0] );

//         for( size_t ii = 1; ii < pts.size(); ++ii )
//         {
//             wxXmlNode* polyNode = appendNode( polygonNode, "PolyStepSegment" );
//             addXY( polyNode, pts[ii] );
//         }

//         wxXmlNode* polyendNode = appendNode( polygonNode, "PolyStepSegment" );
//         addXY( polyendNode, pts[0] );
//     };

//     // Allow the case where we don't want line/fill information in the polygon
//     if( aFillType == FILL_T::NO_FILL )
//     {
//         make_node();
//         // If we specify a line width, we need to add a LineDescRef node and
//         // since this is only valid for a non-filled polygon, we need to create
//         // the fillNode as well
//         if( aWidth > 0 )
//             addLineDesc( polygonNode, aWidth, aDashType, true );
//     }
//     else
//     {
//         wxCHECK( aWidth == 0, false );
//         make_node();
//     }

//     addFillDesc( polygonNode, aFillType );

//     return true;
// }


void FEATURES_MANAGER::AddShape( const PCB_SHAPE& aShape )
{
    wxString name;

    switch( aShape.GetShape() )
    {
    case SHAPE_T::CIRCLE:
    {
        int diameter = aShape.GetRadius() * 2.0;
        int width = aShape.GetStroke().GetWidth();
        VECTOR2I center = ODB::GetShapePosition( aShape );

        if( aShape.GetFillMode() == FILL_T::NO_FILL )
        {
            AddFeature<ODB_PAD>( ODB::AddXY( center ),
                AddCircleSymbol( std::to_string( KiROUND( m_ODBScale * diameter ) ) ) );
        }
        else
        {
            AddFeature<ODB_PAD>( ODB::AddXY( center ),
                AddCircleSymbol( std::to_string( KiROUND( m_ODBScale * ( diameter + width ) ) ) ) );         
        }

        break;
    }

    case SHAPE_T::RECTANGLE:
    {
        int width = std::abs( aShape.GetRectangleWidth() );
        int height = std::abs( aShape.GetRectangleHeight() );
        int stroke_width = aShape.GetStroke().GetWidth();
        VECTOR2I center = ODB::GetShapePosition( aShape );
        
        if( aShape.GetFillMode() != FILL_T::NO_FILL )
        {
            width += stroke_width;
            height += stroke_width;
        }

        wxString rad = ODB::Float2StrVal( m_ODBScale * ( stroke_width / 2.0 ) );
        wxString dim = ODB::Float2StrVal( m_ODBScale * width ) + ODB_DIM_X
                     + ODB::Float2StrVal( m_ODBScale * height ) + ODB_DIM_X
                     + ODB_DIM_R + rad;
                       

        AddFeature<ODB_PAD>( ODB::AddXY( center ), AddRoundRectSymbol( dim ) );

        break;
    }

    case SHAPE_T::POLY:
    {
        const SHAPE_POLY_SET& poly_set = aShape.GetPolyShape();

        for( int ii = 0; ii < poly_set.OutlineCount(); ++ii )
        {
            if( aShape.GetFillMode() != FILL_T::NO_FILL )
            {
                AddContour( poly_set, ii, FILL_T::FILLED_SHAPE );
            }

            AddContour( poly_set, ii, FILL_T::NO_FILL );
        }

        break;
    }

    case SHAPE_T::ARC:
    {
        ODB_DIRECTION dir = !aShape.IsClockwiseArc() ? ODB_DIRECTION::CW : ODB_DIRECTION::CCW;

        AddFeatureArc( aShape.GetStart(), aShape.GetEnd(),
                       aShape.GetCenter(), aShape.GetStroke().GetWidth(), dir );

        break;
    }

    case SHAPE_T::BEZIER:
    {
        const std::vector<VECTOR2I>& points = aShape.GetBezierPoints();

        for( size_t i = 0; i < points.size(); i++ )
        {
            AddFeatureLine( points[i], points[i+1],
                        aShape.GetStroke().GetWidth() );
        }

        break;
    }

    case SHAPE_T::SEGMENT:
    {
        AddFeatureLine( aShape.GetStart(), aShape.GetEnd(),
                        aShape.GetStroke().GetWidth() );
        
        break;
    }
    }
}

ODB_SURFACE& FEATURES_MANAGER::AddFeatureSurface( const SHAPE_POLY_SET::POLYGON& aPolygon,
                                                  FILL_T aFillType = FILL_T::FILLED_SHAPE )
{
    return AddFeature<ODB_SURFACE>( aPolygon, aFillType );
}

void FEATURES_MANAGER::AddPadShape( const PAD& aPad, PCB_LAYER_ID aLayer )
{
    FOOTPRINT* fp = aPad.GetParentFootprint();
    bool mirror = false;
    if( aPad.GetOrientation() != EDA_ANGLE::m_Angle0 )
    {
        if( fp && fp->IsFlipped() )
            mirror = true;
    }

    int      maxError = m_board->GetDesignSettings().m_MaxError;
    wxString name;

    VECTOR2I expansion{ 0, 0 };

    if( LSET( 2, F_Mask, B_Mask ).Contains( aLayer ) )
        expansion.x = expansion.y = 2 * aPad.GetSolderMaskExpansion();

    if( LSET( 2, F_Paste, B_Paste ).Contains( aLayer ) )
        expansion = 2 * aPad.GetSolderPasteMargin();

    VECTOR2I center = aPad.GetPosition();

    switch( aPad.GetShape() )
    {
    case PAD_SHAPE::CIRCLE:
    {
        wxString diam = ODB::Float2StrVal( m_ODBScale * ( expansion.x + aPad.GetSizeX() ) );

        AddFeature<ODB_PAD>( ODB::AddXY( center ), AddCircleSymbol( diam ),
                             aPad.GetOrientation(), mirror ); 

        break;
    }

    case PAD_SHAPE::RECTANGLE:
    {
        VECTOR2D pad_size = aPad.GetSize() + expansion;
        wxString width = ODB::Float2StrVal( m_ODBScale * std::abs( pad_size.x ) );
        wxString height = ODB::Float2StrVal( m_ODBScale * std::abs( pad_size.y ) );

        AddFeature<ODB_PAD>( ODB::AddXY( center ), AddRectSymbol( width, height ),
                             aPad.GetOrientation(), mirror );

        break;
    }
    case PAD_SHAPE::OVAL:
    {
        VECTOR2D pad_size = aPad.GetSize() + expansion;
        wxString width = ODB::Float2StrVal( m_ODBScale * std::abs( pad_size.x ) );
        wxString height = ODB::Float2StrVal( m_ODBScale * std::abs( pad_size.y ) );

        AddFeature<ODB_PAD>( ODB::AddXY( center ), AddOvalSymbol( width, height ),
                             aPad.GetOrientation(), mirror ); 

        break;
    }

    case PAD_SHAPE::ROUNDRECT:
    {
        VECTOR2D pad_size = aPad.GetSize() + expansion;
        wxString width = ODB::Float2StrVal( m_ODBScale * std::abs( pad_size.x ) );
        wxString height = ODB::Float2StrVal( m_ODBScale * std::abs( pad_size.y ) );
        wxString rad = ODB::Float2StrVal( m_ODBScale * aPad.GetRoundRectCornerRadius() );
        wxString dim = width + ODB_DIM_X + height + ODB_DIM_X +
                       ODB_DIM_R + rad;

        AddFeature<ODB_PAD>( ODB::AddXY( center ), AddRoundRectSymbol( dim ),
                             aPad.GetOrientation(), mirror );

        break;
    }

    case PAD_SHAPE::CHAMFERED_RECT:
    {        
        VECTOR2D pad_size = aPad.GetSize() + expansion;
        wxString width = ODB::Float2StrVal( m_ODBScale * std::abs( pad_size.x ) );
        wxString height = ODB::Float2StrVal( m_ODBScale * std::abs( pad_size.y ) );
        int shorterSide = std::min( pad_size.x, pad_size.y );
        int chamfer = std::max( 0, KiROUND( aPad.GetChamferRectRatio() * shorterSide ) );
        wxString rad = ODB::Float2StrVal( m_ODBScale * chamfer );
        int positions = aPad.GetChamferPositions();
        wxString dim = width + ODB_DIM_X + height + ODB_DIM_X +
                       ODB_DIM_C + rad;
        
        if( positions != RECT_CHAMFER_ALL )
        {
            dim += ODB_DIM_X;
            if( positions & RECT_CHAMFER_TOP_RIGHT )
                dim += "1";
            if( positions & RECT_CHAMFER_TOP_LEFT )
                dim += "2";
            if( positions & RECT_CHAMFER_BOTTOM_LEFT )
                dim += "3";
            if( positions & RECT_CHAMFER_BOTTOM_RIGHT )
                dim += "4";
        }

        AddFeature<ODB_PAD>( ODB::AddXY( center ), AddChamferRectSymbol( dim ),
                             aPad.GetOrientation(), mirror );

        break;
    }

    case PAD_SHAPE::TRAPEZOID:
    {

        VECTOR2I       pad_size = aPad.GetSize();
        VECTOR2I       trap_delta = aPad.GetDelta();
        SHAPE_POLY_SET outline;
        outline.NewOutline();
        int dx = pad_size.x / 2;
        int dy = pad_size.y / 2;
        int ddx = trap_delta.x / 2;
        int ddy = trap_delta.y / 2;

        outline.Append( -dx - ddy,  dy + ddx );
        outline.Append(  dx + ddy,  dy - ddx );
        outline.Append(  dx - ddy, -dy + ddx );
        outline.Append( -dx + ddy, -dy - ddx );

        // Shape polygon can have holes so use InflateWithLinkedHoles(), not Inflate()
        // which can create bad shapes if margin.x is < 0
        if( expansion.x )
        {
            outline.InflateWithLinkedHoles( expansion.x, CORNER_STRATEGY::ROUND_ALL_CORNERS,
                                            maxError, SHAPE_POLY_SET::PM_FAST );
        }

        for( int ii = 0; ii < outline.OutlineCount(); ++ii )
            AddContour( outline, ii );

        break;
    }
    case PAD_SHAPE::CUSTOM:
    {

        SHAPE_POLY_SET shape;
        aPad.MergePrimitivesAsPolygon( &shape );

        if( expansion != VECTOR2I( 0, 0 ) )
        {
            shape.InflateWithLinkedHoles( std::max( expansion.x, expansion.y ),
                                            CORNER_STRATEGY::ROUND_ALL_CORNERS, maxError,
                                            SHAPE_POLY_SET::PM_FAST );
        }

        for( int ii = 0; ii < shape.OutlineCount(); ++ii )
            AddContour( shape, ii );

        break;
    }
    default:
        // wxLogError( "Unknown pad type" );
        break;
    }

}



void FEATURES_MANAGER::InitFeatureList( PCB_LAYER_ID aLayer,
                                        std::vector<BOARD_ITEM*>& aItems )
{
    // auto it = aItems.begin();
    // if( BOARD_CONNECTED_ITEM* item = dynamic_cast<BOARD_CONNECTED_ITEM*>( *it ); IsCopperLayer( aLayer ) && item )
    // {
    //     if( item->GetNetCode() > 0 )
    //         // addAttribute( layerSetNode,  "net", genString( item->GetNetname(), "NET" ) );
    // }

    auto add_track = [&]( PCB_TRACK* track )
    {
        if( track->Type() == PCB_TRACE_T )
        {
            PCB_SHAPE shape( nullptr, SHAPE_T::SEGMENT );
            shape.SetStart( track->GetStart() );
            shape.SetEnd( track->GetEnd() );
            shape.SetWidth( track->GetWidth() );
            AddShape( shape );
        }
        else if( track->Type() == PCB_ARC_T )
        {
            PCB_ARC* arc = static_cast<PCB_ARC*>( track );
            PCB_SHAPE shape( nullptr, SHAPE_T::ARC );
            shape.SetArcGeometry( arc->GetStart(), arc->GetMid(), arc->GetEnd() );
            shape.SetWidth( arc->GetWidth() );
            AddShape( shape );
        }
        else
        {
            // add via
            PCB_VIA* via = static_cast<PCB_VIA*>( track );

            if( via->FlashLayer( aLayer ) )
            {
                PCB_SHAPE shape( nullptr, SHAPE_T::CIRCLE );
            
                shape.SetPosition( via->GetPosition() );
                shape.SetEnd( { KiROUND( via->GetWidth() / 2.0 ), 0 } );
                AddShape( shape );

            }
        }
    };

    auto add_zone = [&]( ZONE* zone )
    {
        SHAPE_POLY_SET& zone_shape = *zone->GetFilledPolysList( aLayer );

        for( int ii = 0; ii < zone_shape.OutlineCount(); ++ii )
            AddContour( zone_shape, ii );
    };

    auto add_text = [&] ( BOARD_ITEM* text )
    {
        EDA_TEXT* text_item;
        FOOTPRINT* fp = text->GetParentFootprint();

        if( PCB_TEXT* tmp_text = dynamic_cast<PCB_TEXT*>( text ) )
            text_item = static_cast<EDA_TEXT*>( tmp_text );
        else if( PCB_TEXTBOX* tmp_text = dynamic_cast<PCB_TEXTBOX*>( text ) )
            text_item = static_cast<EDA_TEXT*>( tmp_text );

        if( text_item->GetShownText( false ).empty() )
            return;

        // wxXmlNode* tempSetNode = appendNode( aLayerNode, "Set" );

        // if( m_version > 'B' )
        //     addAttribute( tempSetNode,  "geometryUsage", "TEXT" );

        // if( fp )
        //     addAttribute( tempSetNode,  "componentRef", genString( fp->GetReference(), "CMP" ) );

        // wxXmlNode* nonStandardAttributeNode = appendNode( tempSetNode, "NonstandardAttribute" );
        // addAttribute( nonStandardAttributeNode,  "name", "TEXT" );
        // addAttribute( nonStandardAttributeNode,  "value", text_item->GetShownText( false ) );
        // addAttribute( nonStandardAttributeNode,  "type", "STRING" );

        // wxXmlNode* tempFeature = appendNode( tempSetNode, "Features" );
        
        
        // addLocationNode( tempFeature, 0.0, 0.0 );
        // addText( tempFeature, text_item, text->GetFontMetrics() );

        // if( text->Type() == PCB_TEXTBOX_T )
        // {
        //     PCB_TEXTBOX* textbox = static_cast<PCB_TEXTBOX*>( text );

        //     if( textbox->IsBorderEnabled() )
        //         addShape( tempFeature, *static_cast<PCB_SHAPE*>( textbox ) );
        // }
    };

    auto add_pad = [&]( PAD* pad )
    {
        FOOTPRINT* fp = pad->GetParentFootprint();
        
        if( fp && fp->IsFlipped() )
            AddPadShape( *pad, FlipLayer( aLayer ) );
        else
            AddPadShape( *pad, aLayer );
    };

    for( BOARD_ITEM* item : aItems )
    {
        switch( item->Type() )
        {
        case PCB_TRACE_T:
        case PCB_ARC_T:
        case PCB_VIA_T:
            add_track( static_cast<PCB_TRACK*>( item ) );
            break;

        case PCB_ZONE_T:
            add_zone( static_cast<ZONE*>( item ) );
            break;

        case PCB_PAD_T:
            add_pad( static_cast<PAD*>( item ) );
            break;

        case PCB_SHAPE_T:
            AddShape( static_cast<PCB_SHAPE*>( item ) );
            break;

        case PCB_TEXT_T:
        case PCB_TEXTBOX_T:
        case PCB_FIELD_T:
            add_text( item );
            break;

        case PCB_DIMENSION_T:
        case PCB_TARGET_T:
        case PCB_DIM_ALIGNED_T:
        case PCB_DIM_LEADER_T:
        case PCB_DIM_CENTER_T:
        case PCB_DIM_RADIAL_T:
        case PCB_DIM_ORTHOGONAL_T:
            //TODO: Add support for dimensions
            break;

        default:
            // wxLogDebug( "Unhandled type %s",
            //             ENUM_MAP<KICAD_T>::Instance().ToString( item->Type() ) );
        }
    }
}


void FEATURES_MANAGER::GenerateFeatureFile( std::ostream &ost ) const
{
    if ( m_featuresList.empty() )
        return;


    ost << "UNITS=MM" << std::endl;
    ost << "#Symbols" << std::endl;
    for( const auto &[diameter, n] : m_circleSymMap )
    {
        ost << "$" << n << " " << diameter << std::endl;
    }

    for( const auto &[dim, n] : m_rectSymMap )
    {
        ost << "$" << n << " " << dim.first << dim.second << std::endl;
    }

    for( const auto &[dim, n] : m_ovalSymMap )
    {
        ost << "$" << n << " " << dim.first << dim.second << std::endl;
    }

    for( const auto &[name, n] : m_padSymMap )
    {
        ost << "$" << n << " " << name << std::endl;
    }

    write_attributes(ost);

    for( const auto &feat : m_featuresList )
    {
        feat->WriteFeatures(ost);
    }
}

void ODB_FEATURE::WriteFeatures( std::ostream &ost )
{
    switch ( GetFeatureType() )
    {
    case FEATURE_TYPE::LINE:
        ost << "L";
        break;

    case FEATURE_TYPE::ARC:
        ost << "A";
        break;

    case FEATURE_TYPE::PAD:
        ost << "P";
        break;

    case FEATURE_TYPE::SURFACE:
        ost << "S";
        break;
    }

    ost << " ";
    WriteRecordContent( ost );

    write_attributes( ost );
    ost << std::endl;

}


void ODB_LINE::WriteRecordContent( std::ostream &ost )
{
    ost << m_start.first << " " << m_start.second << " " 
        << m_end.first << " " << m_end.second << " "
        << m_symIndex << " P 0";
}

void ODB_ARC::WriteRecordContent( std::ostream &ost )
{
    ost << m_start.first << " " << m_start.second << " " 
        << m_end.first << " " << m_end.second << " "
        << m_center.first << " " << m_center.second << " "
        << m_symIndex << " P 0 " 
        << ( m_direction == ODB_DIRECTION::CW ? "Y" : "N" );
}

void ODB_PAD::WriteRecordContent( std::ostream &ost )
{
    ost << m_center.first << " " << m_center.second << " ";

    // todo
    if( false )
    {
        ost << "-1" << " " << m_symIndex << " "
            << m_resize << " P 0 ";
    }
    else
    {
        ost << m_symIndex << " P 0 ";
    }

    if ( m_mirror )
        ost << "9";
    else
        ost << "8";

    ost << " " << ODB::Float2StrVal( m_angle.Normalize().AsDegrees() );
}

ODB_SURFACE::ODB_SURFACE( uint32_t aIndex, const SHAPE_POLY_SET::POLYGON& aPolygon,
                FILL_T aFillType = FILL_T::FILLED_SHAPE ) : ODB_FEATURE( aIndex )
{
    if( !aPolygon.empty() && aPolygon[0].PointCount() >= 3 )
    {
        m_surfaces = std::make_unique<ODB_SURFACE_DATA>( aPolygon );
        if( aFillType != FILL_T::NO_FILL )
        {
            m_surfaces->AddPolygonHoles( aPolygon );
        }
    }
    else
    {
        delete this;
    }

}



void ODB_SURFACE::WriteRecordContent( std::ostream &ost )
{
    ost << "P 0";
    m_surfaces->WriteData( ost );
    ost << "SE" << std::endl;
}


ODB_SURFACE_DATA::ODB_SURFACE_DATA( const SHAPE_POLY_SET::POLYGON& aPolygon )
{
    const std::vector<VECTOR2I>& pts = aPolygon[0].CPoints();
    m_polygons.at( 0 ).emplace_back( pts.back() );

    for( size_t jj = 0; jj < pts.size(); ++jj )
    {
        m_polygons.at( 0 ).emplace_back( pts.at( jj ) );
    }
}



bool ODB_SURFACE_DATA::AddPolygonHoles( const SHAPE_POLY_SET::POLYGON& aPolygon )
{
    for( size_t ii = 1; ii < aPolygon.size(); ++ii )
    {
        wxCHECK2( aPolygon[ii].PointCount() >= 3, continue );

        const std::vector<VECTOR2I>& hole = aPolygon[ii].CPoints();
        m_polygons.at( ii ).emplace_back( hole.back() );

        for( size_t jj = 0; jj < hole.size(); ++jj )
        {
            m_polygons.at( ii ).emplace_back( hole[jj] );
        }
    }

}


void ODB_SURFACE_DATA::WriteData( std::ostream &ost ) const
{
    Once is_island;
    for( const auto& contour : m_polygons )
    {
        ost << "OB " << ODB::AddXY( contour.back().m_end ).first << " "
            << ODB::AddXY( contour.back().m_end ).second << " ";
        if ( is_island() )
            ost << "I";
        else
            ost << "H";
        ost << std::endl;

        for( const auto& line : contour )
        {
            if ( SURFACE_LINE::LINE_TYPE::SEGMENT == line.m_type )
                ost << "OS " << ODB::AddXY( line.m_end ).first << " "
                    << ODB::AddXY( line.m_end ).second << std::endl;
            else
                ost << "OC " << ODB::AddXY( line.m_end ).first << " "
                    << ODB::AddXY( line.m_end ).second << " "
                    << ODB::AddXY( line.m_center ).first << " "
                    << ODB::AddXY( line.m_center ).second << " "
                    << (line.m_direction == ODB_DIRECTION::CW ? "Y" : "N") << std::endl;
        }
        ost << "OE" << std::endl;
    }

}
