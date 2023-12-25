
#include "odb_util.h"
#include <optional>
#include <vector>
#include <map>
#include <wx/string.h>
#include "odb_attribute.h"


 

enum class Polarity
{
    POSITIVE,
    NEGATIVE
};

class ODB_ENTITY_BASE
{
public:
	ODB_ENTITY_BASE();
	virtual ~ODB_ENTITY_BASE();
    virtual bool GenerateFiles(const TreeWriter &writer) = 0;

//     ODB_GENERATE_INTERFACE* GetPluginInterface() const { return m_generator; }
//     ODB_BOARD_MANAGER* GetBoardManager() const { return m_boardManager; }
// private:
//     ODB_GENERATE_INTERFACE* m_generator;
//     ODB_BOARD_MANAGER* m_boardManager;

};




class ODB_MATRIX_ENTITY : public ODB_ENTITY_BASE
{
public:
	ODB_MATRIX_ENTITY(const char* name, const char* description, const char* manufacturer, const char* version, const char* date);
	virtual ~ODB_MATRIX_ENTITY();

    struct MATRIX_LAYER
    {
        enum class CONTEXT
        { 
            BOARD,
            MISC
        };

        enum class TYPE {
            SIGNAL,
            POWER_GROUND,
            DIELECTRIC,
            MIXED,
            SOLDER_MASK,
            SOLDER_PASTE,
            SILK_SCREEN,
            DRILL,
            ROUT,
            DOCUMENT,
            COMPONENT,
            MASK,
            CONDUCTIVE_PASTE,
        };

        struct Span {
            wxString start;
            wxString end;
        };

        enum class Subtype {
            COVERLAY,
            COVERCOAT,
            STIFFENER,
            BEND_AREA,
            FLEX_AREA,
            RIGID_AREA,
            PSA,
            SILVER_MASK,
            CARBON_MASK,
        };

        std::optional<Span> m_span;
        std::optional<Subtype> m_addType;


        Polarity polarity = Polarity::POSITIVE;

        const unsigned int m_rowNumber;
        const wxString m_layerName;
        CONTEXT m_context;
        TYPE m_type;


        MATRIX_LAYER(unsigned int aRow, const wxString &aLayerName) :
         m_rowNumber( aRow ), m_layerName( aLayerName ){};

    };


    Layer& add_layer(const std::string &name);
    void add_step(const std::string &name);

    void write(std::ostream &ost) const;

    

private:

    std::map<wxString, unsigned int> m_matrix_steps;
    std::vector<MATRIX_LAYER> m_matrix_layers;  
    std::vector<Layer> layers;
    unsigned int row = 1;
    unsigned int col = 1;

};

class Features;
class Components;
class EDAData;
class BoardPackage;
class ODB_STEP_ENTITY : public ODB_ENTITY_BASE
{
public:
    ODB_STEP_ENTITY(const wxString &name, const wxString &type, const wxString &desc, const wxString &file, const wxString &line);
    virtual ~ODB_STEP_ENTITY();

    bool AddAttrList();
    bool AddBom();
    bool AddEdaData();
    bool AddLayerEntity();
    bool AddNetList();
    bool AddProfile();
    bool AddStepHeader();
    // bool AddImpedanceFile();
    // bool AddZonesFile();

    virtual bool GenerateFiles(const TreeWriter &writer);

private:

    ODB_ATTRLIST m_attrList;
    // std::map<std::string, Features> layer_features;
    std::vector<ODB_LAYER_ENTITY> m_layerEntity;
    std::optional<Features> m_profile;
    std::optional<ODB_BOMS> m_boms;

    std::optional<ODB_COMPONENTS> m_compTop;
    std::optional<ODB_COMPONENTS> m_compBot;

    EDAData m_edaData;
    STEP_HDR m_stepHdr;
    Components::Component &add_component(const BoardPackage &pkg);





};

class ODB_SYMBOL_ENTITY : public ODB_ENTITY_BASE
{

};


class ODB_FONTS_ENTITY : public ODB_ENTITY_BASE
{

};


class ODB_MISC_ENTITY : public ODB_ENTITY_BASE
{
public:
    ODB_MISC_ENTITY( const std::vector<wxString>& aValue );
    virtual ~ODB_MISC_ENTITY();
    
    virtual bool GenerateFiles(const TreeWriter &writer);
    
private:
    std::map<wxString, wxString> m_info;
    ODB_ATTRLIST m_attrlist;

};

class ODB_LAYER_ENTITY : public ODB_ENTITY_BASE
{
public:
    ODB_LAYER_ENTITY( const std::vector<wxString>& aValue );
    virtual ~ODB_LAYER_ENTITY();
    
    bool AddAttrList();
    
private:
    ODB_ATTRLIST m_attrlist;


};

