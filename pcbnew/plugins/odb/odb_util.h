#include <map>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <wx/string.h>
#include "pcb_shape.h"
#include <wx/filename.h>
#include "odb_defines.h"

//<! Scale factor from IU to IPC2581 units (mm, micron, in)
static double m_ODBScale = 1.0 / PCB_IU_PER_MM; 

namespace ODB
{
    
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

    std::pair<wxString, wxString>& AddXY( const VECTOR2I& aVec )
    {
        std::pair<wxString, wxString> xy = std::pair<wxString, wxString>(
                                     Float2StrVal( m_ODBScale * aVec.x ),
                                     Float2StrVal( m_ODBScale * aVec.y ) );

        return xy;
    }

    VECTOR2I GetShapePosition( const PCB_SHAPE& aShape )
    {
        VECTOR2D pos{};

        switch( aShape.GetShape() )
        {
        // Rectangles in KiCad are mapped by their corner while IPC2581 uses the center
        case SHAPE_T::RECTANGLE:
            pos = aShape.GetPosition()
                + VECTOR2I( aShape.GetRectangleWidth() / 2.0, aShape.GetRectangleHeight() / 2.0 );
            break;
        // Both KiCad and IPC2581 use the center of the circle
        case SHAPE_T::CIRCLE:
            pos = aShape.GetPosition();
            break;

        // KiCad uses the exact points on the board, so we want the reference location to be 0,0
        case SHAPE_T::POLY:
        case SHAPE_T::BEZIER:
        case SHAPE_T::SEGMENT:
        case SHAPE_T::ARC:
            pos = VECTOR2D( 0, 0 );
            break;
        }

        return VECTOR2I( pos.x, pos.y );
    }

}


class ODB_TREE_WRITER
{
public:
    ODB_TREE_WRITER( const wxString& aDir ) : m_currentPath( aDir )
    {
    }

    ODB_TREE_WRITER( const wxString& aPareDir, const wxString& aSubDir )
    {
        CreateEntityDirectory( aPareDir, aSubDir );
    }

    virtual ~ODB_TREE_WRITER() {}
    
    [[nodiscard]] ODB_FILE_WRITER CreateFileProxy( const wxString& aFileName )
    {
        return ODB_FILE_WRITER( *this, aFileName );
    }

    void CreateEntityDirectory( const wxString& aPareDir,
                          const wxString& aSubDir = wxEmptyString );

    inline const wxString GetCurrentPath() const
    {
        return m_currentPath;
    }

    inline void SetCurrentPath( const wxString& aDir )
    {
        m_currentPath = aDir;
    }

    inline void SetRootPath( const wxString& aDir )
    {
        m_rootPath = aDir;
    }

    inline const wxString GetRootPath() const
    {
        return m_rootPath;
    }


private:
    wxString m_currentPath;
    wxString m_rootPath;
};

class ODB_FILE_WRITER
{
public:
    ODB_FILE_WRITER( ODB_TREE_WRITER& aTreeWriter, const wxString& aFileName )
     : m_treeWriter(aTreeWriter)
    {
        CreateFile( aFileName );
    }

    virtual ~ODB_FILE_WRITER()
    {
        if( m_ostream.is_open() )
            m_ostream.close();
    }

    ODB_FILE_WRITER( ODB_FILE_WRITER && ) = delete;
    ODB_FILE_WRITER &operator=( ODB_FILE_WRITER && ) = delete;

    ODB_FILE_WRITER( ODB_FILE_WRITER const & ) = delete;
    ODB_FILE_WRITER &operator=( ODB_FILE_WRITER const & ) = delete;

    void CreateFile( const wxString& aFileName );
    bool CloseFile();
    inline std::ostream& GetStream() { return m_ostream; }

private:
    ODB_TREE_WRITER& m_treeWriter;
    std::ofstream m_ostream;

};


class ODB_TEXT_WRITER
{
public:
    ODB_TEXT_WRITER( std::ostream& aStream ) : m_ostream( aStream ) {}
    virtual ~ODB_TEXT_WRITER() {}

    // void write_line( const std::string &var, const std::string &value );
    void write_line( const std::string &var, int value );
    void write_line(const wxString &var, const wxString &value);
    template <typename T> void write_line_enum( const std::string &var, const T &value )
    {
        write_line(var, enum_to_string(value));
    }

    class ArrayProxy {
        friend ODB_TEXT_WRITER;

    public:
        ~ArrayProxy();

    private:
        ArrayProxy(ODB_TEXT_WRITER &writer, const std::string &a);

        ODB_TEXT_WRITER &writer;

        ArrayProxy(ArrayProxy &&) = delete;
        ArrayProxy &operator=(ArrayProxy &&) = delete;

        ArrayProxy(ArrayProxy const &) = delete;
        ArrayProxy &operator=(ArrayProxy const &) = delete;
    };

    [[nodiscard]] ArrayProxy make_array_proxy(const std::string &a)
    {
        return ArrayProxy(*this, a);
    }

private:
    void write_indent();
    void begin_array(const std::string &a);
    void end_array();
    std::ostream& m_ostream;
    bool in_array = false;
};


class Once {
public:
    bool operator()()
    {
        if (first) {
            first = false;
            return true;
        }
        return false;
    }

private:
    bool first = true;
};


