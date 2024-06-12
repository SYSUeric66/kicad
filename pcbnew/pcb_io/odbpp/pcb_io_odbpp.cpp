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

#include <string_utf8_map.h>
#include "pcb_io_odbpp.h"
#include "odb_util.h"
#include "odb_attribute.h"

#include "odb_defines.h"
#include "odb_feature.h"
#include "odb_entity.h"
#include "wx/log.h"


PCB_IO_ODBPP::~PCB_IO_ODBPP()
{
    ClearLoadedFootprints();
}


void PCB_IO_ODBPP::ClearLoadedFootprints()
{
    m_loaded_footprints.clear();
}


bool PCB_IO_ODBPP::CreateEntity()
{
    Make<ODB_FONTS_ENTITY>();
    Make<ODB_INPUT_ENTITY>();
    Make<ODB_MATRIX_ENTITY>( m_board, this );
    Make<ODB_STEP_ENTITY>( m_board, this );
    std::vector<wxString> misc_setting = { m_units_str };
    Make<ODB_MISC_ENTITY>( misc_setting );
    Make<ODB_SYMBOLS_ENTITY>();
    Make<ODB_USER_ENTITY>();
    Make<ODB_WHEELS_ENTITY>();
    return true;
}        

bool PCB_IO_ODBPP::GenerateFiles( ODB_TREE_WRITER &writer )
{
    for( const auto entity : m_entities )
    {
        if ( !entity->CreateDirectiryTree( writer ) )
        {
            throw std::runtime_error( "Failed in create directiry tree process" );
            return false;
        }

        if( !entity->GenerateFiles( writer ) )
        {
            throw std::runtime_error( "Failed in generate files process" );
            return false;
        }
    }
    return true;

}
bool PCB_IO_ODBPP::ExportODB( const wxString& aFileName )
{
    try
    {
        std::shared_ptr<ODB_TREE_WRITER> writer =
             std::make_shared<ODB_TREE_WRITER>( aFileName, "odb" );
        writer->SetRootPath( writer->GetCurrentPath() );

        if( !CreateEntity() )
        {
            return false;
        }

        InitEntityData();

        if( !GenerateFiles( *writer ) )
        {
            return false;
        }

        return true;
    }
    catch(const std::exception& e)
    {
        wxLogError( "Exception in ODB++ ExportODB process: %s", e.what() );
        std::cerr << e.what() << std::endl;
        return false;
    }

}


void PCB_IO_ODBPP::InitEntityData()
{
    for( auto const& entity : m_entities )
    {
        entity->InitEntityData();
    }

}

std::vector<FOOTPRINT*> PCB_IO_ODBPP::GetImportedCachedLibraryFootprints()
{
    std::vector<FOOTPRINT*> retval;
    retval.reserve( m_loaded_footprints.size() );

    for( const auto& fp : m_loaded_footprints )
    {
        retval.push_back( static_cast<FOOTPRINT*>( fp->Clone() ) );
    }

    return retval;
}



void PCB_IO_ODBPP::SaveBoard( const wxString& aFileName, BOARD* aBoard,
                                const STRING_UTF8_MAP* aProperties )
{
    m_board = aBoard;
    m_units_str = "MM";
    m_ODBScale = 1.0 / PCB_IU_PER_MM;
    m_sigfig = 4;

    if( auto it = aProperties->find( "units" ); it != aProperties->end() )
    {
        if( it->second == "inch" )
        {
            m_units_str = "INCH";
            m_ODBScale = 25.4 / PCB_IU_PER_MM;
        }
    }

    if( auto it = aProperties->find( "sigfig" ); it != aProperties->end() )
        m_sigfig = std::stoi( it->second );
    // m_progress_reporter = aProgressReporter;

    // if( m_progress_reporter )
    //     {
    //         m_progress_reporter->Report( wxString::Format( _( "Exporting Entity %s, Net %s" ),
    //                                                         m_board->GetLayerName( layer ),
    //                                                         net->GetNetname() ) );
    //         m_progress_reporter->AdvanceProgress();
    //     }

    ExportODB( aFileName );

    // if( m_progress_reporter )
    // {
    //     m_progress_reporter->SetNumPhases( 7 );
    //     m_progress_reporter->BeginPhase( 1 );
    //     m_progress_reporter->Report( _( "Generating logistic section" ) );
    // }

    // generateLogisticSection();
    // generateHistorySection();

    // wxXmlNode* ecad_node = generateEcadSection();
    // generateBOMSection( ecad_node );
    // generateAvlSection();

    // if( m_progress_reporter )
    // {
    //     m_progress_reporter->AdvancePhase( _( "Saving file" ) );
    // }

    // wxFileOutputStreamWithProgress out_stream( aFileName );

    // double written_bytes = 0.0;
    // double last_yield = 0.0;

    // This is a rough estimation of the size of the spaces in the file
    // We just need to total to be slightly larger than the value of the
    // progress bar, so accurately counting spaces is not terribly important
    // m_total_bytes += m_total_bytes / 10;

    // auto update_progress = [&]( size_t aBytes )
    // {
    //     written_bytes += aBytes;
    //     double percent = written_bytes / static_cast<double>( m_total_bytes );

    //     if( m_progress_reporter )
    //     {
    //         // Only update every percent
    //         if( last_yield + 0.01 < percent )
    //         {
    //             last_yield = percent;
    //             m_progress_reporter->SetCurrentProgress( percent );
    //         }
    //     }
    // };

    // out_stream.SetProgressCallback( update_progress );

    // if( !m_xml_doc->Save( out_stream ) )
    // {
    //     wxLogError( _( "Failed to save file to buffer" ) );
    //     return;
    // }

    // size_t size = out_stream.GetSize();

}
