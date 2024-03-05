
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

    // wxString GenString( const wxString& aStr )
    // {
    //     wxString tmp = aStr.ToAscii();
    //     std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    //     std::wstring wstr = converter.from_bytes( tmp.ToStdString() );

    //     wxString str;

    //     std::transform( wstr.begin(), wstr.end(), std::back_inserter(str),
    //                []( wchar_t ch )
    //                {
    //                     if( ch == ';' || isspace( ch ) )
    //                         ch = '_';

    //                     return  ch;
    //                } );

    //     return str;
    // }


    // The names of these ODB++ entities must comply with
    // the rules for legal entity names: 
    // product, model, step, layer, symbol, and attribute.
    std::string GenLegalEntityName( const wxString& aStr )
    {
        std::string s = aStr.ToStdString();
        std::locale loc;
        std::string out;
        out.reserve(s.size());

        std::transform(s.begin(), s.end(), std::back_inserter(out),
            [&loc](unsigned char c) -> unsigned char
            {
                if (std::isalnum(c, loc) || (c == '-') || (c == '_') || (c == '+'))
                {
                    return c;
                }
                return '_';
            } );

        std::transform(out.begin(), out.end(), out.begin(),
            [&loc](unsigned char c) -> unsigned char
            {
                if (std::isalpha(c, loc)) {
                    return std::tolower(c, loc);
                }
                return c;
            } );

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
    wxFileName path;

    path.AssignDir( aPareDir );
    path.AppendDir( aSubDir.Lower() );

    if( !path.DirExists() )
    {
        if( !wxMkdir( path.GetPath() ) )
        {
            throw( std::runtime_error( "Could not create directory" + path.GetPath() ) );
        }
        else
        {
            m_currentPath = path.GetPath();
        }
    }

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

    if ( m_ostream.is_open() )
        throw std::runtime_error( fn.GetFullPath() + " is already open" );

    m_ostream.open( TO_UTF8( fn.GetFullPath() ),                                
                std::ios_base::out | std::ios_base::trunc | std::ios_base::binary );

    m_ostream.imbue( std::locale::classic() );

    if ( !m_ostream.is_open() || !m_ostream.good() )
        throw std::runtime_error( fn.GetFullPath() + " open failed");
    
}

bool ODB_FILE_WRITER::CloseFile()
{
    m_ostream.close();

    if ( !m_ostream.good() )
    {
        throw std::runtime_error( "close file failed");
        return false;
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
