
#include <base_units.h>
#include <board_stackup_manager/stackup_predefined_prms.h>
#include <build_version.h>
#include <callback_gal.h>
#include <connectivity/connectivity_data.h>
#include <connectivity/connectivity_algo.h>
#include <convert_basic_shapes_to_polygon.h>
#include <font/font.h>
#include <footprint.h>
#include <hash_eda.h>
#include <pad.h>
#include <pcb_dimension.h>
#include <pcb_shape.h>
#include <pcb_text.h>
#include <pcb_textbox.h>
#include <pcb_track.h>
#include <pcbnew_settings.h>
#include <board_design_settings.h>
#include <pgm_base.h>
#include <progress_reporter.h>
#include <settings/settings_manager.h>
#include <string_utf8_map.h>
#include <wx_fstream_progress.h>

#include <geometry/shape_circle.h>
#include <geometry/shape_line_chain.h>
#include <geometry/shape_poly_set.h>
#include <geometry/shape_segment.h>

#include <wx/log.h>
#include <wx/numformatter.h>
#include <wx/mstream.h>

#include "odb_attribute.h"
#include "odb_entity.h"
#include "odb_defines.h"
#include "odb_feature.h"
#include "odb_util.h"
#include "odb_plugin.h"
#include "odb_eda_data.h"


bool ODB_ENTITY_BASE::CreateDirectiryTree( ODB_TREE_WRITER& writer )
{
    try
    {
        writer.CreateEntityDirectory( writer.GetRootPath(), GetEntityName() );
        return true;
    }
    catch( const std::exception& e )
    {
        std::cerr << e.what() << std::endl;
        return false;
    }

}


ODB_MISC_ENTITY::ODB_MISC_ENTITY( const std::vector<wxString>& aValue )
{
    m_info = 
    {
        { wxS( ODB_JOB_NAME ), wxS( "job" ) },
        { wxS( ODB_UNITS ), wxS( "MM" ) },
        { wxS( "ODB_VERSION_MAJOR" ), wxS( "8" ) },
        { wxS( "ODB_VERSION_MINOR" ), wxS( "0" ) },
        { wxS( "ODB_SOURCE" ), wxS( "KiCad EDA" + GetMajorMinorPatchVersion() ) },
        { wxS( "CREATION_DATE" ), wxDateTime::Now().FormatISOCombined() },
        { wxS( "SAVE_DATE" ), wxDateTime::Now().FormatISOCombined() },
        { wxS( "SAVE_APP" ), wxS( "Pcbnew" ) },
        { wxS( "SAVE_USER" ), wxS( "" ) },
        { wxS( "MAX_UID" ), wxS( "" ) }
    };

    if( !aValue.at( 0 ).IsEmpty() )
    {
        m_info[ wxS( ODB_JOB_NAME ) ] = aValue.at( 0 );
    }

    if( !aValue.at( 1 ).IsEmpty() )
    {
        m_info[ wxS( ODB_UNITS ) ] = aValue.at( 1 );
    }
}

bool ODB_MISC_ENTITY::GenerateInfoFile( ODB_TREE_WRITER& writer )
{
    auto fileproxy = writer.CreateFileProxy( "info" );
    
    ODB_TEXT_WRITER twriter( fileproxy.GetStream() );
    for( auto& info : m_info )
    {
        twriter.write_line( info.first, info.second );
    }
    
    return fileproxy.CloseFile();
}

bool ODB_MISC_ENTITY::GenerateFiles( ODB_TREE_WRITER& writer )
{
    if( !GenerateInfoFile( writer ) )
    {
        return false;
    }

    return true;
}


void ODB_MATRIX_ENTITY::AddStep( const wxString& aStepName )
{
    m_matrixSteps.emplace( aStepName.Upper(), m_col++ );
}

void ODB_MATRIX_ENTITY::InitEntityData()
{
    AddStep( "PCB" );

    InitMatrixLayerData();
}

void ODB_MATRIX_ENTITY::InitMatrixLayerData()
{
    BOARD_DESIGN_SETTINGS& dsnSettings = m_board->GetDesignSettings();
    BOARD_STACKUP&         stackup = dsnSettings.GetStackupDescriptor();
    stackup.SynchronizeWithBoard( &dsnSettings );

    std::vector<BOARD_STACKUP_ITEM*> layers = stackup.GetList();
    std::set<PCB_LAYER_ID> added_layers;

    for( int i = 0; i < stackup.GetCount(); i++ )
    {
        BOARD_STACKUP_ITEM* stackup_item = layers.at( i );

        for( int sublayer_id = 0; sublayer_id < stackup_item->GetSublayersCount(); sublayer_id++ )
        {
            wxString ly_name = stackup_item->GetLayerName();

            if( ly_name.IsEmpty() )
            {
                if( IsValidLayer( stackup_item->GetBrdLayerId() ) )
                    ly_name = m_board->GetLayerName( stackup_item->GetBrdLayerId() );

                if( ly_name.IsEmpty() && stackup_item->GetType() == BS_ITEM_TYPE_DIELECTRIC )
                    ly_name = wxString::Format( "DIELECTRIC_%d", stackup_item->GetDielectricLayerId() );
            }

            MATRIX_LAYER mLayer( m_row++, ly_name );

            if( stackup_item->GetType() == BS_ITEM_TYPE_DIELECTRIC )
            {
                if( stackup_item->GetTypeName() == KEY_CORE )
                    mLayer.m_diType.emplace( ODB_DIELECTRIC_TYPE::CORE );
                else
                    mLayer.m_diType.emplace( ODB_DIELECTRIC_TYPE::PREPREG );
                
                mLayer.m_type = ODB_TYPE::DIELECTRIC;
                mLayer.m_context = ODB_CONTEXT::BOARD;
                mLayer.m_polarity = ODB_POLARITY::POSITIVE;
                m_matrixLayers.push_back( mLayer );
                m_plugin->GetLayerNameList().emplace_back( 
                    std::make_pair( PCB_LAYER_ID::UNDEFINED_LAYER,
                                    mLayer.m_layerName ) );

                continue;
            }
            else
            {
                added_layers.insert( stackup_item->GetBrdLayerId() );
                AddMatrixLayerField( mLayer, stackup_item->GetBrdLayerId() );
            }
        }
    }

    LSEQ layer_seq = m_board->GetEnabledLayers().Seq();

    for( PCB_LAYER_ID layer : layer_seq )
    {
        if( added_layers.find( layer ) != added_layers.end() )
            continue;

        MATRIX_LAYER mLayer( m_row++, m_board->GetLayerName( layer ) );
        added_layers.insert( layer );
        AddMatrixLayerField( mLayer, layer );
    }

    AddDrillMatrixLayer();
    AddCOMPMatrixLayer();
}

void ODB_MATRIX_ENTITY::AddMatrixLayerField( MATRIX_LAYER& aMLayer, PCB_LAYER_ID aLayer )
{
    aMLayer.m_polarity = ODB_POLARITY::POSITIVE;
    aMLayer.m_context = ODB_CONTEXT::BOARD;
    switch( aLayer )
    {
    case F_Paste:
    case B_Paste:
        aMLayer.m_type = ODB_TYPE::SOLDER_PASTE;
        break;
    case F_SilkS:
    case B_SilkS:
        aMLayer.m_type = ODB_TYPE::SILK_SCREEN;
        break;
    case F_Mask:
    case B_Mask:
        aMLayer.m_type = ODB_TYPE::SOLDER_MASK;
        break;
    // case B_CrtYd:
    // case F_CrtYd:
    // case B_Fab:
    // case F_Fab:
    case F_Adhes:
    case B_Adhes:
    case Dwgs_User:
    case Cmts_User:
    case Eco1_User:
    case Eco2_User:
    case Margin:
    case User_1:
    case User_2:
    case User_3:
    case User_4:
    case User_5:
    case User_6:
    case User_7:
    case User_8:
    case User_9:
        aMLayer.m_context = ODB_CONTEXT::MISC;
        aMLayer.m_type = ODB_TYPE::DOCUMENT;
        break;

    default:
        if( IsCopperLayer( aLayer ) )
        {
            aMLayer.m_type = ODB_TYPE::SIGNAL;
        }
        else
        {
            // Do not handle other layers :
            // Edge_Cuts
            aMLayer.m_type = ODB_TYPE::UNDEFINED;
            m_row--;
        }

        break; 
    }

    if( aMLayer.m_type!= ODB_TYPE::UNDEFINED )
    {
        m_matrixLayers.push_back( aMLayer );
        m_plugin->GetLayerNameList().emplace_back( 
            std::make_pair( aLayer, aMLayer.m_layerName ) );
    }
}


void ODB_MATRIX_ENTITY::AddDrillMatrixLayer()
{
    std::map<std::pair<PCB_LAYER_ID, PCB_LAYER_ID>, std::vector<BOARD_ITEM*>>&
            drill_layers = m_plugin->GetDrillLayerItemsMap();

    std::map<std::pair<PCB_LAYER_ID, PCB_LAYER_ID>, std::vector<PAD*>>&
            slot_holes = m_plugin->GetSlotHolesMap();

    for( BOARD_ITEM* item : m_board->Tracks() )
    {
        if( item->Type() == PCB_VIA_T )
        {
            PCB_VIA* via = static_cast<PCB_VIA*>( item );
            drill_layers[std::make_pair( via->TopLayer(), via->BottomLayer() )].push_back( via );
        }
    }

    for( FOOTPRINT* fp : m_board->Footprints() )
    {
        for( PAD* pad : fp->Pads() )
        {
            if( pad->HasHole() && pad->GetDrillSizeX() != pad->GetDrillSizeY() )
                slot_holes[std::make_pair( F_Cu, B_Cu )].push_back( pad );
            else if( pad->HasHole() )
                drill_layers[std::make_pair( F_Cu, B_Cu )].push_back( pad );
        }
    }

    // TODO: FUNCTIONAL
    for( const auto& [layer_pair, vec] : drill_layers )
    {
        wxString dLayerName = wxString::Format( "DRILL_%s-%s",
                        m_board->GetLayerName( layer_pair.first ),
                        m_board->GetLayerName( layer_pair.second ) );
        MATRIX_LAYER mLayer( m_row++, dLayerName );
        mLayer.m_type = ODB_TYPE::DRILL;
        mLayer.m_context = ODB_CONTEXT::BOARD;
        mLayer.m_polarity = ODB_POLARITY::POSITIVE;
        mLayer.m_span.emplace( 
            std::make_pair( m_board->GetLayerName( layer_pair.first ),
                            m_board->GetLayerName( layer_pair.second ) ) );
        m_matrixLayers.push_back( mLayer );
        m_plugin->GetLayerNameList().emplace_back( 
                std::make_pair( PCB_LAYER_ID::UNDEFINED_LAYER,
                                mLayer.m_layerName ) );

    }

    for( const auto& [layer_pair, vec] : slot_holes )
    {
        wxString dLayerName = wxString::Format( "SLOT_%s-%s",
                        m_board->GetLayerName( layer_pair.first ),
                        m_board->GetLayerName( layer_pair.second ) );
        MATRIX_LAYER mLayer( m_row++, dLayerName );
        mLayer.m_type = ODB_TYPE::ROUT;
        mLayer.m_context = ODB_CONTEXT::BOARD;
        mLayer.m_polarity = ODB_POLARITY::POSITIVE;
        mLayer.m_span.emplace( 
                std::make_pair( m_board->GetLayerName( layer_pair.first ),
                                m_board->GetLayerName( layer_pair.second ) ) );
        m_matrixLayers.push_back( mLayer );
        m_plugin->GetLayerNameList().emplace_back( 
                std::make_pair( PCB_LAYER_ID::UNDEFINED_LAYER,
                                mLayer.m_layerName ) );

    }
}


void ODB_MATRIX_ENTITY::AddCOMPMatrixLayer()
{
            aMLayer.m_type = ODB_TYPE::COMPONENT;
        aMLayer.m_layerName = wxString::Format( "COMP_+_%s",
                               aLayer == F_Fab ? "TOP" : "BOT" );
}





bool ODB_MATRIX_ENTITY::GenerateFiles( ODB_TREE_WRITER& writer )
{
    if( !GenerateMatrixFile( writer ) )
    {
        return false;
    }

    return true;
}


bool ODB_MATRIX_ENTITY::GenerateMatrixFile( ODB_TREE_WRITER& writer )
{
    auto fileproxy = writer.CreateFileProxy( "matrix" );
    
    ODB_TEXT_WRITER twriter( fileproxy.GetStream() );

    for ( const auto &[step_name, column] : m_matrixSteps )
    {
        const auto array_proxy = twriter.make_array_proxy( "STEP" );
        twriter.write_line( "COL", column );
        twriter.write_line( "NAME", step_name );
    }

    for ( const auto &layer : m_matrixLayers )
    {
        const auto array_proxy = twriter.make_array_proxy( "LAYER" );
        twriter.write_line( "ROW", layer.m_rowNumber );
        twriter.write_line_enum( "CONTEXT", layer.m_context );
        twriter.write_line_enum( "TYPE", layer.m_type );

        if ( layer.m_addType.has_value() )
        {
            twriter.write_line_enum( "ADD_TYPE", layer.m_addType.value() );
        }

        twriter.write_line( "NAME", layer.m_layerName );
        twriter.write_line( "OLD_NAME", wxEmptyString );
        twriter.write_line_enum( "POLARITY", layer.m_polarity );

        if ( layer.m_diType.has_value() )
        {
            twriter.write_line_enum( "DIELECTRIC_TYPE", layer.m_diType.value() );
        }

        twriter.write_line( "DIELECTRIC_NAME", wxEmptyString );
        twriter.write_line( "CU_TOP", wxEmptyString );
        twriter.write_line( "CU_BOTTOM", wxEmptyString );
        twriter.write_line( "REF", wxEmptyString );
        if ( layer.m_span.has_value() )
        {
            twriter.write_line( "START_NAME", layer.m_span->first );
            twriter.write_line( "END_NAME", layer.m_span->second );
        }
        else
        {
            twriter.write_line( "START_NAME", wxEmptyString );
            twriter.write_line( "END_NAME", wxEmptyString );
        }
        twriter.write_line( "COLOR", wxEmptyString );
    }

    return fileproxy.CloseFile();
}

ODB_LAYER_ENTITY::ODB_LAYER_ENTITY( BOARD* aBoard, ODB_PLUGIN* aPlugin,
      std::map<int, std::vector<BOARD_ITEM*>>& aMap,
      const PCB_LAYER_ID& aLayerID, const wxString& aLayerName )
       : ODB_ENTITY_BASE( aBoard, aPlugin ),
         m_layerItems( aMap ), m_layerID( aLayerID ), m_matrixLayerName( aLayerName )
{
    m_featuresMgr = std::make_unique<FEATURES_MANAGER>( aBoard );
    // m_shape_std_node = appendNode( m_layer_node, "ShapeStandard" );
    // m_profile = std::make_unique<FEATURES_MANAGER>( aBoard );

}

void ODB_LAYER_ENTITY::InitEntityData()
{
    if ( m_matrixLayerName.Contains( "COMP" ) )
    {
        InitComponentData();
        return;
    }

    if( m_matrixLayerName.Contains( "DRILL" ) )
    {
        InitDrillData();
        return;
    }

    if( m_matrixLayerName.Contains( "SLOT" ) )
    {
        InitSlotData();
        return;
    }

    if( m_layerID != PCB_LAYER_ID::UNDEFINED_LAYER )
    {
        InitFeatureData();
    }
}

void ODB_LAYER_ENTITY::InitFeatureData()
{
    if( m_layerItems.empty() )
        return;
    
    const NETINFO_LIST& nets = m_board->GetNetInfo();

    for( const NETINFO_ITEM* net : nets )
    {
        std::vector<BOARD_ITEM*>& vec = m_layerItems[net->GetNetCode()];

        std::stable_sort( vec.begin(), vec.end(),
                    []( BOARD_ITEM* a, BOARD_ITEM* b )
                    {
                        if( a->GetParentFootprint() == b->GetParentFootprint() )
                            return a->Type() < b->Type();

                        return a->GetParentFootprint() < b->GetParentFootprint();
                    } );

        if( vec.empty() )
            continue;

        m_featuresMgr->InitFeatureList( m_layerID, vec );
    }
}


void ODB_LAYER_ENTITY::InitComponentData()
{

    

}

void ODB_LAYER_ENTITY::InitDrillData()
{
    std::map<std::pair<PCB_LAYER_ID, PCB_LAYER_ID>, std::vector<BOARD_ITEM*>>&
            drill_layers = m_plugin->GetDrillLayerItemsMap();

    
    if( !m_layerItems.empty() )
    {
        m_layerItems.clear();
    }
    
    m_tools.emplace( "MM" );

    for( const auto& [layer_pair, vec] : drill_layers )
    {
        wxString dLayerName = wxString::Format( "DRILL_%s-%s",
                        m_board->GetLayerName( layer_pair.first ),
                        m_board->GetLayerName( layer_pair.second ) );
        if( dLayerName == m_matrixLayerName )
        {
            
            for( BOARD_ITEM* item : vec )
            {
                //for drill tools
                if( item->Type() == PCB_VIA_T )
                {
                    PCB_VIA* via = static_cast<PCB_VIA*>( item );

                    m_tools.AddDrillTools(
                        "VIA",
                        ODB::Float2StrVal( m_ODBScale * via->GetDrillValue() ) );

                }
                else if( item->Type() == PCB_PAD_T )
                {
                    PAD* pad = static_cast<PAD*>( item );

                    m_tools.AddDrillTools(
                        pad->GetAttribute() == PAD_ATTRIB::PTH ? "PLATED" : "NON_PLATED",
                        ODB::Float2StrVal( m_ODBScale * pad->GetDrillSizeX() ) );

                }

                //for drill features
                m_layerItems[item->GetNetCode()].push_back( item );
            }

            break;
        }
    }

    InitFeatureData();
}

void ODB_LAYER_ENTITY::InitSlotData()
{
    std::map<std::pair<PCB_LAYER_ID, PCB_LAYER_ID>, std::vector<PAD*>>&
        slot_holes = m_plugin->GetSlotHolesMap();

    if( !m_layerItems.empty() )
    {
        m_layerItems.clear();
    }

    m_tools.emplace();
    
    for( const auto& [layer_pair, vec] : slot_holes )
    {
        wxString sLayerName = wxString::Format( "SLOT_%s-%s",
                        m_board->GetLayerName( layer_pair.first ),
                        m_board->GetLayerName( layer_pair.second ) );

        if( sLayerName == m_matrixLayerName )
        {
            for( PAD* pad : vec )
            {
                //for slot tools
                m_tools.AddDrillTools(
                    pad->GetAttribute() == PAD_ATTRIB::PTH ? "PLATED" : "NON_PLATED",
                    ODB::Float2StrVal( m_ODBScale * pad->GetDrillSizeX() ) );

                //for slot features
                m_layerItems[pad->GetNetCode()].push_back( pad );
            }

            break;
        }
    }

    InitFeatureData();
}



void ODB_STEP_ENTITY::InitEntityData()
{
    InitEdaData();
    InitNetListData();
    InitLayerEntityData();
}

void ODB_STEP_ENTITY::InitPackage( FOOTPRINT* aFp )
{
    std::unique_ptr<FOOTPRINT> fp( static_cast<FOOTPRINT*>( aFp->Clone() ) );
    fp->SetParentGroup( nullptr );
    fp->SetPosition( { 0, 0 } );

    if( fp->GetLayer() != F_Cu )
        fp->Flip( fp->GetPosition(), false );

    fp->SetOrientation( EDA_ANGLE::m_Angle0 );


    size_t hash = hash_fp_item( fp.get(), HASH_POS | REL_COORD );
    wxString name = wxString::Format( "%s_%zu", fp->GetFPID().GetLibItemName().wx_str(),
                             m_footprint_dict.size() + 1 ).ToAscii();

    auto [ iter, success ] = m_footprint_dict.emplace( hash, name );

    if( !success )
        return;

    // Package and Component nodes are at the same level, so we need to find the parent
    // which should be the Step node
    wxXmlNode* packageNode = new wxXmlNode( wxXML_ELEMENT_NODE, "Package" );
    wxXmlNode* otherSideViewNode = nullptr; // Only set this if we have elements on the back side

    addAttribute( packageNode,  "name", name );
    addAttribute( packageNode,  "type", "OTHER" ); // TODO: Replace with actual package type once we encode this

    // We don't specially identify pin 1 in our footprints, so we need to guess
    if( fp->FindPadByNumber( "1" ) )
        addAttribute( packageNode,  "pinOne", "1" );
    else if ( fp->FindPadByNumber( "A1" ) )
        addAttribute( packageNode,  "pinOne", "A1" );
    else if ( fp->FindPadByNumber( "A" ) )
        addAttribute( packageNode,  "pinOne", "A" );
    else if ( fp->FindPadByNumber( "a" ) )
        addAttribute( packageNode,  "pinOne", "a" );
    else if ( fp->FindPadByNumber( "a1" ) )
        addAttribute( packageNode,  "pinOne", "a1" );
    else if ( fp->FindPadByNumber( "Anode" ) )
        addAttribute( packageNode,  "pinOne", "Anode" );
    else if ( fp->FindPadByNumber( "ANODE" ) )
        addAttribute( packageNode,  "pinOne", "ANODE" );
    else
        addAttribute( packageNode,  "pinOne", "UNKNOWN" );

    addAttribute( packageNode,  "pinOneOrientation", "OTHER" );

    const SHAPE_POLY_SET& courtyard = fp->GetCourtyard( F_CrtYd );
    const SHAPE_POLY_SET& courtyard_back = fp->GetCourtyard( B_CrtYd );

    if( courtyard.OutlineCount() > 0 )
        addOutlineNode( packageNode, courtyard, courtyard.Outline( 0 ).Width(), LINE_STYLE::SOLID );

    if( courtyard_back.OutlineCount() > 0 )
    {
        otherSideViewNode = appendNode( packageNode, "OtherSideView" );
        addOutlineNode( otherSideViewNode, courtyard_back, courtyard_back.Outline( 0 ).Width(), LINE_STYLE::SOLID );
    }

    if( !courtyard.OutlineCount() && !courtyard_back.OutlineCount() )
    {
        SHAPE_POLY_SET bbox = fp->GetBoundingHull();
        addOutlineNode( packageNode, bbox );
    }

    wxXmlNode* pickupPointNode = appendNode( packageNode, "PickupPoint" );
    addAttribute( pickupPointNode,  "x", "0.0" );
    addAttribute( pickupPointNode,  "y", "0.0" );

    std::map<PCB_LAYER_ID, std::map<bool, std::vector<BOARD_ITEM*>>> elements;

    for( BOARD_ITEM* item : fp->GraphicalItems() )
    {
        PCB_LAYER_ID layer = item->GetLayer();

        /// IPC2581 only supports the documentation layers for production and post-production
        /// All other layers are ignored
        /// TODO: Decide if we should place the other layers from footprints on the board
        if( layer != F_SilkS && layer != B_SilkS && layer != F_Fab && layer != B_Fab )
            continue;

        bool is_abs = true;

        if( item->Type() == PCB_SHAPE_T )
        {
            PCB_SHAPE* shape = static_cast<PCB_SHAPE*>( item );

            // Circles and Rectanges only have size information so we need to place them in
            // a separate node that has a location
            if( shape->GetShape() == SHAPE_T::CIRCLE || shape->GetShape() == SHAPE_T::RECTANGLE )
                is_abs = false;
        }

        elements[item->GetLayer()][is_abs].push_back( item );
    }

    auto add_base_node = [&]( PCB_LAYER_ID aLayer ) -> wxXmlNode*
    {
        wxXmlNode* parent = packageNode;
        bool is_back = aLayer == B_SilkS || aLayer == B_Fab;

        if( is_back )
        {
            if( !otherSideViewNode )
                otherSideViewNode = appendNode( packageNode, "OtherSideView" );

            parent = otherSideViewNode;
        }

        wxString name;

        if( aLayer == F_SilkS || aLayer == B_SilkS )
            name = "SilkScreen";
        else if( aLayer == F_Fab || aLayer == B_Fab )
            name = "AssemblyDrawing";
        else
            wxASSERT( false );

        wxXmlNode* new_node = appendNode( parent, name );
        return new_node;
    };

    auto add_marking_node = [&]( wxXmlNode* aNode ) -> wxXmlNode*
    {
        wxXmlNode* marking_node = appendNode( aNode, "Marking" );
        addAttribute( marking_node,  "markingUsage", "NONE" );
        return marking_node;
    };

    std::map<PCB_LAYER_ID, wxXmlNode*> layer_nodes;
    std::map<PCB_LAYER_ID, BOX2I> layer_bbox;

    for( auto layer : { F_Fab, B_Fab } )
    {
        if( elements.find( layer ) != elements.end() )
        {
            if( elements[layer][true].size() > 0 )
                layer_bbox[layer] = elements[layer][true][0]->GetBoundingBox();
            else if( elements[layer][false].size() > 0 )
                layer_bbox[layer] = elements[layer][false][0]->GetBoundingBox();
        }
    }

    for( auto& [layer, map] : elements )
    {
        wxXmlNode* layer_node = add_base_node( layer );
        wxXmlNode* marking_node = add_marking_node( layer_node );
        wxXmlNode* group_node = appendNode( marking_node, "UserSpecial" );
        bool update_bbox = false;

        if( layer == F_Fab || layer == B_Fab )
        {
            layer_nodes[layer] = layer_node;
            update_bbox = true;
        }

        for( auto& [is_abs, vec] : map )
        {
            for( BOARD_ITEM* item : vec )
            {
                wxXmlNode* output_node = nullptr;

                if( update_bbox )
                    layer_bbox[layer].Merge( item->GetBoundingBox() );

                if( !is_abs )
                    output_node = add_marking_node( layer_node );
                else
                    output_node = group_node;

                switch( item->Type() )
                {
                case PCB_TEXT_T:
                {
                    PCB_TEXT* text = static_cast<PCB_TEXT*>( item );
                    addText( output_node, text, text->GetFontMetrics() );
                    break;
                }

                case PCB_TEXTBOX_T:
                {
                    PCB_TEXTBOX* text = static_cast<PCB_TEXTBOX*>( item );
                    addText( output_node, text, text->GetFontMetrics() );

                    // We want to force this to be a polygon to get absolute coordinates
                    if( text->IsBorderEnabled() )
                    {
                        SHAPE_POLY_SET poly_set;
                        text->GetEffectiveShape()->TransformToPolygon( poly_set, 0, ERROR_INSIDE );
                        addContourNode( output_node, poly_set, 0, FILL_T::NO_FILL,
                                        text->GetBorderWidth() );
                    }

                    break;
                }

                case PCB_SHAPE_T:
                {
                    if( !is_abs )
                        addLocationNode( output_node, static_cast<PCB_SHAPE*>( item ) );

                    addShape( output_node, *static_cast<PCB_SHAPE*>( item ) );

                    break;
                }

                default: break;
                }
            }
        }

        if( group_node->GetChildren() == nullptr )
        {
            marking_node->RemoveChild( group_node );
            layer_node->RemoveChild( marking_node );
            delete group_node;
            delete marking_node;
        }
    }

    for( auto&[layer, bbox] : layer_bbox)
    {
        if( bbox.GetWidth() > 0 )
        {
            wxXmlNode* outlineNode = insertNode( layer_nodes[layer], "Outline" );

            SHAPE_POLY_SET::POLYGON outline( 1 );
            std::vector<VECTOR2I> points( 4 );
            points[0] = bbox.GetPosition();
            points[2] = bbox.GetEnd();
            points[1].x = points[0].x;
            points[1].y = points[2].y;
            points[3].x = points[2].x;
            points[3].y = points[0].y;

            outline[0].Append( points );
            addPolygonNode( outlineNode, outline, FILL_T::NO_FILL, 0 );
            addLineDesc( outlineNode, 0, LINE_STYLE::SOLID );
        }
    }

    for( size_t ii = 0; ii < fp->Pads().size(); ++ii )
    {
        PAD* pad = fp->Pads()[ii];
        wxXmlNode* pinNode = appendNode( packageNode, "Pin" );
        wxString name = pad->GetNumber();

        // Pins are required to have names, so if our pad doesn't have a name, we need to
        // generate one that is unique
        if( pad->GetAttribute() == PAD_ATTRIB::NPTH )
            name = wxString::Format( "NPTH%zu", ii );
        else if( name.empty() )
            name = wxString::Format( "PAD%zu", ii );

        addAttribute( pinNode,  "number", name );

        m_net_pin_dict[pad->GetNetCode()].emplace_back(
                genString( fp->GetReference(), "CMP" ), genString( name, "PIN" ) );

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

        if( pad->GetFPRelativeOrientation() != EDA_ANGLE::m_Angle0 )
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

    return packageNode;
}


bool ODB_LAYER_ENTITY::GenerateFiles( ODB_TREE_WRITER &writer )
{

    if ( !GenAttrList() )
    {
        /* code */
    }
    
    if ( !GenComponents( writer ))
    {
        /* code */
    }
    
    if ( !GenFeatures( writer ) )
    {
        /* code */
    }

    if ( !GenTools( writer ) )
    {
        /* code */
    }
    
    return true;

}

bool ODB_LAYER_ENTITY::GenFeatures( ODB_TREE_WRITER &writer )
{
    auto fileproxy = writer.CreateFileProxy( "features" );
    
    m_featuresMgr->GenerateFeatureFile( fileproxy.GetStream() );
    return true;
}

bool ODB_LAYER_ENTITY::GenTools( ODB_TREE_WRITER &writer )
{
    auto fileproxy = writer.CreateFileProxy( "tools" );

    return m_tools.GenerateFile( fileproxy.GetStream() );

}


void ODB_STEP_ENTITY::InitEdaData()
{
    std::vector<wxXmlNode*> componentNodes;
    std::vector<wxXmlNode*> packageNodes;
    std::set<wxString> packageNames;

    bool generate_unique = m_OEMRef.empty();

    for( FOOTPRINT* fp : m_board->Footprints() )
    {
        wxXmlNode* componentNode = new wxXmlNode( wxXML_ELEMENT_NODE, "Component" );
        addAttribute( componentNode,  "refDes", genString( fp->GetReference(), "CMP" ) );
        wxXmlNode* pkg = addPackage( componentNode, fp );

        if( pkg )
            packageNodes.push_back( pkg );

        wxString name;

        PCB_FIELD* field = nullptr;

        if( !generate_unique )
            field = fp->GetFieldByName( m_OEMRef );

        if( field && !field->GetText().empty() )
        {
            name = field->GetShownText( false );
        }
        else
        {
            name = wxString::Format( "%s_%s_%s", fp->GetFPID().GetFullLibraryName(),
                                     fp->GetFPID().GetLibItemName().wx_str(),
                                     fp->GetValue() );
        }

        if( !m_OEMRef_dict.emplace( fp, name ).second )
            wxLogError( "Duplicate footprint pointers.  Please report this bug." );

        addAttribute( componentNode,  "part", name );
        addAttribute( componentNode,  "layerRef", m_layer_name_map[fp->GetLayer()] );

        if( fp->GetAttributes() & FP_THROUGH_HOLE )
            addAttribute( componentNode,  "mountType", "THMT" );
        else if( fp->GetAttributes() & FP_SMD )
            addAttribute( componentNode,  "mountType", "SMT" );
        else
            addAttribute( componentNode,  "mountType", "OTHER" );

        if( fp->GetOrientation() != EDA_ANGLE::m_Angle0 || fp->GetLayer() != F_Cu )
        {
            wxXmlNode* xformNode = appendNode( componentNode, "Xform" );

            if( fp->GetOrientation() != EDA_ANGLE::m_Angle0 )
                addAttribute( xformNode,  "rotation", floatVal( fp->GetOrientation().Normalize().AsDegrees() ) );

            if( fp->GetLayer() != F_Cu )
                addAttribute( xformNode,  "mirror", "true" );
        }

        addLocationNode( componentNode, fp->GetPosition().x, fp->GetPosition().y );

        componentNodes.push_back( componentNode );
    }

    for( wxXmlNode* padstack : m_padstacks )
    {
        insertNode( aStepNode, padstack );
        m_last_padstack = padstack;
    }

    for( wxXmlNode* pkg : packageNodes )
        aStepNode->AddChild( pkg );

    for( wxXmlNode* cmp : componentNodes )
        aStepNode->AddChild( cmp );
}


void ODB_STEP_ENTITY::InitNetListData()
{
}

bool ODB_STEP_ENTITY::GenerateFiles( ODB_TREE_WRITER& writer )
{
    wxString step_root = writer.GetCurrentPath();

    writer.CreateEntityDirectory( step_root, "layers" );
    if( !GenerateLayerFiles( writer ) )
    {
        return false;
    }

    writer.CreateEntityDirectory( step_root, "eda" );
    if( !GenerateEdaFiles( writer ) )
    {
        return false;
    }

    writer.CreateEntityDirectory( step_root, "netlists" );
    if( !GenerateNetlistsFiles( writer ) )
    {
        return false;
    }

    writer.SetCurrentPath( step_root );
    if( !GenerateProfileFile( writer ) )
    {
        return false;
    }

    if( !GenerateStepHeaderFile( writer ) )
    {
        return false;
    }

    if( !GenerateAttrListFile( writer ) )
    {
        return false;
    }


    return true;
}

bool ODB_STEP_ENTITY::GenerateProfileFile( ODB_TREE_WRITER& writer )
{
    auto fileproxy = writer.CreateFileProxy( "profile" );
        
    std::map<PCB_LAYER_ID, std::map<int, std::vector<BOARD_ITEM*>>>&
                layerElementMap = m_plugin->GetLayerElementsMap();

    m_profile = std::make_unique<FEATURES_MANAGER>( m_board );

    SHAPE_POLY_SET board_outline;

    if( !m_board->GetBoardPolygonOutlines( board_outline ) )
    {
        wxLogError( "Failed to get board outline" );
        return false;
    }

    if( !m_profile->AddContour( board_outline, 0 ) )
    {
        wxLogDebug( "Failed to add polygon to profile" );
        return false;
    }

    m_profile->GenerateProfileFeatures( fileproxy.GetStream() );
    
    return fileproxy.CloseFile();
}


bool ODB_STEP_ENTITY::GenerateStepHeaderFile( ODB_TREE_WRITER& writer )
{
    auto fileproxy = writer.CreateFileProxy( "stephdr" );

    m_stephdr = 
    {
        { ODB_UNITS, "MM" },
        { "X_DATUM", "0" },
        { "Y_DATUM", "0" },
        { "X_ORIGIN", "0" },
        { "Y_ORIGIN", "0" },
        { "TOP_ACTIVE", "0" },
        { "BOTTOM_ACTIVE", "0" },
        { "RIGHT_ACTIVE", "0" },
        { "LEFT_ACTIVE", "0" },
        { "AFFECTING_BOM", "" },
        { "AFFECTING_BOM_CHANGED", "0" },
    };

    ODB_TEXT_WRITER twriter( fileproxy.GetStream() );

    for( const auto &[key, value] : m_stephdr )
    {
        twriter.write_line( key, value );
    }

    return fileproxy.CloseFile();
}



bool ODB_STEP_ENTITY::GenerateLayerFiles( ODB_TREE_WRITER& writer )
{
    wxString layers_root = writer.GetCurrentPath();
    for( auto& [layerName, layerEntity] : m_layerEntityMap )
    {
        writer.CreateEntityDirectory( layers_root, layerName );

        if( !layerEntity->GenerateFiles( writer ) )
        {
            return false;
        }
    }
    return true;
}

ODB_STEP_ENTITY::ODB_STEP_ENTITY( BOARD* aBoard, ODB_PLUGIN* aPlugin )
     : ODB_ENTITY_BASE( aBoard, aPlugin )
{
    m_profile = nullptr;
}

bool ODB_STEP_ENTITY::CreateDirectiryTree( ODB_TREE_WRITER& writer )
{
    try
    {
        writer.CreateEntityDirectory( writer.GetRootPath(), "steps" );
        writer.CreateEntityDirectory( writer.GetCurrentPath(), GetEntityName() );
        return true;
    }
    catch( const std::exception& e )
    {
        std::cerr << e.what() << std::endl;
        return false;
    }
}

void ODB_STEP_ENTITY::InitLayerEntityData()
{
    LSEQ layers = m_board->GetEnabledLayers().Seq();
    const NETINFO_LIST& nets = m_board->GetNetInfo();
    std::vector<std::unique_ptr<FOOTPRINT>> footprints;

    // To avoid the overhead of repeatedly cycling through the layers and nets,
    // we pre-sort the board items into a map of layer -> net -> items
    std::map<PCB_LAYER_ID, std::map<int, std::vector<BOARD_ITEM*>>>& elements =
        m_plugin->GetLayerElementsMap();

    std::for_each(
        m_board->Tracks().begin(),
        m_board->Tracks().end(),
        [&layers, &elements]( PCB_TRACK* aTrack )
        {
            if( aTrack->Type() == PCB_VIA_T )
            {
                PCB_VIA* via = static_cast<PCB_VIA*>( aTrack );

                for( PCB_LAYER_ID layer : layers )
                {
                    if( via->FlashLayer( layer ) )
                        elements[layer][via->GetNetCode()].push_back( via );
                }
            }
            else
            {
                elements[aTrack->GetLayer()][aTrack->GetNetCode()].push_back( aTrack );
            }
        } );

    std::for_each(
        m_board->Zones().begin(),
        m_board->Zones().end(),
        [ &elements ]( ZONE* zone )
        {
            LSEQ zone_layers = zone->GetLayerSet().Seq();

            for( PCB_LAYER_ID layer : zone_layers )
            {
                elements[layer][zone->GetNetCode()].push_back( zone );
            }
        } );

    for( BOARD_ITEM* item : m_board->Drawings() )
    {
        if( BOARD_CONNECTED_ITEM* conn_it = dynamic_cast<BOARD_CONNECTED_ITEM*>( item ) )
            elements[conn_it->GetLayer()][conn_it->GetNetCode()].push_back( conn_it );
        else
            elements[item->GetLayer()][0].push_back( item );
    }

    for( FOOTPRINT* it_fp : m_board->Footprints() )
    {
        std::unique_ptr<FOOTPRINT> fp( static_cast<FOOTPRINT*>( it_fp->Clone() ) );

        if( fp->GetLayer() != F_Cu )
            fp->Flip( fp->GetPosition(), false );

        for( PCB_FIELD* field : fp->GetFields() )
            elements[field->GetLayer()][0].push_back( field );

        for( BOARD_ITEM* item : fp->GraphicalItems() )
            elements[item->GetLayer()][0].push_back( item );

        for( PAD* pad : fp->Pads() )
        {
            LSEQ pad_layers = pad->GetLayerSet().Seq();

            for( PCB_LAYER_ID layer : pad_layers )
            {
                if( fp->IsFlipped() )
                    layer = FlipLayer( layer );

                if( pad->FlashLayer( layer ) )
                    elements[layer][pad->GetNetCode()].push_back( pad );
            }
        }

        footprints.push_back( std::move( fp ) );
    }

    for( const auto& [layerID, layerName] : m_plugin->GetLayerNameList() )
    {
        // if( m_progress_reporter )
        //     m_progress_reporter->SetMaxProgress( nets.GetNetCount() * layers.size() );

        // m_layer_name_map.emplace( layer, m_board->GetLayerName( layer ) );

        std::shared_ptr<ODB_LAYER_ENTITY> layer_entity_ptr = std::make_shared<ODB_LAYER_ENTITY>(
             m_board, m_plugin, elements[layerID], layerID, layerName );

        layer_entity_ptr->InitEntityData();

        m_layerEntityMap.emplace( layerName, layer_entity_ptr );
    }
}


