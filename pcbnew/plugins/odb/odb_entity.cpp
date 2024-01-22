#include "odb_entity.h"
#include "odb_defines.h"
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

#include "odb_util.h"

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

bool ODB_FONTS_ENTITY::GenerateFiles( ODB_TREE_WRITER& writer )
{

}


ODB_MISC_ENTITY::ODB_MISC_ENTITY( BOARD* aBoard, const std::vector<wxString>& aValue )
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
    bool ret = true;
    ret = ret && GenerateInfoFile( writer );
    return ret;
}

void ODB_MATRIX_ENTITY::AddLayer( const wxString &aLayerName )
{
    m_matrixLayers.emplace_back( m_row++, aLayerName );
}

void ODB_MATRIX_ENTITY::AddStep( const wxString &aStepName )
{
    m_matrixSteps.emplace( aStepName, m_col++ );
}


void ODB_MATRIX_ENTITY::InitEntityData()
{
    wxFileName fn( m_board->GetFileName() );
    AddStep( fn.GetName() );

    LSEQ layer_seq = m_board->GetEnabledLayers().Seq();

    for( PCB_LAYER_ID layer : layer_seq )
    {
        AddLayer( m_board->GetLayerName( layer ) );
    }
}



bool ODB_MATRIX_ENTITY::GenerateFiles( ODB_TREE_WRITER& writer )
{
    bool ret = true;
    ret = ret && GenerateMatrixFile( writer );
    return ret;
}
bool ODB_MATRIX_ENTITY::GenerateMatrixFile( ODB_TREE_WRITER& writer )
{
    auto fileproxy = writer.CreateFileProxy( "matrix" );
    
    ODB_TEXT_WRITER twriter( fileproxy.GetStream() );
    for ( const auto &[step_name, column] : m_matrixSteps ) {
        const auto a = twriter.make_array_proxy( "STEP" );
        twriter.write_line( "COL", column );
        twriter.write_line( "NAME", step_name );
    }
    for ( const auto &layer : m_matrixLayers ) {
        const auto a = twriter.make_array_proxy( "LAYER" );
        twriter.write_line( "ROW", layer.m_rowNumber );
        twriter.write_line_enum( "CONTEXT", layer.m_context );
        twriter.write_line_enum( "TYPE", layer.m_type);
        if (layer.m_addType.has_value())
            twriter.write_line_enum("ADD_TYPE", layer.m_addType.value());
        twriter.write_line("NAME", layer.m_layerName);
        twriter.write_line_enum("POLARITY", layer.m_polarity);
        if (layer.m_span) {
            twriter.write_line("START_NAME", layer.m_span->start);
            twriter.write_line("END_NAME", layer.m_span->end);
        }
    }
}

ODB_LAYER_ENTITY::ODB_LAYER_ENTITY( BOARD* aBoard,
     std::map<int, std::vector<BOARD_ITEM*>>& aMap,
     const PCB_LAYER_ID& aLayerID )
      : ODB_ENTITY_BASE( aBoard ),
       m_layerItems( aMap ), m_layerID( aLayerID )
{
    m_featuresMgr = std::make_unique<FEATURES_MANAGER>( aBoard );
    // m_shape_std_node = appendNode( m_layer_node, "ShapeStandard" );
    m_profile = std::make_unique<FEATURES_MANAGER>( aBoard );

}

void ODB_LAYER_ENTITY::InitEntityData()
{
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
        // generateLayerSetNet( layerNode, layer, vec );
    }
}
void ODB_STEP_ENTITY::InitEntityData()
{
    InitLayerEntityData();
    
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

bool ODB_STEP_ENTITY::GenerateLayerFiles( ODB_TREE_WRITER& writer )
{
    wxString layers_root = writer.GetCurrentPath();
    for( auto& [layerName, layerEntity] : m_layerEntityMap )
    {
        writer.CreateEntityDirectory( layers_root, layerName );

        layerEntity->GenerateFiles( writer );

    }
    
}

ODB_STEP_ENTITY::ODB_STEP_ENTITY( BOARD* aBoard ) : ODB_ENTITY_BASE( aBoard )
{
        // m_stepHdr = STEP_HDR();
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

bool ODB_STEP_ENTITY::InitLayerEntityData()
{
    LSEQ layers = m_board->GetEnabledLayers().Seq();
    const NETINFO_LIST& nets = m_board->GetNetInfo();
    std::vector<std::unique_ptr<FOOTPRINT>> footprints;

    // To avoid the overhead of repeatedly cycling through the layers and nets,
    // we pre-sort the board items into a map of layer -> net -> items
    std::map<PCB_LAYER_ID, std::map<int, std::vector<BOARD_ITEM*>>> elements;

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
        fp->SetParentGroup( nullptr );
        fp->SetPosition( { 0, 0 } );

        if( fp->GetLayer() != F_Cu )
            fp->Flip( fp->GetPosition(), false );

        fp->SetOrientation( EDA_ANGLE::m_Angle0 );

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

    for( PCB_LAYER_ID layer : layers )
    {
        // if( m_progress_reporter )
        //     m_progress_reporter->SetMaxProgress( nets.GetNetCount() * layers.size() );

        // m_layer_name_map.emplace( layer, m_board->GetLayerName( layer ) );

        std::shared_ptr<ODB_LAYER_ENTITY> layer_entity_ptr = 
                std::make_shared<ODB_LAYER_ENTITY>( m_board, elements[layer], layer );
        
        layer_entity_ptr->InitEntityData();
        m_layerEntityMap.emplace( m_board->GetLayerName( layer ), layer_entity_ptr );
    }
}


bool ODB_LAYER_ENTITY::GenerateFiles( ODB_TREE_WRITER &writer )
{

    if ( !GenAttrList() )
    {
        /* code */
    }
    
    if ( !GenComponents())
    {
        /* code */
    }
    
    if ( !GenFeatures( writer ) )
    {
        /* code */
    }

    // if ( !GenProfiles() )
    // {
    //     /* code */
    // }
    
    if ( !GenTools() )
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