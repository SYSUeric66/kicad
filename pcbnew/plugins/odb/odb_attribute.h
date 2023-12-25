

class ODB_ATTRIBUTE
{
public:
	ODB_ATTRIBUTE_BASE();
    virtual ~ODB_ATTRIBUTE_BASE();
};

class SYSTEM_ATTRIBUTE : public ODB_ATTRIBUTE
{
public:
	SYSTEM_ATTRIBUTE();
    virtual ~SYSTEM_ATTRIBUTE();
    // virtual wxTextOutputStream& Generate () = 0;

};

class SYSATTR : public SYSTEM_ATTRIBUTE
{
private:
    enum class PRODUCT_ATTR
    {
        all_eda_layers,

    };
    
public:
    SYSATTR(/* args */);
    virtual ~SYSATTR();
};

class SYSATTR_DFM : public SYSTEM_ATTRIBUTE
{
private:
    /* data */
public:
    SYSATTR_DFM(/* args */);
    virtual ~SYSATTR_DFM();
};

class SYSATTR_FAB : public SYSTEM_ATTRIBUTE
{
private:
    /* data */
public:
    SYSATTR_FAB(/* args */);
    virtual ~SYSATTR_FAB();
};

class SYSATTR_ASSY : public SYSTEM_ATTRIBUTE
{
private:
    /* data */
public:
    SYSATTR_ASSY(/* args */);
    virtual ~SYSATTR_ASSY();
};

class SYSATTR_TEST : public SYSTEM_ATTRIBUTE
{
private:
    /* data */
public:
    SYSATTR_TEST(/* args */);
    virtual ~SYSATTR_TEST();
};

class SYSATTR_GEN : public SYSTEM_ATTRIBUTE
{
private:
    /* data */
public:
    SYSATTR_GEN(/* args */);
    virtual ~SYSATTR_GEN();
};

struct ODB_ATTR_NAME
{
    std::map<std::string, unsigned int> m_names;
};

struct ODB_ATTR_VALUE
{
    std::map<std::string, unsigned int> m_values;
};





class AttributeProvider
{

public:
    template <typename Tr, typename Ta> void add_attribute(Tr &r, Ta v)
    {
        using Tc = typename Tr::template check_type<Ta>;
        static_assert(Tc::value);

        const auto id = get_or_create_attribute_name(attribute::attribute_name<Ta>::name);
        if constexpr (std::is_enum_v<Ta>)
            r.attributes.emplace_back(id, std::to_string(static_cast<int>(v)));
        else
            r.attributes.emplace_back(id, attr_to_string(v));
    }

protected:
    unsigned int get_or_create_attribute_name(const std::string &name);

    void write_attributes(std::ostream &ost, const std::string &prefix = "") const;


private:
    unsigned int get_or_create_attribute_text(const std::string &name);

    static std::string double_to_string(double v, unsigned int n);

    template <typename T, unsigned int n> std::string attr_to_string(attribute::float_attribute<T, n> a)
    {
        return double_to_string(a.value, a.ndigits);
    }

    template <typename T> std::string attr_to_string(attribute::boolean_attribute<T> a)
    {
        return "";
    }

    template <typename T> std::string attr_to_string(attribute::text_attribute<T> a)
    {
        return std::to_string(get_or_create_attribute_text(a.value));
    }


    std::map<std::string, unsigned int> attribute_names;
    std::map<std::string, unsigned int> attribute_texts;
};

class RecordWithAttributes
{

protected:
    void write_attributes(std::ostream &ost) const;

public:
    std::vector<std::pair<unsigned int, std::string>> attributes;
};


struct ODB_ATTRLIST
{
    ODB_ATTRLIST( const wxString &aUnit ) : m_unit( aUnit ) {}
    wxString m_unit;
    std::vector<std::pair<ODB_ATTRIBUTE, wxString>> m_attrlist;
}

struct STEP_HDR
{
    STEP_HDR( const wxString &aUnit ) : m_unit( aUnit ) {}

    static const wxString m_general_fields[] =
    {
        wxS( "X_DATUM" ),
        wxS( "Y_DATUM" ),
        wxS( "ID" ),
        wxS( "X_ORIGIN" ),
        wxS( "Y_ORIGIN" ),
        wxS( "TOP_ACTIVE" ),
        wxS( "BOTTOM_ACTIVE" ),
        wxS( "RIGHT_ACTIVE" ),
        wxS( "LEFT_ACTIVE" ),
        wxS( "AFFECTING_BOM" ),
        wxS( "AFFECTING_BOM_CHANGED" ),

    };

    struct STEP_REPEAT
    {
        static const wxString m_stepRepeat_fields[] =
        {
            wxS( "NAME" ),
            wxS( "X" ),
            wxS( "Y" ),
            wxS( "DX" ),
            wxS( "DY" ),
            wxS( "NX" ),
            wxS( "NY" ),
            wxS( "ANGLE" ),
            wxS( "FLIP" ),
            wxS( "MIRROR" ),

        };

        const wxString m_structuredText_name = wxS( "STEP-REPEAT" );

    };

    std::vector<STEP_REPEAT> m_repeat;

    wxString m_unit;

};

namespace ODB
{   
    struct XY
    {
        XY() : m_x(), m_y() {}
        wxString m_x;
        wxString m_y;
    };
    struct ODB_COMPONENTS;
}


struct ODB_COMPONENTS
{
    ODB_COMPONENTS() {}
    wxString m_unit;


    struct Toeprint
    {
    public:
        Toeprint(const EDAData::Pin &pin) : pin_num(pin.index), toeprint_name(pin.name)
        {
        }

        unsigned int pin_num;

        Placement placement;
        unsigned int net_num = 0;
        unsigned int subnet_num = 0;
        std::string toeprint_name = 0;

        void write(std::ostream &ost) const;
    };

    struct COMPONENT
    {
        COMPONENT(unsigned int i, unsigned int r) : m_index(i), m_pkg_ref(r)
        {
        }
        const unsigned int m_index;
        unsigned int m_pkg_ref;
        Placement placement;

        wxString m_comp_name;
        wxString m_part_name;

        std::vector<std::pair<wxString, wxString>> m_prp;   // !< Component Property Record

        std::list<Toeprint> toeprints;

        void write(std::ostream &ost) const;
    };

    std::list<Component> components;





};