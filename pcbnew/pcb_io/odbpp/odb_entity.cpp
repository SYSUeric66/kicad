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
        // std::shared_ptr<FOOTPRINT> fp( static_cast<FOOTPRINT*>( it_fp->Clone() ) );

        if( fp->IsFlipped() )
        {
            // fp->Flip( fp->GetPosition(), false );
            m_hasBotComp = true;
        }
            
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
    MATRIX_LAYER mLayer( m_row++, "COMP_+_TOP" );
    mLayer.m_type = ODB_TYPE::COMPONENT;
    mLayer.m_context = ODB_CONTEXT::BOARD;
    
    m_matrixLayers.push_back( mLayer );
    m_plugin->GetLayerNameList().emplace_back( std::make_pair(
                            PCB_LAYER_ID::UNDEFINED_LAYER,
                            mLayer.m_layerName ) );

    if( m_hasBotComp )
    {
        mLayer.m_layerName = "COMP_+_BOT";
        m_matrixLayers.push_back( mLayer );
        m_plugin->GetLayerNameList().emplace_back( std::make_pair(
                            PCB_LAYER_ID::UNDEFINED_LAYER,
                            mLayer.m_layerName ) );
    }
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
    m_featuresMgr = std::make_unique<FEATURES_MANAGER>( aBoard, aPlugin, aLayerName );
    // m_shape_std_node = appendNode( m_layer_node, "ShapeStandard" );
    // m_profile = std::make_unique<FEATURES_MANAGER>( aBoard );

}

void ODB_LAYER_ENTITY::InitEntityData()
{
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


ODB_COMPONENT& ODB_LAYER_ENTITY::InitComponentData( FOOTPRINT* aFp, EDAData& aEDAData )
{   
    if( m_matrixLayerName == "COMP_+_BOT" )
    {
        if( !m_compBot.has_value() )
        {
            m_compBot.emplace();
        }
        return m_compBot.value().AddComponent( aFp, aEDAData );
    }
    else
    {
        if( !m_compTop.has_value() )
        {
            m_compTop.emplace();
        }

        return m_compTop.value().AddComponent( aFp, aEDAData );
    }
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

        dLayerName.MakeUpper().Replace( wxT( "." ), wxT( "_" ) );

        if( dLayerName == m_matrixLayerName )
        {
            for( BOARD_ITEM* item : vec )
            {
                // for drill tools
                if( item->Type() == PCB_VIA_T )
                {
                    PCB_VIA* via = static_cast<PCB_VIA*>( item );

                    m_tools.value().AddDrillTools(
                        "VIA",
                        ODB::Float2StrVal( m_ODBScale * via->GetDrillValue() ) );

                    // for drill features
                    m_layerItems[via->GetNetCode()].push_back( item );
                }
                else if( item->Type() == PCB_PAD_T )
                {
                    PAD* pad = static_cast<PAD*>( item );

                    m_tools.value().AddDrillTools(
                        pad->GetAttribute() == PAD_ATTRIB::PTH ? "PLATED" : "NON_PLATED",
                        ODB::Float2StrVal( m_ODBScale * pad->GetDrillSizeX() ) );
                    
                    // for drill features
                    m_layerItems[pad->GetNetCode()].push_back( item );
                }

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

    m_tools.emplace( "MM" );
    
    for( const auto& [layer_pair, vec] : slot_holes )
    {
        wxString sLayerName = wxString::Format( "SLOT_%s-%s",
                        m_board->GetLayerName( layer_pair.first ),
                        m_board->GetLayerName( layer_pair.second ) );

        sLayerName.MakeUpper().Replace( wxT( "." ), wxT( "_" ) );

        if( sLayerName == m_matrixLayerName )
        {
            for( PAD* pad : vec )
            {
                //for slot tools
                m_tools.value().AddDrillTools(
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
    MakeLayerEntity();
    InitEdaData();
    // InitNetListData();
    InitLayerEntityData();

}

void ODB_STEP_ENTITY::InitPackage()
{
    for( FOOTPRINT* fp : m_board->Footprints() )
    {
        m_edaData.add_package( fp );
    }
    
}


bool ODB_LAYER_ENTITY::GenerateFiles( ODB_TREE_WRITER &writer )
{
    //TODO
    if ( !GenAttrList() )
    {
        /* code */
    }
    
    if ( m_compTop.has_value() || m_compBot.has_value() )
    {
        GenComponents( writer );
    }
    
    if ( !GenFeatures( writer ) )
    {
        /* code */
    }

    if ( m_tools.has_value() )
    {
        GenTools( writer );
    }
    
    return true;

}

bool ODB_LAYER_ENTITY::GenComponents( ODB_TREE_WRITER &writer )
{
    auto fileproxy = writer.CreateFileProxy( "component" );
    
    if( m_compTop.has_value() )
    {
        m_compTop->write( fileproxy.GetStream() );
    }
    else if( m_compBot.has_value() )
    {
        m_compBot->write( fileproxy.GetStream() );
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

    return m_tools.value().GenerateFile( fileproxy.GetStream() );

}


void ODB_STEP_ENTITY::InitEdaData()
{
    InitPackage();

    // for NET
    const NETINFO_LIST& nets = m_board->GetNetInfo();
    for( const NETINFO_ITEM* net : nets )
    {
        m_edaData.AddNET( net );
    }

    // for CMP
    for( FOOTPRINT* fp : m_board->Footprints() )
    {
        wxString compName = "COMP_+_TOP";
        if( fp->IsFlipped() )
            compName = "COMP_+_BOT";

        auto iter = m_layerEntityMap.find( compName );
        if( iter == m_layerEntityMap.end() )
        {
            wxLogError( _( "Failed to add component data" ) );
            return;
        }

        ODB_COMPONENT& comp = iter->second->InitComponentData( fp, m_edaData );

        const EDAData::Package& eda_pkg = m_edaData.get_package( hash_fp_item( fp, HASH_POS | REL_COORD ) );

        for( PAD* pad : fp->Pads() )
        {
            auto& eda_net = m_edaData.GetNet( pad->GetNetCode() );

            auto& subnet = eda_net.AddSubnet<EDAData::SubnetToeprint>(
                    &m_edaData,
                    fp->IsFlipped() ? EDAData::SubnetToeprint::Side::BOTTOM 
                                    : EDAData::SubnetToeprint::Side::TOP,
                    comp.m_index, comp.toeprints.size() );

            m_plugin->GetPadSubnetMap().emplace( pad, &subnet );
            
            auto &toep = comp.toeprints.emplace_back(
                            eda_pkg.GetEdaPkgPin( hash_fp_item( pad, 0 ) ) );

            toep.net_num = eda_net.index;
            toep.subnet_num = subnet.index;

            toep.m_center = ODB::AddXY( pad->GetCenter() );

            toep.m_rot = ODB::Float2StrVal(
                        pad->GetOrientation().Normalize().AsDegrees() );

            if( pad->IsFlipped() )
                toep.m_mirror = wxT( "M" );
        }
    }

    for( PCB_TRACK* track : m_board->Tracks() )
    {
        auto& eda_net = m_edaData.GetNet( track->GetNetCode() );
        EDAData::Subnet* subnet = nullptr;
        if( track->Type() == PCB_VIA_T )
            subnet = &( eda_net.AddSubnet<EDAData::SubnetVia>( &m_edaData ) );
        else
            subnet = &( eda_net.AddSubnet<EDAData::SubnetTrace>( &m_edaData ) );

        m_plugin->GetViaTraceSubnetMap().emplace( track, subnet );
    }

    for( ZONE* zone : m_board->Zones() )
    {
        for( PCB_LAYER_ID layer : zone->GetLayerSet().Seq() )
        {
            auto& eda_net = m_edaData.GetNet( zone->GetNetCode() );
            auto& subnet = eda_net.AddSubnet<EDAData::SubnetPlane>(
                &m_edaData,
                EDAData::SubnetPlane::FillType::SOLID,
                EDAData::SubnetPlane::CutoutType::CIRCLE,
                0.00 );
            m_plugin->GetPlaneSubnetMap().emplace( std::piecewise_construct,
                    std::forward_as_tuple( layer, zone ),
                    std::forward_as_tuple( &subnet ) );
        }
    }

}


// void ODB_STEP_ENTITY::InitNetListData()
// {

// }

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

    writer.CreateEntityDirectory( step_root, "netlists/cadnet" );
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
        
    // std::map<PCB_LAYER_ID, std::map<int, std::vector<BOARD_ITEM*>>>&
    //             layerElementMap = m_plugin->GetLayerElementsMap();

    m_profile = std::make_unique<FEATURES_MANAGER>( m_board, m_plugin, wxEmptyString );

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

bool ODB_STEP_ENTITY::GenerateEdaFiles( ODB_TREE_WRITER& writer )
{
    auto fileproxy = writer.CreateFileProxy( "data" );
    
    m_edaData.write( fileproxy.GetStream() );
    return true;
}

bool ODB_STEP_ENTITY::GenerateNetlistsFiles( ODB_TREE_WRITER& writer )
{
    auto fileproxy = writer.CreateFileProxy( "netlist" );
    
    m_netlist.Write( fileproxy.GetStream() );
    return true;
}

ODB_STEP_ENTITY::ODB_STEP_ENTITY( BOARD* aBoard, ODB_PLUGIN* aPlugin )
     : ODB_ENTITY_BASE( aBoard, aPlugin )
{
    m_profile = nullptr;
    m_edaData = EDAData();
    m_netlist = ODB_NET_LIST( aBoard );
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

void ODB_STEP_ENTITY::MakeLayerEntity()
{
    LSEQ layers = m_board->GetEnabledLayers().Seq();
    const NETINFO_LIST& nets = m_board->GetNetInfo();
    std::vector<std::shared_ptr<FOOTPRINT>>& footprints =
            m_plugin->GetLoadedFootprintList();

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

    for( FOOTPRINT* fp : m_board->Footprints() )
    {
        // std::shared_ptr<FOOTPRINT> fp( static_cast<FOOTPRINT*>( it_fp->Clone() ) );

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

        // footprints.push_back( std::move( fp ) );
    }

    for( const auto& [layerID, layerName] : m_plugin->GetLayerNameList() )
    {
        // if( m_progress_reporter )
        //     m_progress_reporter->SetMaxProgress( nets.GetNetCount() * layers.size() );

        // m_layer_name_map.emplace( layer, m_board->GetLayerName( layer ) );

        std::shared_ptr<ODB_LAYER_ENTITY> layer_entity_ptr = std::make_shared<ODB_LAYER_ENTITY>(
             m_board, m_plugin, elements[layerID], layerID, layerName );

        // layer_entity_ptr->InitEntityData();

        m_layerEntityMap.emplace( layerName, layer_entity_ptr );
    }
}

void ODB_STEP_ENTITY::InitLayerEntityData()
{
    for( const auto& [layerName, layer_entity_ptr] : m_layerEntityMap )
    {
        // if( m_progress_reporter )
        //     m_progress_reporter->SetMaxProgress( nets.GetNetCount() * layers.size() );

        // m_layer_name_map.emplace( layer, m_board->GetLayerName( layer ) );
        layer_entity_ptr->InitEntityData();
    }
}



