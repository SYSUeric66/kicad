
#ifndef _ODB_COMPONENT_H_
#define _ODB_COMPONENT_H_

#include "odb_util.h"
#include <list>
#include <wx/string.h>



class COMPONENTS_MANAGER : public AttributeProvider
{
public:
    COMPONENTS_MANAGER( BOARD* aBoard ) {}

    virtual ~COMPONENTS_MANAGER()
    {
        m_compList.clear();
    }

    std::list<ODB_COMPONENT> m_compList;

};

class ODB_COMPONENT : public RecordWithAttributes 
{
    ODB_COMPONENT(unsigned int i, unsigned int r) : m_index(i), m_pkg_ref(r)
    {
    }
    const unsigned int m_index;
    unsigned int m_pkg_ref;
    // Placement placement;

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

        // Placement placement;
        unsigned int net_num = 0;
        unsigned int subnet_num = 0;
        std::string toeprint_name = 0;

        void write(std::ostream &ost) const;
    };


    std::list<Toeprint> toeprints;

    void write(std::ostream &ost) const;
};


#endif // _ODB_COMPONENT_H_