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

#include <string>
#include <algorithm>
#include <locale>
#include "odb_util.h"
#include <wx/chartype.h>
#include <wx/dir.h>
#include "idf_helpers.h"
#include "odb_defines.h"

namespace ODB
{

    wxString GenODBString( const wxString& aStr )
    {
        wxString str;

        for( size_t ii = 0; ii < aStr.Len(); ++ii )
        {
            // Rule: we can only use the standard ASCII, control excluded
            wxUniChar ch = aStr[ii];

            if( ch > 126 || !std::isgraph( static_cast<unsigned char>( ch ) ) )
                ch = '?';

            str += ch;
        }

        // Rule: only uppercase
        str.MakeUpper();

        return str;
    }

    wxString GenLegalNetName( const wxString& aStr )
    {
        std::string str = aStr.ToStdString();
        wxString out;
        out.reserve( str.size() );

        for( auto c : str )
        {
            if( ( c >= 33 && c <= 126 ) && c != ';' )
            {
                out.append( 1, c );
            }
            else
            {
                out.append( 1, '_' ); // Replace invalid characters with underscore
            }
        }

        return out;
    }


    // The names of these ODB++ entities must comply with
    // the rules for legal entity names: 
    // product, model, step, layer, symbol, and attribute.
    wxString GenLegalEntityName( const wxString& aStr )
    {
        std::string str = aStr.ToStdString();
        wxString out;
        out.reserve( str.size() );
        
        for( auto c : str )
        {
            if( isalpha( c ) )
                c = tolower( c );
            else if( isdigit( c ) || c == '-' || c == '_' || c == '+' || c == '.' )
                ;
            else
                c = '_';

            out.append(1, c);
        }

        if( out.length() > 64 )
        {
            out.Truncate( 64 );
        }

        while( !out.IsEmpty() && ( out[0] == '.' || out[0] == '-' || out[0] == '+' ) )
        {
            out.erase( 0, 1 );
        }

        while( !out.IsEmpty() && out.Last() == '.' )
        {
            out.RemoveLast();
        }

        return out;
    }


    wxString Float2StrVal( double aVal )
    {
        // We don't want to output -0.0 as this value is just 0 for fabs
        if( aVal == -0.0 )
            aVal = 0.0;

        wxString str = wxString::FromCDouble( aVal, 4/*m_sigfig*/ );

        // Remove all but the last trailing zeros from str
        while( str.EndsWith( wxT( "00" ) ) )
            str.RemoveLast();

        return str;
    }

    std::pair<wxString, wxString> AddXY( const VECTOR2I& aVec )
    {
        std::pair<wxString, wxString> xy = std::pair<wxString, wxString>(
                                     Float2StrVal( m_ODBScale * aVec.x ),
                                     Float2StrVal( -m_ODBScale * aVec.y ) );

        return xy;
    }

    VECTOR2I GetShapePosition( const PCB_SHAPE& aShape )
    {
        VECTOR2D pos{};

        switch( aShape.GetShape() )
        {
        // Rectangles in KiCad are mapped by their corner while ODBPP uses the center
        case SHAPE_T::RECTANGLE:
            pos = aShape.GetPosition()
                + VECTOR2I( aShape.GetRectangleWidth() / 2.0, aShape.GetRectangleHeight() / 2.0 );
            break;
        // Both KiCad and ODBPP use the center of the circle
        case SHAPE_T::CIRCLE:
        // KiCad uses the exact points on the board
        case SHAPE_T::POLY:
        case SHAPE_T::BEZIER:
        case SHAPE_T::SEGMENT:
        case SHAPE_T::ARC:
            pos = aShape.GetPosition();
            break;
        }

        return pos;
    }
}



void ODB_TREE_WRITER::CreateEntityDirectory( const wxString& aPareDir,
                                             const wxString& aSubDir /*= wxEmptyString*/ )
{
    wxFileName path = wxFileName::DirName( aPareDir );

    wxArrayString subDirs = wxFileName::DirName( aSubDir.Lower() ).GetDirs();

    for( size_t i = 0; i < subDirs.GetCount(); i++ )
        path.AppendDir( subDirs[i] );

    if( !path.DirExists() )
    {
        if( !path.Mkdir( wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL ) )
        {
            throw( std::runtime_error( "Could not create directory" + path.GetPath() ) );
        }
    }

    m_currentPath = path.GetPath();
}

ODB_FILE_WRITER::ODB_FILE_WRITER( ODB_TREE_WRITER& aTreeWriter,
                 const wxString& aFileName ) : m_treeWriter(aTreeWriter)
{
    CreateFile( aFileName );
}

void ODB_FILE_WRITER::CreateFile( const wxString& aFileName )
{
    if( aFileName.IsEmpty() || m_treeWriter.GetCurrentPath().IsEmpty() )
        return;
    
    wxFileName fn;
    fn.SetPath( m_treeWriter.GetCurrentPath() );
    fn.SetFullName( aFileName );
    
    wxString dirPath = fn.GetPath();

    if( !wxDir::Exists( dirPath ) )
    {
        if( !wxDir::Make( dirPath ) )
            throw( std::runtime_error( "Could not create directory" + dirPath ) );
    }
    
    if( !fn.IsDirWritable() || ( fn.Exists() && !fn.IsFileWritable() ) )
        return;

    if( m_ostream.is_open() )
        m_ostream.close();

    m_ostream.open( TO_UTF8( fn.GetFullPath() ),                                
                std::ios_base::out | std::ios_base::trunc | std::ios_base::binary );

    m_ostream.imbue( std::locale::classic() );

    if ( !m_ostream.is_open() || !m_ostream.good() )
        throw std::runtime_error( "Failed to open file: " + fn.GetFullPath() );
    
}

bool ODB_FILE_WRITER::CloseFile()
{
    if( m_ostream.is_open() )
    {
        m_ostream.close();

        if( !m_ostream.good() )
        {
            throw std::runtime_error( "close file failed");
            return false;
        }
    }

    return true;
}



void ODB_TEXT_WRITER::write_line(const std::string &var, int value)
{
    write_indent();
    m_ostream << var << "=" << value << std::endl;
}

void ODB_TEXT_WRITER::write_line(const wxString &var, const wxString &value)
{
    write_indent();
    m_ostream << var << "=" << value << std::endl;
}


void ODB_TEXT_WRITER::write_indent()
{
    if (in_array)
        m_ostream << "    ";
}

void ODB_TEXT_WRITER::begin_array(const std::string &a)
{
    if (in_array)
        throw std::runtime_error("already in array");
    in_array = true;
    m_ostream << a << " {" << std::endl;
}

void ODB_TEXT_WRITER::end_array()
{
    if (!in_array)
        throw std::runtime_error("not in array");
    in_array = false;
    m_ostream << "}" << std::endl << std::endl;
}

ODB_TEXT_WRITER::ArrayProxy::ArrayProxy(ODB_TEXT_WRITER &wr, const std::string &a) : writer(wr)
{
    writer.begin_array(a);
}

ODB_TEXT_WRITER::ArrayProxy::~ArrayProxy()
{
    writer.end_array();
}

ODB_DRILL_TOOLS::ODB_DRILL_TOOLS( const wxString& aUnits,
        const wxString& aThickness, const wxString& aUserParams )
         : m_units( aUnits ), m_thickness( aThickness ), m_userParams( aUserParams )
{
}

bool ODB_DRILL_TOOLS::GenerateFile( std::ostream& aStream )
{   
    ODB_TEXT_WRITER twriter( aStream );

    twriter.write_line( "UNITS", m_units );
    twriter.write_line( "THICKNESS", m_thickness );
    twriter.write_line( "USER_PARAMS", m_userParams );

    for ( const auto& tool : m_tools )
    {
        const auto array_proxy = twriter.make_array_proxy( "TOOLS" );
        twriter.write_line( "NUM", tool.m_num );
        twriter.write_line( "TYPE", tool.m_type );
        twriter.write_line( "TYPE2", tool.m_type2 );
        twriter.write_line( "MIN_TOL", tool.m_minTol );
        twriter.write_line( "MAX_TOL", tool.m_maxTol );
        twriter.write_line( "BIT", tool.m_bit );
        twriter.write_line( "FINISH_SIZE", tool.m_finishSize );
        twriter.write_line( "DRILL_SIZE", tool.m_drillSize );
    }

    return true;
}










// TreeWriter::FileProxy::FileProxy(TreeWriter &wr, const fs::path &p) : writer(wr), stream(writer.create_file_internal(p))
// {
// }

// TreeWriter::FileProxy::~FileProxy()
// {
//     writer.close_file();
// }

// TreeWriterPrefixed::TreeWriterPrefixed(TreeWriter &pa, const fs::path &pr) : parent(pa), prefix(pr)
// {
// }

// std::ostream &TreeWriterPrefixed::create_file_internal(const fs::path &path)
// {
//     return parent.create_file_internal(prefix / path);
// }

// void TreeWriterPrefixed::close_file()
// {
//     parent.close_file();
// }



// TreeWriterFS::TreeWriterFS(const fs::path &path) : base_path(path)
// {
// }

// std::ostream &TreeWriterFS::create_file_internal(const fs::path &path)
// {
//     if (created_files.count(path))
//         throw std::runtime_error(path.generic_u8string() + " already exists");

//     if (ofstream.is_open())
//         throw std::runtime_error("file is already open");

//     const auto abs = base_path / path;
//     fs::create_directories(abs.parent_path());
//     ofstream.open(abs, std::ios_base::out | std::ios_base::binary);
//     ofstream.imbue(std::locale::classic());
//     if (!ofstream.is_open())
//         throw std::runtime_error(abs.u8string() + " not opened");

//     created_files.insert(path);

//     return ofstream;
// }

// void TreeWriterFS::close_file()
// {
//     if (!ofstream.is_open())
//         throw std::runtime_error("no open file");
//     ofstream.close();
// }
