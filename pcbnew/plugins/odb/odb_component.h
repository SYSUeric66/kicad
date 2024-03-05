
#ifndef _ODB_COMPONENT_H_
#define _ODB_COMPONENT_H_

#include "odb_util.h"
#include <list>
#include <wx/string.h>
#include "odb_attribute.h"
#include "odb_eda_data.h"

class ODB_COMPONENT;
class COMPONENTS_MANAGER : public AttributeProvider
{
public:
    COMPONENTS_MANAGER() = default;

    virtual ~COMPONENTS_MANAGER()
    {
        m_compList.clear();
    }

    ODB_COMPONENT& AddComponent( FOOTPRINT* aFp, EDAData& eda_data );

    void write(std::ostream &ost) const;
private:
    std::list<ODB_COMPONENT> m_compList;

};


class ODB_COMPONENT : public ATTR_RECORD_WRITER 
{
public:
    ODB_COMPONENT(unsigned int i, unsigned int r) : m_index(i), m_pkg_ref(r)
    {
    }
    const unsigned int m_index;
    unsigned int m_pkg_ref;
    std::pair<wxString, wxString> m_center;
    wxString m_rot = wxT( "0" );
    wxString m_mirror = wxT( "N" );

    wxString m_comp_name;
    wxString m_part_name;

    std::vector<std::pair<wxString, wxString>> m_prp;   // !< Component Property Record

    struct Toeprint
    {
    public:
        Toeprint(const EDAData::Pin &pin) : pin_num(pin.index), toeprint_name(pin.name)
        {
        }

        unsigned int pin_num;

        std::pair<wxString, wxString> m_center;
        wxString m_rot = wxT( "0" );
        wxString m_mirror = wxT( "N" );
        unsigned int net_num = 0;
        unsigned int subnet_num = 0;
        wxString toeprint_name;

        void write(std::ostream &ost) const;
    };


    std::list<Toeprint> toeprints;

    void write(std::ostream &ost) const;
};


#endif // _ODB_COMPONENT_H_