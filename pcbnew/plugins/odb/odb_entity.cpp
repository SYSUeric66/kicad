#include "odb_entity.h"
#include "build_version.h"
#include "odb_defines.h"



ODB_ENTITY_BASE::ODB_ENTITY_BASE()
{
}

ODB_ENTITY_BASE::~ODB_ENTITY_BASE()
{
}

ODB_MISC_ENTITY::ODB_MISC_ENTITY( const std::vector<wxString>& aValue )
{
    m_info = 
    {
        { wxS( JOB_NAME ), wxS( "job" ) },
        { wxS( UNITS ), wxS( "MM" ) },
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
        m_info[ wxS( JOB_NAME ) ] = aValue.at( 0 );
    }

    if( !aValue.at( 1 ).IsEmpty() )
    {
        m_info[ wxS( UNITS ) ] = aValue.at( 1 );
    }
}

