
#ifndef _ATTRIBUTE_PROVIDER_H_
#define _ATTRIBUTE_PROVIDER_H_

#include "odb_util.h"
#include "stroke_params.h"
#include <wx/string.h>


// class ODB_ATTRIBUTE
// {
// public:
// 	ODB_ATTRIBUTE_BASE();
//     virtual ~ODB_ATTRIBUTE_BASE();
// };

// class SYSTEM_ATTRIBUTE : public ODB_ATTRIBUTE
// {
// public:
// 	SYSTEM_ATTRIBUTE();
//     virtual ~SYSTEM_ATTRIBUTE();
//     // virtual wxTextOutputStream& Generate () = 0;

// };

// class SYSATTR : public SYSTEM_ATTRIBUTE
// {
// private:
//     enum class PRODUCT_ATTR
//     {
//         all_eda_layers,

//     };
    
// public:
//     SYSATTR(/* args */);
//     virtual ~SYSATTR();
// };

// class SYSATTR_DFM : public SYSTEM_ATTRIBUTE
// {
// private:
//     /* data */
// public:
//     SYSATTR_DFM(/* args */);
//     virtual ~SYSATTR_DFM();
// };

// class SYSATTR_FAB : public SYSTEM_ATTRIBUTE
// {
// private:
//     /* data */
// public:
//     SYSATTR_FAB(/* args */);
//     virtual ~SYSATTR_FAB();
// };

// class SYSATTR_ASSY : public SYSTEM_ATTRIBUTE
// {
// private:
//     /* data */
// public:
//     SYSATTR_ASSY(/* args */);
//     virtual ~SYSATTR_ASSY();
// };

// class SYSATTR_TEST : public SYSTEM_ATTRIBUTE
// {
// private:
//     /* data */
// public:
//     SYSATTR_TEST(/* args */);
//     virtual ~SYSATTR_TEST();
// };

// class SYSATTR_GEN : public SYSTEM_ATTRIBUTE
// {
// private:
//     /* data */
// public:
//     SYSATTR_GEN(/* args */);
//     virtual ~SYSATTR_GEN();
// };

// struct ODB_ATTR_NAME
// {
//     std::map<std::string, unsigned int> m_names;
// };

// struct ODB_ATTR_VALUE
// {
//     std::map<std::string, unsigned int> m_values;
// };

namespace ODB
{
    template <typename T> struct attribute_name {};

    enum class ATTRTYPE { FLOAT, BOOLEAN, TEXT };

    template <typename T, unsigned int n> struct float_attribute
    {
        double value;
        static constexpr unsigned int ndigits = n;
        static constexpr ATTRTYPE type = ATTRTYPE::FLOAT;
    };

    template <typename T> struct boolean_attribute {
        bool value = true;
        static constexpr ATTRTYPE type = ATTRTYPE::BOOLEAN;
    };

    template <typename T> struct text_attribute {
        text_attribute(const std::string &t) : value(t)
        {
        }
        std::string value;
        static constexpr ATTRTYPE type = ATTRTYPE::TEXT;
    };

    #define ATTR_NAME(n)                                                                                                   \
        template <> struct attribute_name<n> {                                                                             \
            static constexpr const char *name = "." #n;                                                                    \
        };


//     #define MAKE_FLOAT_ATTR(n, nd)                                                                                         \
//         using n = float_attribute<struct n##_t, nd>;                                                                       \
//         ATTR_NAME(n)

//     #define MAKE_TEXT_ATTR(n)                                                                                              \
//         using n = text_attribute<struct n##_t>;                                                                            \
//         ATTR_NAME(n)

//     #define MAKE_BOOLEAN_ATTR(n)                                                                                           \
//         using n = boolean_attribute<struct n##_t>;                                                                         \
//         ATTR_NAME(n)

//     template <typename T> struct is_feature : std::false_type {};
//     template <typename T> struct is_net : std::false_type {};
//     template <typename T> struct is_pkg : std::false_type {};

//     template <class T> inline constexpr bool is_feature_v = is_feature<T>::value;
//     template <class T> inline constexpr bool is_net_v = is_net<T>::value;
//     template <class T> inline constexpr bool is_pkg_v = is_pkg<T>::value;

//     #define ATTR_IS_FEATURE(a)                                                                                             \
//         template <> struct is_feature<a> : std::true_type {};

//     #define ATTR_IS_NET(a)                                                                                                 \
//         template <> struct is_net<a> : std::true_type {};

//     #define ATTR_IS_PKG(a)                                                                                                 \
//         template <> struct is_pkg<a> : std::true_type {};

//     enum class drill { PLATED, NON_PLATED, VIA };
//     ATTR_NAME(drill)
//     ATTR_IS_FEATURE(drill)

//     enum class primary_side { TOP, BOTTOM };
//     ATTR_NAME(primary_side)

//     enum class pad_usage { TOEPRINT, VIA, G_FIDUCIAL, L_FIDUCIAL, TOOLING_HOLE };
//     ATTR_NAME(pad_usage)
//     ATTR_IS_FEATURE(pad_usage)

//     ATTR_NAME( LINE_STYLE )
//     ATTR_IS_FEATURE( LINE_STYLE )


//     MAKE_FLOAT_ATTR(drc_max_height, 3)
//     ATTR_IS_FEATURE(drc_max_height)

//     MAKE_BOOLEAN_ATTR(smd)
//     ATTR_IS_FEATURE(smd)

//     MAKE_TEXT_ATTR(electrical_class)
//     ATTR_IS_NET(electrical_class)

//     MAKE_TEXT_ATTR(net_type)
//     ATTR_IS_NET(net_type)

//     MAKE_TEXT_ATTR(diff_pair)
//     ATTR_IS_NET(diff_pair)

//     MAKE_TEXT_ATTR(string)
//     ATTR_IS_FEATURE(string)

}

class AttributeProvider
{
public:
    AttributeProvider() = default;
    virtual ~AttributeProvider() = default;

    template <typename Tr, typename Ta> void AddFeatureAttribute(Tr &r, Ta v)
    {
        using Tc = typename Tr::template check_type<Ta>;
        static_assert(Tc::value);

        const auto id = get_or_create_attribute_name(ODB::attribute_name<Ta>::name);

        if constexpr (std::is_enum_v<Ta>)
            r.attributes.emplace_back(id, std::to_string(static_cast<int>(v)));
        else
            r.attributes.emplace_back(id, attr_to_string(v));
    }

protected:
    unsigned int get_or_create_attribute_name(const std::string &name);

    void write_attributes(std::ostream &ost, const std::string &prefix = "") const;
    void WriteAttributesName(std::ostream &ost, const std::string &prefix = "") const;
    void WriteAttributesText(std::ostream &ost, const std::string &prefix = "") const;


private:
    unsigned int get_or_create_attribute_text(const std::string &name);

    static std::string double_to_string(double v, unsigned int n);

    template <typename T, unsigned int n> std::string attr_to_string(ODB::float_attribute<T, n> a)
    {
        return double_to_string(a.value, a.ndigits);
    }

    template <typename T> std::string attr_to_string(ODB::boolean_attribute<T> a)
    {
        return "";
    }

    template <typename T> std::string attr_to_string(ODB::text_attribute<T> a)
    {
        return std::to_string(get_or_create_attribute_text(a.value));
    }


    std::map<std::string, unsigned int> attribute_names;
    std::map<std::string, unsigned int> attribute_texts;
};

class ATTR_RECORD_WRITER
{
public:
    ATTR_RECORD_WRITER() = default;
    virtual ~ATTR_RECORD_WRITER() = default;

    void write_attributes(std::ostream &ost) const;

private:
    std::vector<std::pair<unsigned int, std::string>> attributes;
};


// struct ODB_ATTRLIST
// {
//     ODB_ATTRLIST( const wxString &aUnit ) : m_unit( aUnit ) {}
//     wxString m_unit;
//     std::vector<std::pair<ODB_ATTRIBUTE, wxString>> m_attrlist;
// }




// struct STEP_HDR
// {
//     STEP_HDR( const wxString &aUnit ) : m_unit( aUnit ) {}
//     STEP_HDR() : m_unit( wxS( "mm" ) ) {}
//     ~STEP_HDR() {}

//     static const char* m_general_fields[];
//     static const char* m_stepRepeat_fields[];

//     struct STEP_REPEAT
//     {
//         const wxString m_structuredText_name = wxS( "STEP-REPEAT" );

//     };

//     wxString GetFileName()
//     {
//         return "stephdr";
//     }

//     // bool GenerateFiles( ODB_TREE_WRITER& writer )
//     // {
//     //     auto filewriter = writer.CreateFileProxy( GetFileName() );
//     //     filewriter.GetStream();

//     // }

//     std::vector<STEP_REPEAT> m_repeat;
//     wxString m_unit;
// };


// const char* STEP_HDR::m_general_fields[] =
// {
//     "X_DATUM",
//     "Y_DATUM",
//     "ID",
//     "X_ORIGIN",
//     "Y_ORIGIN",
//     "TOP_ACTIVE",
//     "BOTTOM_ACTIVE",
//     "RIGHT_ACTIVE",
//     "LEFT_ACTIVE",
//     "AFFECTING_BOM",
//     "AFFECTING_BOM_CHANGED"
// };

// const char* STEP_HDR::m_stepRepeat_fields[] =
// {
//     "NAME",
//     "X",
//     "Y",
//     "DX",
//     "DY",
//     "NX",
//     "NY",
//     "ANGLE",
//     "FLIP",
//     "MIRROR"
// };





// struct ODB_COMPONENTS
// {
//     ODB_COMPONENTS() {}
//     wxString m_unit;


//     // struct Toeprint
//     // {
//     // public:
//     //     Toeprint(const EDAData::Pin &pin) : pin_num(pin.index), toeprint_name(pin.name)
//     //     {
//     //     }

//     //     unsigned int pin_num;

//     //     // Placement placement;
//     //     unsigned int net_num = 0;
//     //     unsigned int subnet_num = 0;
//     //     std::string toeprint_name = 0;

//     //     void write(std::ostream &ost) const;
//     // };

//     struct COMPONENT
//     {
//         COMPONENT(unsigned int i, unsigned int r) : m_index(i), m_pkg_ref(r)
//         {
//         }
//         const unsigned int m_index;
//         unsigned int m_pkg_ref;
//         // Placement placement;

//         wxString m_comp_name;
//         wxString m_part_name;

//         std::vector<std::pair<wxString, wxString>> m_prp;   // !< Component Property Record

//         std::list<Toeprint> toeprints;

//         void write(std::ostream &ost) const;
//     };

//     std::list<COMPONENT> components;

// };


#endif // ATTRIBUTE_PROVIDER_H_