#include "odb_util.h"
#include <wx/chartype.h>
#include <wx/dir.h>
#include "idf_helpers.h"


void ODB_TREE_WRITER::CreateEntityDirectory( const wxString& aPareDir,
                                             const wxString& aSubDir = wxEmptyString )
{
    wxFileName path;

    path.AssignDir( aPareDir );
    path.AppendDir( aSubDir );

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

void ODB_FILE_WRITER::CreateFile( const wxString& aFileName )
{
    wxString filename = m_treeWriter.GetCurrentPath()
                        + wxFileName::GetPathSeparator()
                        + aFileName;
    
    if( filename.IsEmpty() )
        return;
    
    wxFileName fn( filename );

    wxString abspath = fn.GetAbsolutePath();
    
    if( !wxDir::Exists( abspath ) )
    {
        if( !wxDir::Make( abspath ) )
            throw( std::runtime_error( "Could not create directory" + abspath ) );
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
    
    // return m_ostream;
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
