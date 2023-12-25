/**
* This program source code file is part of KiCad, a free EDA CAD application.
*
* Copyright (C) 2023 SYSUeric66 <jzzhuang666@gmail.com>
* Copyright (C) 2023 KiCad Developers, see AUTHORS.txt for contributors.
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

#include "odb_plugin.h"

#include <base_units.h>
#include <board.h>
#include <board_design_settings.h>
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


ODB_PLUGIN::~ODB_PLUGIN()
{
    clearLoadedFootprints();
}


void ODB_PLUGIN::clearLoadedFootprints()
{
    for( FOOTPRINT* fp : m_loaded_footprints )
    {
        delete fp;
    }

    m_loaded_footprints.clear();
}


const wxString ODB_PLUGIN::PluginName() const
{
    return wxT( "ODB++" );
}

void ODB_PLUGIN::SaveBoard( const wxString& aFileName, BOARD* aBoard,
                                const STRING_UTF8_MAP* aProperties,
                                PROGRESS_REPORTER*     aProgressReporter )
{
    m_board = aBoard;
    m_units_str = "MILLIMETER";
    m_scale = 1.0 / PCB_IU_PER_MM;
    m_sigfig = 4;
    m_progress_reporter = aProgressReporter;

    if( auto it = aProperties->find( "units" ); it != aProperties->end() )
    {
        if( it->second == "inch" )
        {
            m_units_str = "INCH";
            m_scale = 25.4 / PCB_IU_PER_MM;
        }
    }

    if( auto it = aProperties->find( "sigfig" ); it != aProperties->end() )
        m_sigfig = std::stoi( it->second );

    if( auto it = aProperties->find( "version" ); it != aProperties->end() )
        m_version = it->second.c_str()[0];

    if( auto it = aProperties->find( "OEMRef" ); it != aProperties->end() )
        m_OEMRef = it->second;

    if( auto it = aProperties->find( "mpn" ); it != aProperties->end() )
        m_mpn = it->second;

    if( auto it = aProperties->find( "mfg" ); it != aProperties->end() )
        m_mfg = it->second;

    if( auto it = aProperties->find( "dist" ); it != aProperties->end() )
        m_dist = it->second;

    if( auto it = aProperties->find( "distpn" ); it != aProperties->end() )
        m_distpn = it->second;

    if( m_version == 'B' )
    {
        for( char c = 'a'; c <= 'z'; ++c )
            m_acceptable_chars.insert( c );

        for( char c = 'A'; c <= 'Z'; ++c )
            m_acceptable_chars.insert( c );

        for( char c = '0'; c <= '9'; ++c )
            m_acceptable_chars.insert( c );

        // Add special characters
        std::string specialChars = "_\\-.+><";

        for( char c : specialChars )
            m_acceptable_chars.insert( c );
    }

    m_xml_doc = new wxXmlDocument();
    m_xml_root = generateXmlHeader();

    generateContentSection();

    if( m_progress_reporter )
    {
        m_progress_reporter->SetNumPhases( 7 );
        m_progress_reporter->BeginPhase( 1 );
        m_progress_reporter->Report( _( "Generating logistic section" ) );
    }

    generateLogisticSection();
    generateHistorySection();

    wxXmlNode* ecad_node = generateEcadSection();
    generateBOMSection( ecad_node );
    generateAvlSection();

    if( m_progress_reporter )
    {
        m_progress_reporter->AdvancePhase( _( "Saving file" ) );
    }

    wxFileOutputStreamWithProgress out_stream( aFileName );
    double written_bytes = 0.0;
    double last_yield = 0.0;

    // This is a rough estimation of the size of the spaces in the file
    // We just need to total to be slightly larger than the value of the
    // progress bar, so accurately counting spaces is not terribly important
    m_total_bytes += m_total_bytes / 10;

    auto update_progress = [&]( size_t aBytes )
    {
        written_bytes += aBytes;
        double percent = written_bytes / static_cast<double>( m_total_bytes );

        if( m_progress_reporter )
        {
            // Only update every percent
            if( last_yield + 0.01 < percent )
            {
                last_yield = percent;
                m_progress_reporter->SetCurrentProgress( percent );
            }
        }
    };

    out_stream.SetProgressCallback( update_progress );

    if( !m_xml_doc->Save( out_stream ) )
    {
        wxLogError( _( "Failed to save file to buffer" ) );
        return;
    }

    size_t size = out_stream.GetSize();

}
